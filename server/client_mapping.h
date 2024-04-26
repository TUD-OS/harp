#ifndef __CLIENT_MAPPING_H__
#define __CLIENT_MAPPING_H__

#pragma once

#include "client.h"
#include "util/operating_point.h"
#include "util/operating_point_evaluator.h"
#include "util/platform/cpu_sets.h"

#include <map>

namespace tetris {

class ClientMapping {
public:
  /**
   * \brief Checks if a specific client is present in the map.
   *
   * \param c Pointer to the Client to check.
   * \return true if the client is found, false otherwise.
   */
  bool Contains(Client *c) const { return _map.contains(c); }

  /**
   * \brief Retrieves the OperatingPointAllocation associated with a specific
   * client.
   *
   * \param c Pointer to the Client whose allocation is to be retrieved.
   * \return const reference to the Client's OperatingPointAllocation.
   * \throws std::out_of_range if the client is not found.
   */
  const OperatingPointAllocation &Get(Client *c) const { return _map.at(c); }

  /**
   * \brief Sets or updates the OperatingPointAllocation for a specified client.
   *
   * \param client Pointer to the Client whose allocation is to be set.
   * \param allocation The OperatingPointAllocation to set for the client.
   */
  void Set(Client *client, const OperatingPointAllocation &allocation) {
    _map.emplace(client, allocation);
  }

  /**
   * \brief Removes the OperatingPointAllocation associated with a specific
   * client.
   *
   * \param client Pointer to the Client whose allocation is to be removed.
   */
  void Erase(Client *client) { _map.erase(client); }

  /**
   * \brief Collects and returns all threads used by all clients in the map.
   *
   * \return CPUThreadSet containing all threads used.
   */
  CPUThreadSet GetAllThreads() const;

  /**
   * \brief Determines if there are any overlaps in thread usage among clients.
   *
   * \return true if any threads are shared between clients, false otherwise.
   */
  bool HasOverlaps() const;

  /**
   * \brief Provides a textual representation of the client mapping.
   *
   * \return std::string describing the mapping, including each client and their
   * allocation details.
   */
  std::string ToString() const;

  /**
   * \brief Evaluate the client mapping using the operating point evaluator.
   *
   * \return an std::tuple<int, double>. The first value is a number of
   * successfully scheduled clients. The second value represents the objective
   * value.
   */
  std::tuple<int, double> EvaluateWith(const OperatingPointEvaluator &) const;

private:
  std::map<Client *, OperatingPointAllocation> _map;
};

} // namespace tetris

#endif
