#ifndef __CLIENT_H__
#define __CLIENT_H__

#pragma once

#include "proto/tetris.pb.h"

#include "util/connection.h"
#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/operating_point_table.h"
#include "util/platform/cpu_sets.h"
#include "util/platform/perf.h"
#include "util/protobuf_util.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

using ConnectionPtr = std::shared_ptr<Connection>;

class Manager;

struct PerfData {
  std::chrono::high_resolution_clock::time_point time;
  std::chrono::high_resolution_clock::duration update_interval;

  std::map<std::string, uint64_t> data;
  std::map<std::string, uint64_t> diff;
};

struct CpuTimes {
  uint64_t all;
  std::vector<uint64_t> cores;
};

struct ProcessTimes {
  uint64_t all;
  std::map<pid_t, uint64_t> threads;
};

struct CpuEnergy {
  uint64_t all;
  std::vector<uint64_t> cores;
};

struct ProcessEnergy {
  uint64_t all;
  std::map<pid_t, uint64_t> threads;
};

struct EnergyData {
  std::chrono::high_resolution_clock::time_point time;
  uint64_t total_energy_uj;
  CpuTimes raw_ctimes;

  CpuTimes ctimes;
  CpuEnergy energy;
};

struct ProcessEnergyData {
  std::chrono::high_resolution_clock::time_point time;
  std::chrono::high_resolution_clock::duration update_interval;
  ProcessTimes raw_ctimes;

  std::map<pid_t, int> thread_core_assignment;

  ProcessTimes ctimes;
  ProcessEnergy energy;
};

/**
 * \class Client
 * \brief Represents an application client in the manager.
 *
 * This class manages client's active CPU mapping and dynamically updates
 * mapping regions.
 */
class Client {
public:
  /* The management type of an application */
  enum Type : int {
    /* PASSIV: the default type --> The application will be moved around as a
     * whole with no additional management. Mappings and characteristics
     * are not required for this type of application */
    PASSIV = 0x1,

    /* ACTIVE: the default managed type --> TETRiS will actively manage the
     * application. For this type a simple mapping has to be provided. TETRiS
     * will try to optimize the application depending on the application's
     * criteria and the overall system state */
    ACTIVE = 0x2,
  };

public:
  ConnectionPtr connection;
  std::string exec;
  int pid;
  bool mapping_coarse_grained;

  std::unique_ptr<tetris::OperatingPointTable> op_table;
  std::optional<tetris::OperatingPointAllocation> active_op;
  tetris::CPUCoreSet allowed_cores;
  tetris::OperatingPointTableStage stage_at_selection;

  int type;

private:
  tetris::perf::HandlePtr perf_handle;
  std::vector<PerfData> perf_data;
  std::vector<ProcessEnergyData> energy_data;

  int drop_measurements;
  int new_measurements;

private:
  Manager &_manager;

  bool receive_ops(const tetris::ClientMessage::OperatingPointsInfo &);

  void SelectNextOperatingPointForMeasurement();

public:
  Client(const Client &) = delete;

  Client(const ConnectionPtr &conn, Manager &manager);

  ~Client();

  void HandleRegistrationRequest(const tetris::RegistrationRequest &req);

  std::string push_path() const;

  tetris::CPUThreadSet threads() const {
    if (active_op.has_value()) {
      return active_op->threads();
    }
    return tetris::CPUThreadSet{};
  }

  void activate_type(const Type &t) { type |= t; }

  void deactivate_type(const Type &t) { type &= ~t; }

  void activate_op(const tetris::OperatingPointAllocation &new_op);

  tetris::ServerResponse handle_message(const tetris::ClientMessage &msg);

  void enable_perf(tetris::perf::HandlePtr handle);

  void update_perf_data(std::chrono::high_resolution_clock::time_point tp);

  void update_energy_data(EnergyData &systemwide, uint64_t duration_ms);

  std::optional<tetris::OperatingPoint::Metrics> current_metrics();

  void UpdateCurrentMeasurement();
};

#endif /* __CLIENT_H__ */
