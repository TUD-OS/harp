#include "trace_logger.h"

#include <fstream>
#include <string>

#include "client.h"

#include "util/platform/platform.h"

namespace tetris {

void TraceLogger::RegisterPlatform(const Platform &platform) {
  assert(_cpu_names.size() == 0);
  for (const auto &[cid, cpu_thread] : platform.GetCPUThreads()) {
    _cpu_names[cid] = cpu_thread->GetName();
  }
}

void TraceLogger::RegisterClient(Client *client) {
  assert(_active_clients.count(client) == 0);
  _active_clients[client] = _next_client_id;
  _client_desc[_next_client_id] = ClientDesc{client->exec, client->pid};
  _next_client_id++;
}

void TraceLogger::DeregisterClient(Client *client) {
  assert(_active_clients.count(client) == 1);
  int id = _active_clients.at(client);
  assert(_active_segments.count(id) == 0);
  _active_clients.erase(client);
}

void TraceLogger::LogClientMappingBegin(
    const std::chrono::high_resolution_clock::time_point &now, Client *client,
    const OperatingPointAllocation &op) {
  auto client_id = _active_clients.at(client);

  assert(_active_segments.count(client_id) == 0);

  _active_segments.try_emplace(
      client_id, client_id, FromStart(now), -1, op.base.name,
      op.characteristic("execution_time"), op.characteristic("energy"),
      client->progress, -1, op.GetThreadSet());
}

void TraceLogger::LogClientMappingEnd(
    const std::chrono::high_resolution_clock::time_point &now, Client *client) {
  auto client_id = _active_clients.at(client);

  assert(_active_segments.count(client_id) == 1);

  auto segment = _active_segments.at(client_id);
  segment.end_ts = FromStart(now);
  segment.end_progress = client->progress;
  _segments.push_back(segment);

  _active_segments.erase(client_id);
}

//
// Export to Json
//

void TraceLogger::ExportClients(nlohmann::json &trace) const {
  trace.push_back({{"name", "process_name"},
                   {"ph", "M"},
                   {"pid", 0},
                   {"args", {{"name", "Clients"}}}});

  for (const auto &[cid, desc] : _client_desc) {
    auto client_name = desc.name + " [" + std::to_string(desc.pid) + "]";
    trace.push_back({{"name", "thread_name"},
                     {"ph", "M"},
                     {"pid", 0},
                     {"tid", cid},
                     {"args", {{"name", client_name}}}});
  }
}

void TraceLogger::ExportCPUs(nlohmann::json &trace) const {
  trace.push_back({{"name", "process_name"},
                   {"ph", "M"},
                   {"pid", 1},
                   {"args", {{"name", "CPUs"}}}});

  for (const auto &[cid, cpu_name] : _cpu_names) {
    trace.push_back({{"name", "thread_name"},
                     {"ph", "M"},
                     {"pid", 1},
                     {"tid", cid},
                     {"args", {{"name", cpu_name}}}});

    // Add a dummy placeholder to ensure the core visibility
    trace.push_back({{"name", "dummy"},
                     {"ph", "X"},
                     {"pid", 1},
                     {"tid", cid},
                     {"ts", 0},
                     {"dur", 1},
                     {"args", {}}});
  }
}

void TraceLogger::ExportSegment(nlohmann::json &trace,
                                const MappingSegmentDesc &segment) const {
  auto client_id = segment.client_id;
  auto &client_desc = _client_desc.at(client_id);
  auto client_name = client_desc.name;
  auto full_client_name =
      client_name + " [" + std::to_string(client_desc.pid) + "]";
  auto mapping_name = client_name + "." + segment.mapping_name;

  // Show in clients frame
  trace.push_back({{"name", mapping_name},
                   {"ph", "B"},
                   {"pid", 0},
                   {"tid", client_id},
                   {"ts", segment.begin_ts},
                   {"args",
                    {{"begin_progress", segment.begin_progress},
                     {"end_progress", segment.end_progress},
                     {"mapping_exec_time", segment.exec_time},
                     {"mapping_energy", segment.energy}}}});
  trace.push_back({{"name", mapping_name},
                   {"ph", "E"},
                   {"pid", 0},
                   {"tid", client_id},
                   {"ts", segment.end_ts},
                   {"args",
                    {{"begin_progress", segment.begin_progress},
                     {"end_progress", segment.end_progress},
                     {"mapping_exec_time", segment.exec_time},
                     {"mapping_energy", segment.energy}}}});

  // Show in CPUs frame
  for (const auto &cpu_id : segment.cpus.GetList()) {
    trace.push_back({{"name", full_client_name},
                     {"ph", "B"},
                     {"pid", 1},
                     {"tid", cpu_id},
                     {"ts", segment.begin_ts},
                     {"args", {}}});
    trace.push_back({{"name", full_client_name},
                     {"ph", "E"},
                     {"pid", 1},
                     {"tid", cpu_id},
                     {"ts", segment.end_ts},
                     {"args", {}}});
  }
}

void TraceLogger::ExportToFile(const std::string &path) {
  CheckFinished();

  nlohmann::json trace;

  ExportClients(trace);
  ExportCPUs(trace);

  for (const auto &s : _segments) {
    ExportSegment(trace, s);
  }

  std::ofstream file(path);

  if (!file.is_open()) {
    std::cerr << "Failed to open the file for writing: " << path << std::endl;
    return;
  }

  file << trace.dump(4); // 4 spaces as indentation
  file.close();
}

} // namespace tetris
