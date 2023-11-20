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
  using MappingList = std::vector<OperatingPoint *>;
  using MappingListValue = std::tuple<int, double>;

  /**
   * Create a schedule object with the current mapping list
   */
  std::unique_ptr<Schedule> ToSchedule(const MappingList &ops,
                                       CPUCoreSet busy_cores);

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
  explicit BruteforceMapper(const Platform &platform,
                            std::unique_ptr<OptimizationObjective> objective)
      : _platform{platform},
        _platform_cores_count{platform.GetCoreCountPerType()},
        _objective{std::move(objective)} {}

  void SetObjective(std::unique_ptr<OptimizationObjective> objective) override {
    _objective = std::move(objective);
  }

  OptimizationObjective *GetObjective() override { return _objective.get(); }

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
  std::unique_ptr<Schedule> GenerateSchedule(std::vector<Client *> clients,
                                             double start_time,
                                             CPUCoreSet blocked_cores) override;

private:
  const Platform &_platform;
  std::map<std::string, int> _platform_cores_count;
  std::unique_ptr<OptimizationObjective> _objective;

  // temporary fields (initialized at each invokation of GenerateSchedule())
  std::vector<Client *> _clients;
  double _start_time;
  CPUCoreSet _blocked;

  std::vector<std::vector<OperatingPoint>> _cl_pareto; // Pareto front of ops

  MappingList _best_ops;
  MappingListValue _best_value;
  MappingList _cur_ops;
};

} // namespace tetris

#endif /* __SCHED_BRUTEFORCE_H__ */
