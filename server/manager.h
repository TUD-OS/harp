#ifndef __MANAGER_H__
#define __MANAGER_H__

#include <memory>
#pragma once

#include "client.h"
#include "sched/base.h"
#include "util/debug_util.h"
#include "util/operating_point.h"
#include "util/platform/cpu_sets.h"
#include "util/platform/platform.h"

/***
 * Failure handling for no mapping found
 ***/

class NoMappingError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

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
  std::unique_ptr<tetris::BaseScheduler> _scheduler;
  std::map<int, std::unique_ptr<Client>> _clients;
  tetris::CPUCoreSet _blocked_cores;

  bool _needs_reschedule;

  /**
   * \brief Selects the best mapping for a given client.
   */
  tetris::OperatingPointAllocation select_best_mapping(Client &c);

  /**
   * \brief Uses the client's preferred mapping if available, otherwise selects
   * the best one.
   */
  tetris::OperatingPointAllocation use_preferred_mapping(Client &,
                                                         const std::string &);

  void update_client_progresses(std::chrono::high_resolution_clock::time_point new_tp) {
    for (auto& [cid, c]: _clients) {
      c->update_progress(new_tp);
    }
  }

public:
  explicit Manager(std::unique_ptr<tetris::Platform> platform,
                   std::unique_ptr<tetris::BaseScheduler> scheduler)
      : _platform{std::move(platform)},
        _scheduler{std::move(scheduler)}, _clients{}, _needs_reschedule{false}
  {}

  const tetris::Platform &GetPlatform() const { return *_platform.get(); }

  /**
   * \brief Adds a new client to the client list upon connection.
   */
  void client_connect(int fd, const ConnectionPtr &conn) {
    auto c = std::make_unique<Client>(conn, this);
    _clients.emplace(fd, std::move(c));
    _needs_reschedule = true;
  }

  /**
   * \brief Removes a client from the client list upon disconnection.
   */
  void client_disconnect(int fd) {
    _clients.erase(fd);
    _needs_reschedule = true;
  }

  /**
   * \brief Changes the mapping for a specific application.
   */
  void remap(int fd, const std::string &op_name) try {
    Client &c = _clients.at(fd);

    LOGGER->info("Change mapping for client '%s' [%d] to mapping %s\n",
                 c.exec.c_str(), c.pid, op_name.c_str());

    auto it = std::find_if(c.ops.begin(), c.ops.end(),
                           [&](const auto &op) { return op.name == op_name; });
    if (it == c.ops.end()) {
      LOGGER->info("Unknown mapping %s for client %i\n", op_name.c_str(), fd);
      return;
    } else {
      LOGGER->info("Changing mapping for client '%s' [%d] to mapping %s\n",
                   c.exec.c_str(), c.pid, op_name.c_str());
      c.activate_op(tetris::OperatingPointAllocation{*it, {}});
    }
  } catch (std::out_of_range &) {
    LOGGER->error("Unknown client %i\n", fd);
  }

  /**
   * Run the scheduler.
   *
   * First, it updates the current progress for all clients. Then, it runs the
   * scheduler and gets the operating point allocation for each client.
   * Third, it assigns the found operating point allocation and triggers the
   *  message sending to the client.
   */
  void run_scheduler();

  /**
   * \brief Handles the incoming message from a client.
   */
  bool client_message(int fd);

  /**
   * \brief Prints the currently active mappings for all clients.
   */
  void print_mappings();

  /**
   * \brief Updates the mappings for all clients.
   */
  void update_mappings();

  /**
   * \brief Note that we have to generate a new schedule due to changed client states
   **/
  void reschedule() {
      LOGGER->info(" -> Marked for reschedule!\n");
      _needs_reschedule = true;
  }

  bool needs_reschedule() const {
      return _needs_reschedule;
  }
};

#endif /* __MANAGER_H__ */
