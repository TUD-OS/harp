#ifndef __MANAGER_H__
#define __MANAGER_H__

#pragma once

#include <chrono>
#include <memory>

#include "client.h"
#include "sched/base.h"
#include "trace_logger.h"

#include "util/debug_util.h"
#include "util/platform/cpu_sets.h"
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

  void update_client_progresses(
      std::chrono::high_resolution_clock::time_point new_tp) {
    for (auto &[cid, c] : _clients) {
      c->update_progress(new_tp, true);
    }
  }

public:
  explicit Manager(std::unique_ptr<tetris::Platform> platform,
                   std::unique_ptr<tetris::BaseClientMapper> mapper)
      : _platform{std::move(platform)}, _mapper{std::move(mapper)}, _clients{},
        _run_mapper_flag{false},
        _tracelog{std::make_unique<tetris::TraceLogger>(
            std::chrono::high_resolution_clock::now())} {
    _tracelog->RegisterPlatform(*_platform);
  }

  const tetris::Platform &GetPlatform() const { return *_platform; }

  const tetris::TraceLogger &GetTraceLogger() const { return *_tracelog; }

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
      c.update_progress(now);
      _tracelog->LogClientMappingEnd(now, &c);
    }
    _tracelog->DeregisterClient(&c);

    _clients.erase(fd);
    MarkMapperForRun();
  }

  /**
   * Run the client mapper.
   *
   * First, it updates the current progress for all clients. Then, it runs the
   * mapper and gets the operating point allocation for each client.
   * Third, it assigns the found operating point allocation and triggers the
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
};

#endif /* __MANAGER_H__ */
