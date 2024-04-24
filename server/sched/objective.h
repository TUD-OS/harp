#ifndef __SCHED_OBJECTIVE_H__
#define __SCHED_OBJECTIVE_H__

#pragma once

#include <cmath>

#include "server/client.h"
#include "server/client_mapping.h"

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
  virtual double EvaluateOP(const OperatingPoint &op) const = 0;

  /**
   * Evaluate the job w.r.t. some objective value.
   *
   * Returns an std::tuple<int, double>. The first value represents whether a
   * client was successfully scheduled (0 or 1). The second value represents
   * the objective value.
   */
  virtual std::tuple<int, double>
  EvaluateClient(const ClientMapping &client_mapping, Client *client) const = 0;

  /**
   * Evaluate the schedule  w.r.t. some objective value.
   *
   * Returns an std::tuple<int, double>. The first value is a number of
   * successfully scheduled clients. The second value represents the objective
   * value.
   */
  virtual std::tuple<int, double>
  EvaluateClientMapping(const ClientMapping &client_mapping) const {
    int num_apps = 0;
    double value = 0.0;
    for (auto &[client, op] : client_mapping.map) {
      auto cr = EvaluateClient(client_mapping, client);
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

  double EvaluateGEDP(double power, double utility) const {
    return pow(power / utility, _alpha) * pow(1.0 / utility, 1 - _alpha);
  }

public:
  GEDPObjective(double alpha) : _alpha{alpha} {
    if (alpha < 0 || alpha > 1) {
      throw std::runtime_error("alpha must be in the range 0.0..1.0");
    }
  }

  double EvaluateOP(const OperatingPoint &op) const override {
    return EvaluateGEDP(op.power(), op.utility());
  }

  std::tuple<int, double> EvaluateClient(const ClientMapping &client_mapping,
                                         Client *client) const override {
    if (!client_mapping.Contains(client)) {
      return std::make_tuple(0, 0.0);
    }
    auto op = client_mapping.Get(client);
    double value = EvaluateOP(op.base);
    return std::make_tuple(1, value);
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
