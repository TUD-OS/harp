#ifndef __TRACE_LOGGER_H__
#define __TRACE_LOGGER_H__

#pragma once

#include <chrono>
#include <map>
#include <stdexcept>

#include "util/json.h"
#include "util/operating_point.h"
#include "util/platform/cpu_sets.h"

class Client;

namespace tetris {

class Platform;

class TraceLogger {

  struct ClientDesc {
    std::string name;
    int pid;
  };

  struct MappingSegmentDesc {
    int client_id;
    double begin_ts;
    double end_ts;
    std::string mapping_name;
    double exec_time;
    double energy;
    double begin_progress;
    double end_progress;
    CPUThreadSet cpus;
  };

private:
  void CheckFinished() {
    if (_active_clients.size() > 0) {
      throw std::runtime_error("List of active clients is not empty.");
    }

    if (_active_segments.size() > 0) {
      throw std::runtime_error("List of active segments is not empty.");
    }
  }

  double
  FromStart(const std::chrono::high_resolution_clock::time_point now) const {
    std::chrono::duration<double, std::micro> diff_us = now - _start_time;
    return diff_us.count();
  }

  void ExportClients(nlohmann::json &) const;
  void ExportCPUs(nlohmann::json &) const;
  void ExportSegment(nlohmann::json &, const MappingSegmentDesc &) const;

public:
  explicit TraceLogger(
      const std::chrono::high_resolution_clock::time_point &now)
      : _start_time{now}, _next_client_id{0} {};

  ~TraceLogger() { CheckFinished(); }

  void RegisterPlatform(const Platform &);

  void RegisterClient(Client *);

  void DeregisterClient(Client *);

  void LogClientMappingBegin(
      const std::chrono::high_resolution_clock::time_point &now, Client *,
      const OperatingPointAllocation &op);

  void
  LogClientMappingEnd(const std::chrono::high_resolution_clock::time_point &now,
                      Client *);

  void ExportToFile(const std::string &path);

private:
  std::chrono::high_resolution_clock::time_point _start_time;
  int _next_client_id = 0;

  std::map<int, std::string> _cpu_names; // key - thread id

  // map from Client* to client id (assigned by this class)
  std::map<Client *, int> _active_clients;

  // map from client id to its description
  std::map<int, ClientDesc> _client_desc;

  // open segments per active client id
  std::map<int, MappingSegmentDesc> _active_segments;

  // a list of finished segments
  std::vector<MappingSegmentDesc> _segments;
};

} // namespace tetris

#endif /* __TRACE_LOGGER_H__ */
