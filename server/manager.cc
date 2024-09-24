#include "manager.h"

#include <time.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "util/platform/platform.h"
#include "util/string_util.h"
#include "util/util.h"

using namespace tetris;

using ClientPtr = std::unique_ptr<Client>;

/**
 * \brief Handles the incoming message from a client.
 *
 * This function processes incoming messages from a client.
 *
 * If any error occurs during the processing of the message, the function will
 * log an error message and close the connection to the client.
 *
 * \param fd File descriptor of the client's connection.
 * \return A boolean indicating if the connection to the client should be
 * closed. Returns true if an error occurs or if the client is not being
 * managed.
 */
bool Manager::client_message(int fd) try {
  ClientPtr &c = _clients.at(fd);
  ConnectionPtr conn = c->connection;

  bool done = false;
  bool close = false;

  LOGGER->debug("Reading message for client %d\n", fd);

  while (!done) {
    if (c->pid == -1) {
      /* The client is not yet fully registered with the server. Until now we
       * only accept registration requests. */
      RegistrationRequest request{};
      auto res = protobuf_util::Receive(conn->locked(), request);

      if (res == Connection::InState::DONE) {
        /* We are done processing. So return. */
        done = true;
      } else if (res == Connection::InState::AGAIN) {
        LOGGER->debug("Message was incomplete for client %d\n", fd);
        done = true;
      } else if (res == Connection::InState::CLOSED) {
        /* We are done processing and the remote site closed the connection */
        close = true;
        done = true;
      } else {
        c->HandleRegistrationRequest(request);

        // Check whether there is a previously store operating point table
        if (!_optable_storage.empty()) {
          std::string name = c->exec;
          std::replace(name.begin(), name.end(), '/', '_');
          name = name + ".yaml";
          auto full_path = _optable_storage / name;
          if (std::filesystem::exists(full_path)) {
            LOGGER->debug("Loading OperatingPointTable from %s\n",
                          full_path.c_str());
            c->op_table->LoadFromFile(full_path);
          }
        }

        auto stage = c->op_table->Stage();
        std::stringstream ss;
        ss << stage;
        auto stage_str = ss.str();
        LOGGER->info(" -> The client registered! '%s' [%d] (Stage %s)\n", c->exec.c_str(),
                     c->pid, stage_str.c_str());
        _tracelog->RegisterClient(c.get());

        if (_enable_measurement) {
          /* Get the perf handle for this client */
          if (auto handle = _perf_manager->open(c->pid)) {
            c->enable_perf(std::move(handle.value()));
          }
        }

        /* Construct and send the server's registration response */
        RegistrationResponse response{};
        response.set_id(fd);

        if (protobuf_util::Send(conn->locked(), response) !=
            Connection::OutState::DONE)
          LOGGER->error("Failed to acknowledge the registration message\n");
      }
    } else {
      /* The client is fully registered, receive the message and let
       * the client handle it properly */
      ClientMessage msg{};
      auto res = protobuf_util::Receive(conn->locked(), msg);

      if (res == Connection::InState::DONE) {
        /* We are done processing. So return. */
        done = true;
      } else if (res == Connection::InState::AGAIN) {
        LOGGER->debug("Message was incomplete for client %d\n", fd);
        done = true;
      } else if (res == Connection::InState::CLOSED) {
        /* We are done processing and the remote site closed the connection */
        close = true;
        done = true;
      } else {
        LOGGER->debug(" -> Forward message to client [%d]\n", c->pid);
        auto response = c->handle_message(msg);
        if (protobuf_util::Send(conn->locked(), response) !=
            Connection::OutState::DONE)
          LOGGER->error("Failed to acknowledge the client message\n");
      }
    }
  }
  return close;
} catch (std::out_of_range) {
  LOGGER->warning("Received message for unknown client %i\n", fd);
  return true;
} catch (std::runtime_error &e) {
  LOGGER->error("Error working with message for client %i: %s\n", fd, e.what());
  return true;
}

void Manager::print_mappings() {
  auto &allocator = _platform->GetEquivResAllocator();
  std::cout << "Currently active mappings:" << std::endl
            << "==========================" << std::endl;
  for (const auto &[name, client] : _clients) {
    std::cout << "Client '" << client->exec << "' [" << client->pid
              << "] (ID: " << name << ")" << std::endl;
    if (client->active_op.has_value()) {
      std::cout << "-> mapping: " << client->active_op->name() << " ["
                << allocator.GetEquivClassName(*(client->active_op)) << "]"
                << std::endl;
    } else {
      std::cout << "-> mapping: none" << std::endl;
    }
  }
  std::cout << "======= END OF LIST =======" << std::endl;
}

void Manager::RunMapper() {
  _run_mapper_flag = false;
  auto start = std::chrono::high_resolution_clock::now();

  // Run the mapper
  std::vector<Client *> clients;
  for (auto &[cid, c] : _clients) {
    if (c->type == Client::ACTIVE)
      clients.push_back(c.get());
  }

  if (clients.empty()) {
    /* We have no clients to map here -> bail early */
    LOGGER->debug(" --> No active clients to map\n");
    return;
  }

  // Measure separately the call to GenerateClientMapping()
  auto before = std::chrono::high_resolution_clock::now();

  auto client_mapping = _mapper->GenerateClientMapping(clients, _blocked_cores);

  // The scheduling might be too too long, update progresses again
  auto after = std::chrono::high_resolution_clock::now();

  // print
  LOGGER->info("%s\n", client_mapping.ToString().c_str());

  // Collect all core sets of the mapped applications
  // If there remaining free cores and some applications in the initial or
  // exploration phases, then distribute the remaining cores between them
  std::map<Client *, CPUCoreSet> clients_cores;
  CPUCoreSet busy_cores;
  std::vector<Client *> clients_to_extend;
  for (auto &c : clients) {
    if (!client_mapping.Contains(c)) {
      continue;
    }
    auto op = client_mapping.Get(c);
    auto core_set = _platform->ToCPUCoreSet(op.threads());
    clients_cores.emplace(c, core_set);
    busy_cores |= core_set;

    auto stage = c->op_table->Stage();
    if (stage == OperatingPointTableStage::kInitial ||
        stage == OperatingPointTableStage::kExploration) {
      clients_to_extend.push_back(c);
    }
  }

  if (clients_to_extend.size() > 0) {
    CPUCoreSet free_cores = _platform->GetFullCPUCoreSet();
    free_cores ^= busy_cores;

    auto core_list = free_cores.GetList();
    while (core_list.size() > 0) {
      for (auto &c : clients_to_extend) {
        if (core_list.size() == 0)
          break;
        int core = core_list.back();
        clients_cores[c].Set(core);
        core_list.pop_back();
      }
    }
  }

  // updates the mappings
  for (auto &c : clients) {
    if (!client_mapping.Contains(c)) {
      LOGGER->error("No mapping generated for client '%s' [%d]."
                    "Handling of such cases is not yet implemented.",
                    c->exec.c_str(), c->pid);
      throw std::runtime_error("Not yet implemented");
    }
    auto op = client_mapping.Get(c);
    c->allowed_cores = clients_cores.at(c);

    // if client in initial or exploration stage, we should explore the space
    auto stage = c->op_table->Stage();
    if (stage == OperatingPointTableStage::kInitial ||
        stage == OperatingPointTableStage::kExploration) {

      auto op_explor =
          c->op_table->GetOperatingPointToMeasure(c->allowed_cores);
      if (op_explor) {
        auto busy_cores = _platform->GetFullCPUCoreSet();
        busy_cores ^= c->allowed_cores;
        auto &op_allocator = _platform->GetEquivResAllocator();
        auto opa = op_allocator.FindEquivOP(*op_explor, busy_cores);
        if (opa) {
          op = *opa;
        }
      }
    }
    c->activate_op(op);
  }
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> full_dur = end - start;
  auto full_dur_s = full_dur.count();
  std::chrono::duration<double> sched_dur = after - before;
  auto sched_dur_s = sched_dur.count();
  LOGGER->info("Activated the mapper: duration = %lfs [mapping time: %lfs ]\n",
               full_dur_s, sched_dur_s);
}

void Manager::update_perf_data() {
  LOGGER->debug("Updating perf data based on timer update\n");
  auto now = std::chrono::high_resolution_clock::now();

  for (auto &[cid, c] : _clients) {
    c->update_perf_data(now);

    if (c->has_client_utility())
        c->update_client_utility(now);
  }
}

void Manager::update_energy_data() {
  LOGGER->debug("Updating energy data based on timer update\n");
  auto now = std::chrono::high_resolution_clock::now();

  EnergyData energy;
  energy.time = now;
  energy.total_energy_uj= _energy_measure->read();

  /* Ok we got the total energy consumption. Now it is time to attribute it to the individual tasks.
   * In order to achieve this, we follow the approach given by the EnergAt paper. We basically calculate
   * the individual influence of the tasks at the overall system energy by comparing their cputime with
   * the overall system wide cputime */

  /* 1: Read the overall cputime statistics from /proc/stat for all CPUs as well as total*/
  std::ifstream stat("/proc/stat");
  if (!stat.is_open()) {
    LOGGER->warning("Can't open /proc/stat for global cputime statistics");
  }

  /* The first line contains the total CPU time */
  std::string stat_line;
  std::getline(stat, stat_line);

  /* Since the line looks as follows:
   * cpu  <user> <niced> <system> …
   * Hence we are interested in element 2 and 4 (when split at every ' ').
   */
  {
    auto elements = string_util::split(stat_line, ' ');
    energy.raw_ctimes.all = std::stoull(elements[2]) + std::stoull(elements[4]);
  }

  /* Now read the remaining lines to get the CPU time per cores */
  while (true) {
    std::getline(stat, stat_line);
    if (string_util::starts_with(stat_line, "cpu")) {
      /* Since the line looks as follows:
       * cpuN <user> <niced> <system> …
       * Hence we are interested in element 1 and 3 (when split at every ' ').
       */
      auto elements = string_util::split(stat_line, ' ');
      energy.raw_ctimes.cores.push_back(std::stoull(elements[1]) + std::stoull(elements[3]));
    } else {
      break;
    }
  }

  if (_energy_data.size() == 0) {
    /* If we don't have any prior data, the remaining measurements are not meaningful. Bail early in this case. */
    _energy_data.push_back(energy);
    return;
  }

  auto &last = _energy_data.back();
  /* 2a: Calculate how much the cores were active over the last period */
  energy.ctimes.all = util::ctime_to_ms(energy.raw_ctimes.all - last.raw_ctimes.all);
  for (int i = 0; i < energy.raw_ctimes.cores.size(); ++i) {
    energy.ctimes.cores.push_back(util::ctime_to_ms(energy.raw_ctimes.cores[i] - last.raw_ctimes.cores[i]));
  }

  /* 2b: Attribute the measured energy to the individual CPUs respecting their power coefficient */
  auto all_energy_uj = energy.total_energy_uj - last.total_energy_uj;
  auto duration_ms =  std::chrono::duration_cast<std::chrono::milliseconds>(energy.time - last.time).count();

  if (last.total_energy_uj > energy.total_energy_uj) {
    LOGGER->warning("Energy counters overflowed: %llu (LAST) vs %llu (CURRENT)\n",
            last.total_energy_uj, energy.total_energy_uj);
    all_energy_uj = 0;
  }

  auto static_energy_uj = _platform->GetStaticPower() * duration_ms;
  energy.energy.all = all_energy_uj - static_energy_uj;

  if (all_energy_uj < static_energy_uj) {
    LOGGER->warning("Reported energy is lower than estimated static energy consumption: %llu (ALL) vs %llu (STATIC)\n",
            all_energy_uj, static_energy_uj);
    energy.energy.all = 0;
  }

  double time_coefficient_sum = 0.0;
  for (int i = 0; i < energy.ctimes.cores.size(); ++i) {
      bool smt_core = false;

      if (energy.ctimes.cores[i] != 0) {
        for (auto &t : _platform->FindCPUThread(i)->GetSiblings()) {
          if (energy.ctimes.cores[t->GetID()] != 0)
            smt_core = true;
        }
      }
      if (smt_core)
        time_coefficient_sum += energy.ctimes.cores[i] * static_cast<double>(_platform->FindCPUThread(i)->GetPowerCoefficient())/2;
      else
        time_coefficient_sum += energy.ctimes.cores[i] * _platform->FindCPUThread(i)->GetPowerCoefficient();
  }
  for (int i = 0; i < energy.ctimes.cores.size(); ++i) {
      energy.energy.cores.push_back((energy.energy.all * energy.ctimes.cores[i] * _platform->FindCPUThread(i)->GetPowerCoefficient()) / time_coefficient_sum);
  }

  LOGGER->debug("Current energy consumption: Total: %llu uJ --> %llu uJ (%llu mW) since last update\n",
          energy.total_energy_uj, energy.energy.all, energy.energy.all / duration_ms);
  /*
  LOGGER->debug("Per Core values:\n");
  for (int i = 0; i < energy.ctimes.cores.size(); ++i)
      LOGGER->debug("Core %d: %llu uJ  with %llu ms active --> %llu mW\n", i,
              energy.energy.cores[i], energy.ctimes.cores[i],
              energy.ctimes.cores[i] != 0 ? energy.energy.cores[i] / energy.ctimes.cores[i] : 0);
  */
  /* 2: Now do local attribution at the individual clients */
  for (auto& [cid, c]: _clients) {
      c->update_energy_data(energy, duration_ms);
  }

  _energy_data.push_back(energy);
}

void Manager::update_client_metrics() {
  /* First update tell all clients to update their perf data and energy
   * measurements */
  update_perf_data();
  update_energy_data();

  /* Iterate through all clients, get their current metrics and update their
   * op-table accordingly with the metrics */
  for (auto &[_, client] : _clients) {
    client->UpdateCurrentMeasurement();
  }
}
