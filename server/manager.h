#ifndef __MANAGER_H__
#define __MANAGER_H__

#pragma once

#include "client.h"
#include "util/debug_util.h"

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
 *  \property _blocked_cpus A list of blocked CPUs.
 */

class Manager {
 private:
  std::map<int, Client> _clients;
  std::map<std::string, std::vector<Mapping>> _mappings;

  CPUList _blocked_cpus;

  /**
   * \brief Selects the best mapping for a given client.
   */
  Mapping select_best_mapping(Client &c);

  /**
   * \brief Uses the client's preferred mapping if available, otherwise selects
   * the best one.
   */
  Mapping use_preferred_mapping(Client &c,
                                const std::string &preferred_mapping_name) {
    LOGGER->info("Use preferred mapping '%s' for '%s' [%d]\n",
                 preferred_mapping_name.c_str(), c.exec.c_str(), c.pid);

    auto it = std::find_if(
        c.mappings.begin(), c.mappings.end(),
        [&](const auto &m) { return m.name == preferred_mapping_name; });
    if (it != c.mappings.end())
      return *it;
    else {
      LOGGER->info("Couldn't find preferred mapping\n");
      return select_best_mapping(c);
    }
  }

 public:
  explicit Manager()
      : _clients{}, _mappings{} {}

  /**
   * \brief Adds a new client to the client list upon connection.
   */
  void client_connect(int fd, const ConnectionPtr &conn) {
    _clients.emplace(fd, conn);
  }

  /**
   * \brief Removes a client from the client list upon disconnection.
   */
  void client_disconnect(int fd) { _clients.erase(fd); }

  /**
   * \brief Changes the mapping for a specific application.
   */
  void remap(int fd, const std::string &preferred_mapping_name) try {
    Client &c = _clients.at(fd);

    LOGGER->info("Change mapping for client '%s' [%d] to mapping %s\n",
                 c.exec.c_str(), c.pid, preferred_mapping_name.c_str());

    auto it = std::find_if(
        c.mappings.begin(), c.mappings.end(),
        [&](const auto &m) { return m.name == preferred_mapping_name; });
    if (it == c.mappings.end()) {
      LOGGER->info("Unknown mapping %s for client %i\n",
                   preferred_mapping_name.c_str(), fd);
      return;
    } else {
      LOGGER->info("Changing mapping for client '%s' [%d] to mapping %s\n",
                   c.exec.c_str(), c.pid, preferred_mapping_name.c_str());
      c.update_mapping(*it);
    }
  } catch (std::out_of_range &) {
    LOGGER->error("Unknown client %i\n", fd);
  }

  /**
   * \brief Handles the incoming message from a client.
   */
  bool client_message(int fd);

  /**
   * \brief Prints the currently active mappings for all clients.
   */
  void print_mappings() {
    std::cout << "Currently active mappings:" << std::endl
              << "==========================" << std::endl;
    for (const auto &[name, client] : _clients) {
      std::cout << "Client '" << client.exec << "' [" << client.pid
                << "] (ID: " << name << ")" << std::endl;
      std::cout << "-> mapping: " << client.active_mapping.name << " ["
                << client.active_mapping.equivalence_class().name() << "]"
                << std::endl;

      std::cout << "-> threads:" << std::endl;
      for (const auto &t : client.threads)
        std::cout << "--> " << t.name << "(" << t.tid
                  << "): " << string_util::join(t.cpus.cpulist(num_cpus), ",")
                  << std::endl;
    }
    std::cout << "======= END OF LIST =======" << std::endl;
  }

  /**
   * \brief Updates the mappings for all clients.
   */
  void update_mappings();
};

#endif /* __MANAGER_H__ */
