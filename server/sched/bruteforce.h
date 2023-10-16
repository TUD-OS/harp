#ifndef __SCHED_BRUTEFORCE_H__
#define __SCHED_BRUTEFORCE_H__

#pragma once

#include "base.h"
#include "util/platform/platform.h"

#include <tuple>

namespace tetris {

/**
 * Bruteforce mapper.
 *
 * This scheduler generates a single-segment schedule (that's why called
 * "mapper") using a bruteforce algorithm.
 */
class BruteforceMapper : public BaseScheduler {
private:
  using MappingList = std::vector<std::optional<OperatingPointAllocation>>;
  using MappingListValue = std::tuple<int, double>;

  /**
   * Create a schedule object with the current mapping list
   */
  Schedule ToSchedule(const MappingList &ops);

  /**
   * Evaluates the quality of a given list of mappings.
   *
   * \param ops The list of mappings to be evaluated.
   * \return The number of applications and the total energy consumption.
   */
  MappingListValue Evaluate(const MappingList &ops);

  /**
   * Updates the best solution found so far.
   */
  void UpdateBestSolution();

  /**
   * Recursively iterates over all clients and tries all possible mappings.
   *
   * \param n The index of the current client.
   * \param busy_cpus The set of busy CPUs.
   */
  void IterateClient(int n, CPUThreadSet busy_cpus);

public:
  BruteforceMapper(const Platform &platform) : _platform{platform} {}

  // Bring all overloads of GenerateSchedule()
  using BaseScheduler::GenerateSchedule;

  /**
   * Selects client mappings considering a list of blocked CPUs.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \param blocked_cpus The list of blocked CPUs.
   * \return The selected mappings.
   */
  Schedule GenerateSchedule(std::vector<Client *> clients, double start_time,
                            CPUThreadSet blocked_cpus) override;

private:
  const Platform &_platform;
  // temporary fields (initialized at each invokation of GenerateSchedule())
  std::vector<Client *> _clients;
  double _start_time;
  CPUThreadSet _blocked;
  MappingList _best_ops;
  MappingListValue _best_value;
  MappingList _cur_ops;
};

} // namespace tetris

#endif /* __SCHED_BRUTEFORCE_H__ */
