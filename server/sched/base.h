#ifndef __MAPPER_BASE_H__
#define __MAPPER_BASE_H__

#pragma once

#include <vector>

#include "server/client.h"
#include "server/schedule.h"
#include "util/platform/cpu_sets.h"

namespace tetris {

/**
 * An abstract class defining the interface for a Mapper.
 */
class BaseScheduler {
public:
  /**
   * Generates a schedule.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \return The selected mappings.
   */
  virtual Schedule GenerateSchedule(std::vector<Client *> clients,
                                    double start_time) {
    return GenerateSchedule(clients, start_time, CPUThreadSet());
  }

  /**
   * Generates a schedule considering a list of blocked CPUs.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \param blocked_cpus The list of blocked CPUs.
   * \return The selected mappings.
   */
  virtual Schedule GenerateSchedule(std::vector<Client *> clients,
                                    double start_time,
                                    CPUThreadSet blocked_cpus) = 0;
};

} // namespace tetris

#endif /* __MAPPER_BASE_H__ */
