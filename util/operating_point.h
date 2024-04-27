#ifndef __OPERATING_POINT_H__
#define __OPERATING_POINT_H__

#pragma once

#include "proto/tetris.pb.h"
#include "util/platform/cpu_sets.h"

#include <map>
#include <string>

namespace tetris {

struct OperatingPoint {
  struct Configuration {
    std::string name;                       // Unique name of the configuration
    CPUThreadSet threads;                   // Tracks active CPU threads
    std::map<std::string, int> core_counts; // Core counts by type
  };

  struct Metrics {
    double utility; // Performance (instructions per second)
    double power;   // Power consumption in watts
  };

  Configuration config; // Details about the configuration
  Metrics metrics;      // Metrics from the configuration

  const std::string &name() const { return config.name; }

  const CPUThreadSet &threads() const { return config.threads; }

  const std::map<std::string, int> &core_counts() const {
    return config.core_counts;
  }

  const double &utility() const { return metrics.utility; }

  const double &power() const { return metrics.power; }
};

struct OperatingPointAllocation {
  OperatingPoint base;
  std::map<int, int> permutation;

  const std::string &name() const { return base.config.name; }

  CPUThreadSet threads() const {
    CPUThreadSet res;
    auto cores = base.config.threads.GetList();

    for (auto &c : cores) {
      if (permutation.contains(c)) {
        res.Set(permutation.at(c));
      } else {
        res.Set(c);
      }
    }
    return res;
  }

  const std::map<std::string, int> &core_counts() const {
    return base.config.core_counts;
  }

  const double &utility() const { return base.metrics.utility; }

  const double &power() const { return base.metrics.power; }
};

} /* namespace tetris */

#endif // __OPERATING_POINT_H__
