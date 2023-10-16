#ifndef __SCHED_OBJECTIVE_H__
#define __SCHED_OBJECTIVE_H__

#pragma once

#include "server/client.h"
#include "server/schedule.h"

namespace tetris {

class OptimizationObjective {
public:
  virtual ~OptimizationObjective() = default;

  /**
   * Evaluate the job w.r.t. some objective value.
   *
   * Returns an std::tuple<int, double>. The first value represents whether a
   * client was successfully scheduled (0 or 1). The second value represents
   * the objective value.
   */
  virtual std::tuple<int, double> EvaluateClient(const Schedule &schedule,
                                                 Client *client) const = 0;

  /**
   * Evaluate the schedule  w.r.t. some objective value.
   *
   * Returns an std::tuple<int, double>. The first value is a number of
   * successfully scheduled clients. The second value represents the objective
   * value.
   */
  virtual std::tuple<int, double>
  EvaluateSchedule(const Schedule &schedule) const {
    auto clients = schedule.GetClients();
    int num_apps = 0;
    double value = 0.0;
    for (auto &c : clients) {
      auto cr = EvaluateClient(schedule, c);
      if (std::get<0>(cr) == 1) {
        num_apps += 1;
        value += std::get<1>(cr);
      }
    }
    return std::make_tuple(num_apps, value);
  }
};

class EnergyObjective : public OptimizationObjective {
public:
  std::tuple<int, double> EvaluateClient(const Schedule &schedule,
                                         Client *client) const override {
    if (schedule.IsMultiSegment()) {
      throw std::runtime_error("Not yet implemented");
    } else {
      if (schedule.GetNumberOfSegments() == 0)
        return std::make_tuple(0, 0.0);
      assert(schedule.GetNumberOfSegments() == 1);
      auto opt_op = schedule.GetOperatingPoint(0, client);
      if (!opt_op.has_value())
        return std::make_tuple(0, 0.0);
      auto op = *opt_op;
      double rem_cratio = 1.0 - client->progress;
      double value = op.characteristic("energy") * rem_cratio;
      return std::make_tuple(1, value);
    }
  }
};

} // namespace tetris

#endif /* __SCHED_OBJECTIVE_H__ */
