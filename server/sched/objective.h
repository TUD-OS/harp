#ifndef __SCHED_OBJECTIVE_H__
#define __SCHED_OBJECTIVE_H__

#pragma once

#include <cmath>

#include "server/client.h"
#include "server/schedule.h"

namespace tetris {

class Platform;

class OptimizationObjective {
private:
  struct OPParetoState {
    std::map<std::string, int> cores;
    double value;
    bool is_pareto;

    bool Dominates(const OPParetoState &other) {
      bool res = true;
      for (auto &[name, core_count] : cores) {
        if (core_count > other.cores.at(name))
          return false;
      }
      if (value > other.value) {
        return false;
      }
      return true;
    }
  };

public:
  virtual ~OptimizationObjective() = default;

  /**
   * Evaluate an operating point.
   */
  virtual double EvaluateOP(const OperatingPoint &op,
                            double rem_cratio = 1.0) const = 0;

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

  /**
   * Filter Pareto-optimal operating points.
   *
   * Filter the given operating points w.r.t. the following objectives:
   * number of used cores of each type and the objective value.
   */
  virtual std::vector<OperatingPoint>
  FilterParetoFront(const Platform &platform,
                    const std::vector<OperatingPoint> &ops) const;
};

class GEDPObjective : public OptimizationObjective {
private:
  double _alpha;

  double EvaluateGEDP(double power, double utility, double rem_cratio) const {
    return pow(power / utility * rem_cratio, _alpha) *
           pow(1.0 / utility * rem_cratio, 1 - _alpha);
  }

public:
  GEDPObjective(double alpha) : _alpha{alpha} {
    if (alpha < 0 || alpha > 1) {
      throw std::runtime_error("alpha must be in the range 0.0..1.0");
    }
  }

  double EvaluateOP(const OperatingPoint &op,
                    double rem_cratio = 1.0) const override {
    return EvaluateGEDP(op.power(), op.utility(), rem_cratio);
  }

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
      double value = EvaluateOP(op.base, rem_cratio);
      return std::make_tuple(1, value);
    }
  }
};

class EnergyObjective : public GEDPObjective {
public:
  EnergyObjective() : GEDPObjective{1} {}
};

class DelayObjective : public GEDPObjective {
public:
  DelayObjective() : GEDPObjective{0} {}
};

class PerformanceObjective : public GEDPObjective {
public:
  PerformanceObjective() : GEDPObjective{0.25} {}
};

class BalancedObjective : public GEDPObjective {
public:
  BalancedObjective() : GEDPObjective{0.5} {}
};

class EnergySavingObjective : public GEDPObjective {
public:
  EnergySavingObjective() : GEDPObjective{0.75} {}
};

} // namespace tetris

#endif /* __SCHED_OBJECTIVE_H__ */
