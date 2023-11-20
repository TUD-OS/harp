#ifndef __OPERATING_POINT_H__
#define __OPERATING_POINT_H__

#pragma once

#include "proto/tetris.pb.h"
#include "util/platform/cpu_sets.h"

#include <map>
#include <stdexcept>
#include <string>

namespace tetris {

class Platform;

class OperatingPoint {
public:
  std::string name;
  std::map<std::string, double> characteristics;
  CPUThreadSet cpus;
  std::map<std::string, int> cores_count;

public:
  OperatingPoint()
      : name{"default"}, characteristics{}, cpus{}, cores_count{} {}

  OperatingPoint(const std::string &name,
                 const std::map<std::string, double> &characteristics,
                 const CPUThreadSet &cpus,
                 const std::map<std::string, int> &cores_count)
      : name{name}, characteristics{characteristics}, cpus{cpus},
        cores_count{cores_count} {}

  OperatingPoint(const Platform &platform,
                 const ClientMessage::OperatingPointsInfo::OPData &op);

  double characteristic(const std::string &criteria) const {
    if (characteristics.find(criteria) != characteristics.end())
      return characteristics.at(criteria);

    throw std::runtime_error("Unknown characteristic criteria.");
  }

  int CoresCount(const std::string &core_type) {
    if (cores_count.find(core_type) != cores_count.end())
      return cores_count.at(core_type);

    throw std::runtime_error("Unknown core type.");
  }
};

class OperatingPointAllocation {
public:
  OperatingPoint base;
  std::map<int, int> cpu_allocation;

public:
  OperatingPointAllocation() : base{}, cpu_allocation{} {}

  OperatingPointAllocation(const OperatingPoint &base,
                           const std::map<int, int> &cpu_allocation)
      : base{base}, cpu_allocation{cpu_allocation} {}

  double characteristic(const std::string &criteria) const {
    return base.characteristic(criteria);
  }

  CPUThreadSet GetThreadSet() const {
    CPUThreadSet res;
    auto cores = base.cpus.GetList();

    for (auto &c : cores) {
      if (cpu_allocation.count(c) > 0) {
        res.Set(cpu_allocation.at(c));
      } else {
        res.Set(c);
      }
    }
    return res;
  }
};

} /* namespace tetris */

#endif // __OPERATING_POINT_H__
