#ifndef __EQUIV_RES_ALLOC_H__
#define __EQUIV_RES_ALLOC_H__

#include "util/mapping.h"
#pragma once

#include "util/mapping.h"
#include "util/operating_point.h"
#include "util/platform/cpu_sets.h"

#include <map>
#include <optional>
#include <string>

namespace tetris {

class Platform;

class EquivResAllocator {
public:
  virtual ~EquivResAllocator() = default;

  virtual std::string GetEquivClassName(const CPUCoreSet &cpus) const = 0;

  virtual std::string GetEquivClassName(const CPUThreadSet &cpus) const = 0;

  virtual std::string GetEquivClassName(const Mapping &map) const = 0;

  virtual std::string GetEquivClassName(const OperatingPoint &op) const = 0;

  virtual std::string
  GetEquivClassName(const OperatingPointAllocation &op) const {
    return GetEquivClassName(op.base);
  }

  virtual std::optional<Mapping>
  FindEquivMapping(const Mapping &m, const CPUCoreSet &used_cpus) const = 0;

  virtual std::optional<OperatingPointAllocation>
  FindEquivOP(const OperatingPoint &op, const CPUCoreSet &used_cpus) const = 0;
};

class CoreTypeBasedEquivResAllocator : public EquivResAllocator {
public:
  CoreTypeBasedEquivResAllocator(Platform *platform) : _platform(platform) {}

  std::string GetEquivClassName(const CPUCoreSet &core_set) const override;

  std::string GetEquivClassName(const CPUThreadSet &threads) const override;

  std::string GetEquivClassName(const Mapping &map) const override {
    return GetEquivClassName(map.cpus);
  }

  std::string GetEquivClassName(const OperatingPoint &op) const override {
    return GetEquivClassName(op.threads());
  }

  std::optional<Mapping>
  FindEquivMapping(const Mapping &m,
                   const CPUCoreSet &used_cpus) const override;

  std::optional<OperatingPointAllocation>
  FindEquivOP(const OperatingPoint &op,
              const CPUCoreSet &used_cpus) const override;

private:
  std::optional<std::map<int, int>>
  GenerateCorePermutation(const CPUCoreSet &, const CPUCoreSet &) const;

  std::map<int, int> ToThreadPermutation(const std::map<int, int> &) const;

private:
  Platform *_platform;
};

} /* namespace tetris */

#endif /* __EQUIV_RES_ALLOC_H__ */
