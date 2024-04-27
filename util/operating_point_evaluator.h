#ifndef __OPERATING_POINT_EVALUATOR_H__
#define __OPERATING_POINT_EVALUATOR_H__

#pragma once

#include <cmath>

#include "operating_point.h"

namespace tetris {

inline const double kMinPower = 0;
inline const double kMinUtility = 1e-6;

class OperatingPointEvaluator {
public:
  virtual ~OperatingPointEvaluator() = default;
  virtual double Evaluate(const OperatingPoint &op) const = 0;
};

template <double alpha>
class GeneralizedEDPEvaluator : public OperatingPointEvaluator {
private:
  static double CalculateGEDP(double power, double utility) {
    // Calculates the generalized energy-delay product
    power = (power < kMinPower) ? kMinPower : power;
    utility = (utility < kMinUtility) ? kMinUtility : utility;
    return pow(power / utility, alpha) * pow(1.0 / utility, 1 - alpha);
  }

public:
  explicit GeneralizedEDPEvaluator() {
    if (alpha < 0 || alpha > 1) {
      throw std::runtime_error("Alpha must be in the range [0.0, 1.0]");
    }
  }

  double Evaluate(const OperatingPoint &op) const override {
    return CalculateGEDP(op.power(), op.utility());
  }
};

// Focuses entirely on minimizing power consumption
class EnergyEvaluator : public GeneralizedEDPEvaluator<1.0> {
public:
  EnergyEvaluator() : GeneralizedEDPEvaluator() {}
};

// Focuses entirely on maximizing utility
class PerformanceEvaluator : public GeneralizedEDPEvaluator<0.0> {
public:
  PerformanceEvaluator() : GeneralizedEDPEvaluator() {}
};

// Balances between power and utility
class BalancedEvaluator : public GeneralizedEDPEvaluator<0.5> {
public:
  BalancedEvaluator() : GeneralizedEDPEvaluator() {}
};

class OperatingPointEvaluatorFactory {
public:
  static std::unique_ptr<OperatingPointEvaluator>
  Create(const std::string &type) {
    if (type == "energy") {
      return std::make_unique<EnergyEvaluator>();
    } else if (type == "balanced") {
      return std::make_unique<BalancedEvaluator>();
    } else if (type == "performance") {
      return std::make_unique<PerformanceEvaluator>();
    } else {
      throw std::invalid_argument("Unsupported evaluator type: " + type);
    }
  }
};

} // namespace tetris

#endif
