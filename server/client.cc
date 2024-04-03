#include "client.h"

#include "proto/tetris.pb.h"
#include "manager.h"

#include <chrono>
#include <sstream>

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>



using namespace tetris;

Client::Client(const ConnectionPtr &conn, Manager &manager)
    : connection{conn}, exec{}, pid{-1}, op_table{},
      active_op{}, type{Type::PASSIV}, perf_fd{-1}, perf_event_ids{}, perf_data{},
      _manager{manager}
{
  // TODO: choose the type of operating point table based on the client info
  op_table = std::make_unique<CustomOperatingPointTable>(_manager.GetPlatform());
}

Client::~Client()
{
  if (pid != -1)
    LOGGER->info("Client removed '%s' [%d]\n", exec.c_str(), pid);
  if (perf_fd != -1) {
    ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
    update_perf_data(std::chrono::high_resolution_clock::now());
    close(perf_fd);
  }
}



std::string Client::push_path() const
{
  std::stringstream path{};
  path << "/tmp/tetris_push_listener_" << pid;

  return path.str();
}

bool Client::receive_ops(
    const ClientMessage::OperatingPointsInfo &ops_info) {
  /* Mark the client active since we now have operating points */
  type = Type::ACTIVE;

  op_table->Clear();

  /* Convert the protobuf mapping representation into our internal format */
  int op_size = ops_info.operating_points_size();
  for (int i = 0; i < ops_info.operating_points_size(); i++) {
    auto cur = ops_info.operating_points(i);
    op_table->AddOperatingPoint(cur);
  }

  LOGGER->info(" -> Received %d operating points from client %d\n", op_size, pid);
  _manager.MarkMapperForRun();

  return true;
}

struct perf_format {
  uint64_t nr;
  struct {
    uint64_t value;
    u_int64_t id;
  } values[];
};

enum PerfEvents : uint64_t {
  Instructions = PERF_COUNT_HW_INSTRUCTIONS,
  CacheMisses = PERF_COUNT_HW_CACHE_MISSES
};

bool Client::start_perf() {
  if (perf_fd != -1)
      /* perf events are already initialized */
      return true;
  if (pid == -1)
      /* Can't start/initialize perf when the client is not properly registered! */
      return false;

  struct perf_event_attr pea;
  memset(&pea, 0, sizeof(pea));

  pea.size = sizeof(pea);
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

  pea.type = PERF_TYPE_HARDWARE;
  pea.config = PERF_COUNT_HW_INSTRUCTIONS;

  /* Start the perf sampling */
  LOGGER->debug("Initializing perf support for client '%s' [%i]\n", exec.c_str(), pid);
  perf_fd = syscall(SYS_perf_event_open, &pea, pid, -1, -1, 0);
  if (perf_fd == -1) {
    /* Something went wrong when opening the performance monitoring! */
    LOGGER->error(" -> Failed to enable perf tracing for client '%s' [%i]\n", exec.c_str(), pid);
    return false;
  }

  uint64_t id;
  if (ioctl(perf_fd, PERF_EVENT_IOC_ID, &id))
    LOGGER->warning(" -> Failed to get id for event\n");
  perf_event_ids[pea.config] = id;
  LOGGER->debug("Registered perf event %llu with id %llu\n", PERF_COUNT_HW_INSTRUCTIONS, id);

  /* Add all the other performance counter that we want to monitor */
  for (auto &event : {PERF_COUNT_HW_CACHE_REFERENCES}) {
    pea.config = event;
    int efd = syscall(SYS_perf_event_open, &pea, pid, -1, perf_fd, 0);
    if (efd == -1)
      LOGGER->warning(" -> Failed to register further perf events\n");
    uint64_t eid;
    if (ioctl(efd, PERF_EVENT_IOC_ID, &eid))
      LOGGER->warning(" -> Failed to get id for event\n");
    perf_event_ids[pea.config] = eid;
    LOGGER->debug("Registered perf event %llu id %llu\n", event, eid);
  }

  /* Start the perf monitoring for all the grouped events */
  if (ioctl(perf_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP))
    LOGGER->warning(" -> Failed to reset perf counters\n");
  if (ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP))
    LOGGER->warning(" -> Failed to enable perf counters\n");

  return true;
}

void Client::update_perf_data(std::chrono::high_resolution_clock::time_point tp) {
  if (perf_fd == -1) {
    LOGGER->debug("Perf not properly initialized for client '%s' [%i]\n", exec.c_str(), pid);
    return;
  }

  char buf[4096];
  struct perf_format *formatted_buf = reinterpret_cast<struct perf_format*>(buf);

  if (read(perf_fd, buf, sizeof(buf)) == -1) {
    LOGGER->warning("Failed to read values from perf.\n");
    return;
  }

  /* Extract the data from perf */
  PerfData cur;

  cur.time = tp;
  for (uint64_t i = 0; i < formatted_buf->nr; ++i) {
      bool event_found = false;
      for (auto &it : perf_event_ids) {
        if (it.second == formatted_buf->values[i].id) {
          cur.data[it.first] = formatted_buf->values[i].value;
          event_found = true;
          break;
        }
      }

      if (!event_found) {
          LOGGER->warning("Received unexpected perf event %llu with %llu\n", formatted_buf->values[i].id, formatted_buf->values[i].value);
      }
  }

  if (perf_data.size() != 0) {
    auto prev = perf_data.back();
    for (auto &it : cur.data) {
        cur.diff[it.first] = it.second - prev.data[it.first];
    }
  } else {
    for (auto &it : cur.data) {
        cur.diff[it.first] = it.second;
    }
  }

  LOGGER->debug("Perf data update for client '%s' [%i]:\n", exec.c_str(), pid);
  for (auto &it : cur.diff) {
      LOGGER->debug(" %llu --> %llu\n", it.first, it.second);
  }

  perf_data.push_back(cur);
}


void Client::activate_op(const OperatingPointAllocation &new_op) {
  LOGGER->info("Change mapping for client '%s' [%i] to %s\n", exec.c_str(), pid,
               new_op.name().c_str());
  active_op = new_op;

  /* Send the new mapping information to the client so that client library knows
   * about the change and can react accordingly. */
  ServerMessage msg{};
  msg.set_feature_id(0);
  msg.set_type(ServerMessage::ACTIVATE_OP);
  auto op_info = msg.mutable_activated_op_info();
  op_info->set_identifier(new_op.name());

  for (const auto& [fc, tc] : new_op.permutation) {
    auto conv = op_info->add_cpu_convs();
    conv->set_cpu_id_from(fc);
    conv->set_cpu_id_to(tc);
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

  LOGGER->info(" * done\n");
}

ServerResponse Client::handle_message(const ClientMessage &msg)
{
  ServerResponse response{};
  response.set_type(ServerResponse::ERROR);

  switch(msg.type()) {
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

