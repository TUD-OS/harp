#ifndef __CLIENT_ALLOCATION_H__
#define __CLIENT_ALLOCATION_H__

#pragma once

#include "client.h"
#include "util/operating_point.h"
#include "util/platform/cpu_sets.h"

#include <map>

namespace tetris {

struct ClientMapping {
  std::map<Client *, OperatingPointAllocation> map;

  /**
   * \brief Checks if a specific client is present in the map.
   *
   * \param c Pointer to the Client to check.
   * \return true if the client is found, false otherwise.
   */
  bool Contains(Client *c) const { return map.contains(c); }

  /**
   * \brief Retrieves the OperatingPointAllocation associated with a specific
   * client.
   *
   * \param c Pointer to the Client whose allocation is to be retrieved.
   * \return const reference to the Client's OperatingPointAllocation.
   * \throws std::out_of_range if the client is not found.
   */
  const OperatingPointAllocation &Get(Client *c) const { return map.at(c); }

  /**
   * \brief Sets or updates the OperatingPointAllocation for a specified client.
   *
   * \param client Pointer to the Client whose allocation is to be set.
   * \param allocation The OperatingPointAllocation to set for the client.
   */
  void Set(Client *client, const OperatingPointAllocation &allocation) {
    map[client] = allocation;
  }

  /**
   * \brief Removes the OperatingPointAllocation associated with a specific
   * client.
   *
   * \param client Pointer to the Client whose allocation is to be removed.
   */
  void Erase(Client *client) { map.erase(client); }

  /**
   * \brief Collects and returns all threads used by all clients in the map.
   *
   * \return CPUThreadSet containing all threads used.
   */
  CPUThreadSet GetAllThreads() const {
    CPUThreadSet allThreads;
    for (const auto &pair : map) {
      allThreads |= pair.second.threads();
    }
    return allThreads;
  }

  /**
   * \brief Determines if there are any overlaps in thread usage among clients.
   *
   * \return true if any threads are shared between clients, false otherwise.
   */
  bool HasOverlaps() const {
    CPUThreadSet totalSet;
    for (const auto &pair : map) {
      CPUThreadSet currentSet = pair.second.threads();
      if (totalSet.OverlapsWith(currentSet)) {
        return true;
      }
      totalSet |= currentSet;
    }
    return false;
  }

  /**
   * \brief Provides a textual representation of the client mapping.
   *
   * \return std::string describing the mapping, including each client and their
   * allocation details.
   */
  std::string ToString() const {
    std::stringstream ss;
    ss << "ClientMapping | " << map.size() << " clients\n";
    for (const auto &[c, opa] : map) {
      ss << "  - '" << c->exec << "' [" << c->pid << "] | ";
      ss << "OP '" << opa.name() << "' utility " << opa.utility() << " power "
         << opa.power() << " | ";
      auto cores = opa.threads();
      ss << string_util::join(cores.GetList(), ",") << "\n";
    }
    return ss.str();
  }
};

} // namespace tetris

#endif
