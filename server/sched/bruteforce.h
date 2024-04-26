#ifndef __SCHED_BRUTEFORCE_H__
#define __SCHED_BRUTEFORCE_H__

#pragma once

#include "base.h"

#include <tuple>

namespace tetris {

/**
 * Bruteforce mapper.
 */
class BruteforceMapper : public BaseClientMapper {
private:
  using MappingListValue = std::tuple<int, double>;

  /**
   * Updates the best solution found so far.
   */
  void UpdateBestSolution(int, double);

  /**
   * Recursively iterates over all clients and tries all possible mappings.
   *
   * \param n The index of the current client.
   * \param busy_cores The set of busy CPUs.
   */
  void IterateClient(int n, const std::map<std::string, int> &used_cores,
                     int cur_apps, double cur_value);

public:
  explicit BruteforceMapper(const Platform &platform)
      : BaseClientMapper(platform) {}

  // Bring all overloads of GenerateClientMapping()
  using BaseClientMapper::GenerateClientMapping;

  /**
   * Selects client mappings considering a list of blocked CPUs.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param blocked_cpus The list of blocked CPUs.
   * \return The selected mappings.
   */
  ClientMapping GenerateClientMapping(std::vector<Client *> clients,
                                      CPUCoreSet blocked_cores) override;

private:
  // temporary fields (initialized at each invokation of the mapper)
  std::vector<Client *> _clients;
  CPUCoreSet _blocked;

  std::vector<std::vector<OperatingPoint>> _client_ops; // Pareto front of ops

  MappingList _best_ops;
  MappingListValue _best_value;
  MappingList _cur_ops;
};

} // namespace tetris

#endif /* __SCHED_BRUTEFORCE_H__ */
