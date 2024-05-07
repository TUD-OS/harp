#ifndef __MANAGER_H__
#define __MANAGER_H__

#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

#include "client.h"
#include "mapper/base.h"
#include "trace_logger.h"

#include "util/debug_util.h"
#include "util/platform/cpu_sets.h"
#include "util/platform/perf.h"
#include "util/platform/platform.h"

/**
 * \class Manager
 * \brief Manages clients and their respective mappings.
 *
 * The Manager class is responsible for maintaining a collection of Clients and
 * their respective mappings. It provides methods for handling client
 * connections, disconnections, and messages, as well as updating clients and
 * CPU mappings. Additionally, it can print currently active mappings and update
 * mappings based on specified parameters. This class also keeps track of
 * blocked CPUs that shouldn't be allocated to any client.
 *
 * \private
 *  \property _clients A map of client identifiers to Client objects.
 *  \property _mappings A map of applications to vectors of possible mappings.
 *  \property _blocked_cores A list of blocked CPUs.
 */

class Manager {
private:
  std::unique_ptr<tetris::Platform> _platform;
  std::unique_ptr<tetris::BaseClientMapper> _mapper;
  std::map<int, std::unique_ptr<Client>> _clients;
  tetris::CPUCoreSet _blocked_cores;

  bool _run_mapper_flag; // Flag to run the mapper

  std::unique_ptr<tetris::TraceLogger> _tracelog;

  std::filesystem::path _optable_storage;

  tetris::perf::PerfManager _perf_manager;
  std::unique_ptr<tetris::Measure> _energy_measure;
  std::vector<EnergyData> _energy_data;

  /**
   * \brief Update the perf data of all connected clients
   **/
  void update_perf_data();

  /**
   * \brief Update energy data and attribute it to the connected clients
   **/
  void update_energy_data();

public:
  explicit Manager(std::unique_ptr<tetris::Platform> platform,
                   std::unique_ptr<tetris::BaseClientMapper> mapper,
                   std::string storage_path = "")
      : _platform{std::move(platform)}, _mapper{std::move(mapper)}, _clients{},
        _run_mapper_flag{false},
        _tracelog{std::make_unique<tetris::TraceLogger>(
            std::chrono::high_resolution_clock::now())},
        _optable_storage{storage_path}, _perf_manager{} {
    _tracelog->RegisterPlatform(*_platform);

    // Check if the directory already exists
    if (!_optable_storage.empty()) {
      if (!std::filesystem::exists(_optable_storage)) {
        try {
          std::filesystem::create_directories(_optable_storage);
        } catch (const std::filesystem::filesystem_error &e) {
          LOGGER->error("Could not create the directory: %s\n",
                        _optable_storage.c_str());
          _optable_storage.clear();
        }
      }
    }

    _energy_measure = std::move(_platform->GetEnergyMeasureMethod());
  }

  const tetris::Platform &GetPlatform() const { return *_platform; }

  tetris::TraceLogger &GetTraceLogger() const { return *_tracelog; }

  void UpdateOperatingPointEvaluator(
      std::shared_ptr<tetris::OperatingPointEvaluator> evaluator) {
    _mapper->SetOperatingPointEvaluator(std::move(evaluator));
  }

  /**
   * \brief Adds a new client to the client list upon connection.
   */
  void client_connect(int fd, const ConnectionPtr &conn) {
    auto c = std::make_unique<Client>(conn, *this);
    _clients.emplace(fd, std::move(c));
    MarkMapperForRun();
  }

  /**
   * \brief Removes a client from the client list upon disconnection.
   */
  void client_disconnect(int fd) {
    auto now = std::chrono::high_resolution_clock::now();
    auto &c = *_clients.at(fd);

    if (c.active_op.has_value()) {
      _tracelog->LogClientMappingEnd(now, &c);
    }
    _tracelog->DeregisterClient(&c);

    if (!_optable_storage.empty()) {
      std::string name = c.exec;
      std::replace(name.begin(), name.end(), '/', '_');
      name = name + ".yaml";
      auto full_path = _optable_storage / name;
      c.op_table->StoreToFile(full_path);
    }

    _clients.erase(fd);
    MarkMapperForRun();
  }

  /**
   * Run the client mapper.
   *
   * It runs the mapper and gets the operating point allocation for each client.
   * Then, it assigns the found operating point allocation and triggers the
   * message sending to the client.
   */
  void RunMapper();

  /**
   * \brief Handles the incoming message from a client.
   */
  bool client_message(int fd);

  /**
   * \brief Prints the currently active mappings for all clients.
   */
  void print_mappings();

  /**
   * \brief Note that we have to generate a new client mapping due to changed
   * client states
   **/
  void MarkMapperForRun() {
    LOGGER->info(" -> Marked to generate a new client mapping.\n");
    _run_mapper_flag = true;
  }

  bool IsMapperMarkedForRun() const { return _run_mapper_flag; }

  /**
   * \brief Update the metrics of the currently running OPs of all enabled
   * clients.
   */
  void update_client_metrics();
};

#endif /* __MANAGER_H__ */
