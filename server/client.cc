#include "client.h"

#include "manager.h"
#include "proto/tetris.pb.h"
#include "util/operating_point.h"
#include "util/string_util.h"
#include "util/util.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

using namespace tetris;

Client::Client(const ConnectionPtr &conn, Manager &manager)
    : connection{conn}, exec{}, pid{-1}, mapping_coarse_grained{false},
      op_table{}, active_op{}, type{Type::PASSIV}, perf_handle{}, perf_data{},
      new_measurements{0}, _manager{manager} {}

Client::~Client() {
  if (pid != -1)
    LOGGER->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
  if (perf_handle) {
    update_perf_data(std::chrono::high_resolution_clock::now());

    /* Calculate the overall IPS */
    auto start = perf_data.front();
    auto last = perf_data.back();

    auto total_ins = last.data["Instructions"] - start.data["Instructions"];
    auto total_s =
        std::chrono::duration<double>(last.time - start.time).count();
    LOGGER->info(" --> Total: %llu\tTime: %lf s\tIPS: %lf\n", total_ins,
                 total_s, total_s != 0 ? total_ins / total_s : 0);
  }
}

void Client::HandleRegistrationRequest(const tetris::RegistrationRequest &req) {
  pid = req.pid();
  exec = req.exec();

  const auto &mapping_type = req.mapping_type();
  if (mapping_type == RegistrationRequest::COARSE_GRAINED) {
    mapping_coarse_grained = true;
    bool measurement = _manager.EnabledMeasurement();
    op_table = std::make_unique<ThreadSetOperatingPointTable>(
        _manager.GetPlatform(), nullptr, measurement);
  } else if (mapping_type == RegistrationRequest::FINE_GRAINED) {
    mapping_coarse_grained = false;
    op_table =
        std::make_unique<CustomOperatingPointTable>(_manager.GetPlatform());
  }
}

std::string Client::push_path() const {
  std::stringstream path{};
  path << "/tmp/tetris_push_listener_" << pid;

  return path.str();
}

bool Client::receive_ops(const ClientMessage::OperatingPointsInfo &ops_info) {
  /* Mark the client active since we now have operating points */
  type = Type::ACTIVE;

  /* Convert the protobuf mapping representation into our internal format */
  int op_size = ops_info.operating_points_size();
  for (int i = 0; i < ops_info.operating_points_size(); i++) {
    auto cur = ops_info.operating_points(i);
    op_table->AddOperatingPoint(cur);
  }

  auto stage = op_table->Stage();
  std::stringstream ss;
  ss << stage;
  auto stage_str = ss.str();
  LOGGER->info(" -> Received %d operating points from client %d (Stage %s)\n", op_size,
               pid, stage_str.c_str());
  _manager.MarkMapperForRun();

  return true;
}

void Client::enable_perf(tetris::perf::HandlePtr handle) {
  perf_handle = std::move(handle);
}

void Client::update_perf_data(
    std::chrono::high_resolution_clock::time_point tp) {
  if (!perf_handle) {
    LOGGER->debug("Perf not properly initialized for client '%s' [%i]\n",
                  exec.c_str(), pid);
    return;
  }

  PerfData cur;
  cur.time = tp;
  cur.data = perf_handle->read();

  if (perf_data.size() != 0) {
    auto prev = perf_data.back();
    cur.update_interval = cur.time - prev.time;

    for (auto &[name, val] : cur.data) {
        cur.diff[name] = val - prev.data[name];
    }
  } else {
    for (auto &[name, val] : cur.data) {
        cur.diff[name] = val;
    }
  }

  LOGGER->debug("Perf data update for client '%s' [%i]:\n", exec.c_str(), pid);
  for (auto &[name, val] : cur.diff) {
      LOGGER->debug(" %s --> %llu\n", name.c_str(), val);
  }

  perf_data.push_back(cur);
}

void Client::update_energy_data(EnergyData &sw_energy, uint64_t duration_ms) {
  ProcessEnergyData proc_energy;
  proc_energy.time = sw_energy.time;

  /* In order to partially account the energy to the current client we need to
   * get utime and stime as well as core assignments for the threads of the
   * corresponding process */

  /* 1. Get the total utime and stime of the process */
  {
    std::stringstream path;
    path << "/proc/" << pid << "/stat";
    std::ifstream stat(path.str());
    if (!stat.is_open()) {
      LOGGER->warning("Can't open /proc/%d/stat for per process cputime statistics\n", pid);
      return;
    }

    /* The first line in this file contains the interesting information for us */
    std::string stat_line;
    std::getline(stat, stat_line);
    stat.close();

    /* The format of this file is defined in the Linux kernel documentation. Since
     * we are interested in user-time and system-time, we need the elements 13 and
     * 14.
     */
    auto elements = string_util::split(stat_line, ' ');
    proc_energy.raw_ctimes.all =  std::stoull(elements[13]) + std::stoull(elements[14]);
  }
  /* 2. Get the per thread utime and stime as well as core assignments */
  {
    std::stringstream path;
    path << "/proc/" << pid << "/task/";
    for (const auto & entry : std::filesystem::directory_iterator(path.str())) {
      std::ifstream thread_stat(entry.path() / "stat");
      if (!thread_stat.is_open()) {
        LOGGER->warning("Failed to open stat file in %s\n", entry.path().c_str());
        continue;
      }

      std::string stat_line;
      std::getline(thread_stat, stat_line);
      thread_stat.close();

      /* The format of the stat file is the same as before, hence again we are
       * interested in element 13 and 14. The last core assingment of the thread
       * is saved in element 38 of this list */
      auto elements = string_util::split(stat_line, ' ');
      auto tid = std::stod(elements[0]);
      proc_energy.raw_ctimes.threads[tid] = std::stoull(elements[13]) + std::stoull(elements[14]);
      proc_energy.thread_core_assignment[tid] = std::stod(elements[38]);
    }
  }

  /* 3. Calculate the amount of time the client executed since the last update.
   *    If there is no previous update, assume that all the time was executed 
   *    in this period. */
  if (energy_data.size() > 0) {
    auto last = energy_data.back();
    proc_energy.update_interval = proc_energy.time - last.time;

    proc_energy.ctimes.all = util::ctime_to_ms(proc_energy.raw_ctimes.all - last.raw_ctimes.all);
    for (auto &[tid, raw_time] : proc_energy.raw_ctimes.threads) {
      if (last.raw_ctimes.threads.contains(tid))
        proc_energy.ctimes.threads[tid] = util::ctime_to_ms(raw_time - last.raw_ctimes.threads[tid]);
      else
        proc_energy.ctimes.threads[tid] = util::ctime_to_ms(proc_energy.raw_ctimes.threads[tid]);
    }
  } else {
    proc_energy.ctimes.all = util::ctime_to_ms(proc_energy.raw_ctimes.all);
    for (auto &[tid, raw_time] : proc_energy.raw_ctimes.threads)
      proc_energy.ctimes.threads[tid] = util::ctime_to_ms(raw_time);
  }

  /* 4. Now attribute the energy proportional to the time the thread executed on
   * the individual cores */
  uint64_t sum_threads = 0;
  for (auto &[tid, core] : proc_energy.thread_core_assignment) {
    auto thread_energy = sw_energy.ctimes.cores[core] != 0 ? (sw_energy.energy.cores[core] * proc_energy.ctimes.threads[tid]) / sw_energy.ctimes.cores[core] : 0;
    sum_threads += thread_energy;
    proc_energy.energy.threads[tid] = thread_energy;
  }
  proc_energy.energy.all = sum_threads;

  LOGGER->debug("Client %s [%d] has the following energy data: %llu uJ with %llu ms active --> %llu mW\n",
          exec.c_str(), pid, proc_energy.energy.all, proc_energy.ctimes.all, proc_energy.energy.all / duration_ms);
  for (auto &[tid, thread_energy] : proc_energy.energy.threads) {
    LOGGER->debug(" => Thread %d (Core %d): %llu uJ with %llu ms active --> %llu mW\n",
            tid, proc_energy.thread_core_assignment[tid], thread_energy, proc_energy.ctimes.threads[tid],
            proc_energy.ctimes.threads[tid] != 0 ? thread_energy / proc_energy.ctimes.threads[tid] : 0);
  }

  energy_data.push_back(proc_energy);
}

std::optional<tetris::OperatingPoint::Metrics> Client::current_metrics() {
  if (energy_data.size() < 2 || perf_data.size() < 2)
    return std::nullopt;

  tetris::OperatingPoint::Metrics res;

  /* Calculate power in mW */
  auto energy_measurement = energy_data.back();
  res.power = energy_measurement.energy.all / std::chrono::duration<double, std::milli>(energy_measurement.update_interval).count();

  /* Calculate utility in instructions per second (IPS) */
  auto perf_measurement = perf_data.back();
  res.utility = perf_measurement.diff["Instructions"] / std::chrono::duration<double>(perf_measurement.update_interval).count();

  return res;
}

void Client::SelectNextOperatingPointForMeasurement() {
  auto op = op_table->GetOperatingPointToMeasure(allowed_cores);
  if (op) {
    auto busy_cores = _manager.GetPlatform().GetFullCPUCoreSet();
    busy_cores ^= allowed_cores;
    auto &op_allocator = _manager.GetPlatform().GetEquivResAllocator();
    auto opt_opa = op_allocator.FindEquivOP(*op, busy_cores);

    activate_op(*opt_opa);
  } else {
    _manager.MarkMapperForRun();
  }
}

void Client::UpdateCurrentMeasurement() {
  if (!op_table->EnabledMeasurement()) {
    return;
  }

  const auto optable_params =
      _manager.GetPlatform().GetOperatingPointTableParams();

  auto metrics = current_metrics();
  if (active_op && metrics) {
    // FIXME:Currently, the energy measurement may give gigantic values,
    // possible due to overflow
    if (metrics->power <= 1e11) {
      if (drop_measurements > 0) {
        drop_measurements--;
      } else {
        op_table->AddOperatingPointMeasurement(active_op->base.config,
                                               *metrics);
        new_measurements++;
      }
    }
  }

  // If the stage was changed mark to reschedule
  if (((stage_at_selection == OperatingPointTableStage::kInitial) &&
       (new_measurements >= optable_params.at("initial_measurements"))) ||
      ((stage_at_selection == OperatingPointTableStage::kExploration) &&
       (new_measurements >= optable_params.at("exploration_measurements"))) ||
      ((stage_at_selection == OperatingPointTableStage::kMature) &&
       (new_measurements >= optable_params.at("mature_measurements")))) {
    auto stage = op_table->Stage();

    op_table->Dump();

    if (stage != stage_at_selection ||
        stage == OperatingPointTableStage::kMature) {
      _manager.MarkMapperForRun();
    } else {
      if (stage == OperatingPointTableStage::kInitial ||
          stage == OperatingPointTableStage::kExploration) {
        SelectNextOperatingPointForMeasurement();
      }
    }
  }
}

void Client::activate_op(const OperatingPointAllocation &new_op) {
  // Reset new measurements counter
  new_measurements = 0;
  stage_at_selection = op_table->Stage();

  // Check whether the new operating point allocation is different from the
  // current one
  if (active_op && active_op->name() == new_op.name() &&
      active_op->threads() == new_op.threads()) {
    LOGGER->debug("The new operating point is the same as the current one\n");
    return;
  }

  const auto optable_params =
      _manager.GetPlatform().GetOperatingPointTableParams();

  drop_measurements = optable_params.at("drop_measurements");

  auto op_cores = _manager.GetPlatform().ToCPUCoreSet(new_op.threads());
  LOGGER->info("Change mapping for client '%s' [%i] to %s (threads: %s) "
               "[allowed_cores: %s]\n",
               exec.c_str(), pid, new_op.name().c_str(),
               new_op.threads().GetString().c_str(),
               allowed_cores.GetString().c_str());
  if (!op_cores.IsSubsetOf(allowed_cores)) {
    LOGGER->error("The new allocation core set %s is not a subset of the "
                  "allowed core set %s\n",
                  op_cores.GetString().c_str(),
                  allowed_cores.GetString().c_str());
    throw std::runtime_error(
        "New core allocation is not within the allowed core set");
  }

  auto now = std::chrono::high_resolution_clock::now();

  // Close the segment in the trace logger
  if (active_op) {
    _manager.GetTraceLogger().LogClientMappingEnd(now, this);
  }

  active_op = new_op;

  /* Send the new mapping information to the client so that client library knows
   * about the change and can react accordingly. */
  ServerMessage msg{};
  msg.set_feature_id(0);

  if (mapping_coarse_grained) {
    msg.set_type(ServerMessage::ACTIVATE_CPUS);
    auto cpus_info = msg.mutable_activated_cpus();
    auto threads = new_op.threads();

    for (const auto &t : threads.GetList()) {
      cpus_info->add_cpu_ids(t);
    }
  } else {
    msg.set_type(ServerMessage::ACTIVATE_CUSTOM_OP);
    auto op_info = msg.mutable_activated_op_info();
    op_info->set_identifier(new_op.name());

    for (const auto &[fc, tc] : new_op.permutation) {
      auto conv = op_info->add_cpu_convs();
      conv->set_cpu_id_from(fc);
      conv->set_cpu_id_to(tc);
    }
  }

  LOGGER->info(" -> sending mapping info to client\n");

  try {
    Connection conn{push_path()};
    ClientResponse response;

    protobuf_util::Send(conn.locked(), msg);
    protobuf_util::Receive(conn.locked(), response);

    if (response.type() != ClientResponse::ACKNOWLEDGE) {
      LOGGER->warning(" -! Client didn't acknowledge the message!\n");
    }
  } catch (std::exception &e) {
    LOGGER->error(" -! Sending failed with an error: %s\n", e.what());
  }

  // Open the segment in the trace logger
  _manager.GetTraceLogger().LogClientMappingBegin(now, this, *active_op);

  LOGGER->info(" * done\n");
}

ServerResponse Client::handle_message(const ClientMessage &msg) {
  ServerResponse response{};
  response.set_type(ServerResponse::ERROR);

  switch (msg.type()) {
  case ClientMessage::OPERATING_POINTS:
    LOGGER->debug(" -> Received operating point message from client\n");
    /* Parse the mapping information from the client */
    if (msg.has_ops_info() && receive_ops(msg.ops_info())) {
      response.set_type(ServerResponse::ACKNOWLEDGE);
    }
    break;
  case ClientMessage::OPTIMIZATION_TARGET:
    break;
  case ClientMessage::FEATURE_SUBSCRIBE:
    break;
  }

  return response;
}
