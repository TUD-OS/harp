#ifndef __SCHED_BASE_H__
#define __SCHED_BASE_H__

#pragma once

#include <memory>
#include <vector>

#include "server/client.h"
#include "server/client_mapping.h"
#include "util/operating_point_evaluator.h"
#include "util/platform/cpu_sets.h"

namespace tetris {

/**
 * An abstract class defining the interface for a Mapper.
 */
class BaseClientMapper {
public:
  explicit BaseClientMapper(const Platform &platform);

  /**
   * \brief Sets an operating point evaluator
   */
  void SetOperatingPointEvaluator(
      std::shared_ptr<OperatingPointEvaluator> evaluator) {
    _evaluator = std::move(evaluator);
  }

  /**
   * \brief Returns the operating point evaluator
   */
  std::shared_ptr<OperatingPointEvaluator> GetOperatingPointEvaluator() {
    return _evaluator;
  }

  /**
   * Generates a Client Mapping
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \return The selected mappings.
   */
  virtual ClientMapping GenerateClientMapping(std::vector<Client *> clients) {
    return GenerateClientMapping(clients, CPUCoreSet());
  }

  /**
   * Generates a Client Mapping considering a list of blocked CPUs.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param blocked_cpus The list of blocked CPUs.
   * \return The selected mappings.
   */
  virtual ClientMapping GenerateClientMapping(std::vector<Client *> clients,
                                              CPUCoreSet blocked_cores) = 0;

protected:
  using MappingList = std::vector<const OperatingPoint *>;

  /**
   * Create a ClientMapping object with the current mapping list
   */
  ClientMapping ToClientMapping(const std::vector<Client *> clients,
                                const MappingList &ops, CPUCoreSet busy_cores);

  const Platform &_platform;
  std::map<std::string, int> _platform_cores_count;
  std::shared_ptr<OperatingPointEvaluator> _evaluator;
};

} // namespace tetris

#endif /* __SCHED_BASE_H__ */
