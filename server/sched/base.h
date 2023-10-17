#ifndef __SCHED_BASE_H__
#define __SCHED_BASE_H__

#pragma once

#include <memory>
#include <vector>

#include "server/client.h"
#include "server/sched/objective.h"
#include "server/schedule.h"
#include "util/platform/cpu_sets.h"

namespace tetris {

/**
 * An abstract class defining the interface for a Mapper.
 */
class BaseScheduler {
public:
  virtual void
  SetObjective(std::unique_ptr<OptimizationObjective> objective) = 0;

  virtual OptimizationObjective *GetObjective() = 0;
  /**
   * Generates a schedule.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \return The selected mappings.
   */
  virtual std::unique_ptr<Schedule>
  GenerateSchedule(std::vector<Client *> clients, double start_time) {
    return GenerateSchedule(clients, start_time, CPUCoreSet());
  }

  /**
   * Generates a schedule considering a list of blocked CPUs.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \param blocked_cpus The list of blocked CPUs.
   * \return The selected mappings.
   */
  virtual std::unique_ptr<Schedule>
  GenerateSchedule(std::vector<Client *> clients, double start_time,
                   CPUCoreSet blocked_cores) = 0;
};

} // namespace tetris

#endif /* __SCHED_BASE_H__ */
