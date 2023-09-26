#ifndef __EQUIV_RES_ALLOC_H__
#define __EQUIV_RES_ALLOC_H__

#pragma once

#include "util/mapping.h"
#include "util/platform/cpu_sets.h"
#include "util/platform/platform.h"

#include <map>
#include <optional>
#include <stdexcept>
#include <string>

class Platform;

class EquivResAllocator {
public:
  virtual ~EquivResAllocator() = default;

  virtual std::string GetEquivClassName(const CPUCoreSet &cpus) const = 0;

  virtual std::string GetEquivClassName(const CPUThreadSet &cpus) const = 0;

  virtual std::string GetEquivClassName(const Mapping &m) const = 0;

  virtual std::optional<Mapping>
  FindEquivMapping(const Mapping &m, const CPUCoreSet &used_cpus) const = 0;
};

class CoreTypeBasedEquivResAllocator : public EquivResAllocator {
public:
  CoreTypeBasedEquivResAllocator(Platform *platform) : _platform(platform) {}

  std::string GetEquivClassName(const CPUCoreSet &core_set) const override;

  std::string GetEquivClassName(const CPUThreadSet &threads) const override;

  std::string GetEquivClassName(const Mapping &m) const override {
    throw std::runtime_error("Not yet implemented");
  }

  std::optional<Mapping>
  FindEquivMapping(const Mapping &m,
                   const CPUCoreSet &used_cpus) const override {
    throw std::runtime_error("Not yet implemented");
  }

private:
  Platform *_platform;
};

#endif /* __EQUIV_RES_ALLOC_H__ */
