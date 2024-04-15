#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#pragma once

#include "util/platform/cpu_sets.h"
#include "util/platform/equiv_res_alloc.h"

#include "util/debug_util.h"

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace tetris {

template <typename T> class TD;

class CPUCore;
class EquivResAllocator;
class Platform;

/**
 * \class CPUType
 * \brief Represents a specific CPU type and the number of hardware threads it
 * supports.
 */
class CPUType {
public:
  CPUType(const std::string &name, int num_threads)
      : _name(name), _num_threads(num_threads) {}

  CPUType(const CPUType &) = delete;
  CPUType &operator=(const CPUType &) = delete;

  CPUType(CPUType &&) = default;
  CPUType &operator=(CPUType &&) = default;

public:
  std::string GetName() const { return _name; }

  int GetNumThreads() const { return _num_threads; }

private:
  std::string _name;
  int _num_threads;
};

/**
 * \class CPUThread
 * \brief Represents an individual hardware thread within a CPU core.
 */
class CPUThread {
public:
  CPUThread(CPUCore &core, const std::string &name, int thread_id)
      : _core(core), _name(name), _id(thread_id) {}

  CPUThread(const CPUThread &) = delete;
  CPUThread &operator=(const CPUThread &) = delete;

  CPUThread(CPUThread &&) = default;
  CPUThread &operator=(CPUThread &&) = default;

public:
  CPUCore &GetCPUCore() const { return _core; }

  std::string GetName() const { return _name; }

  int GetID() { return _id; }

private:
  CPUCore &_core;
  std::string _name;
  int _id;
};

/**
 * \class CPUCore
 * \brief Represents a CPU core, which is part of a platform and has a specific
 * type. It contains multiple hardware threads.
 */
class CPUCore {
public:
  CPUCore(Platform &platform, CPUType &type, int core_id)
      : _platform(platform), _type(type), _id(core_id) {}

  CPUCore(const CPUCore &) = delete;
  CPUCore &operator=(const CPUCore &) = delete;

  CPUCore(CPUCore &&) = default;
  CPUCore &operator=(CPUCore &&) = default;

public:
  const Platform &GetPlatform() const { return _platform; }
  const CPUType &GetType() const { return _type; }
  int GetID() const { return _id; }

  std::vector<CPUThread *> GetCPUThreads() {
    std::vector<CPUThread *> res;
    for (auto &t : _threads) {
      res.push_back(t.get());
    }
    return res;
  }

private:
  void AddThread(const std::string &name, int affinity);

  friend class YamlPlatformReader;
  friend class Platform;

private:
  Platform &_platform;
  CPUType &_type;
  int _id;
  std::vector<std::unique_ptr<CPUThread>> _threads;
};

/**
 * \class Platform
 * \brief A platform that manages various CPU cores and threads, along with
 * their configurations. Provides methods to find and manage CPU threads and
 * cores based on different criteria.
 */
class Platform {
public:
  Platform() = default;
  Platform(const Platform &) = delete;
  Platform(Platform &&) = default;
  Platform &operator=(Platform &&) = default;

public:
  void SetEquivResAllocator(std::unique_ptr<EquivResAllocator> allocator) {
    _equiv_res_allocator = std::move(allocator);
  }

  EquivResAllocator &GetEquivResAllocator() const {
    return *_equiv_res_allocator.get();
  }

  CPUThread *FindCPUThread(int index) const {
    if (_cpu_threads.count(index) == 0)
      return nullptr;
    return _cpu_threads.at(index);
  }

  CPUThread *FindCPUThread(const std::string &name) const {
    for (const auto &[__, thread_ptr] : _cpu_threads) {
      if (thread_ptr->GetName() == name)
        return thread_ptr;
    }
    return nullptr;
  }

  CPUCore *FindCPUCore(int index) const {
    if (index < 0 || index >= _cpu_cores.size()) {
      return nullptr;
    }
    return _cpu_cores[index].get();
  }

  std::vector<CPUCore *> GetCPUCores() const {
    std::vector<CPUCore *> cpu_list;
    for (auto &core : _cpu_cores) {
      cpu_list.push_back(core.get());
    }
    return cpu_list;
  }

  std::vector<CPUCore *> GetCPUCores(const CPUCoreSet &core_set) const {
    std::vector<CPUCore *> cpu_list;

    for (auto index : core_set) {
      if (index < 0 || index >= _cpu_cores.size()) {
        throw std::out_of_range("Invalid CPU core index encountered.");
      }
      cpu_list.push_back(_cpu_cores[index].get());
    }

    return cpu_list;
  }

  std::map<int, CPUThread *> GetCPUThreads() const { return _cpu_threads; }

  std::vector<CPUThread *> GetCPUThreads(const CPUThreadSet &thread_set) const {
    std::vector<CPUThread *> res;

    for (auto index : thread_set) {
      CPUThread *thread_ptr = FindCPUThread(index);
      if (thread_ptr == nullptr) {
        throw std::out_of_range("Invalid CPU thread affinity encountered.");
      }
      res.push_back(thread_ptr);
    }

    return res;
  }

  CPUCoreSet ToCPUCoreSet(const CPUThreadSet &thread_set) const {
    CPUCoreSet res;
    for (auto t : GetCPUThreads(thread_set)) {
      res.Set(t->GetCPUCore().GetID());
    }
    return res;
  }

  CPUThreadSet ToCPUThreadSet(const CPUCoreSet &core_set) const {
    CPUThreadSet res;
    for (auto c : GetCPUCores(core_set)) {
      for (auto t : c->GetCPUThreads()) {
        res.Set(t->GetID());
      }
    }
    return res;
  }

  std::map<std::string, int> GetCoreCountPerType() const {
    std::map<std::string, int> res;
    for (const auto &[name, _] : _cpu_types) {
      res.emplace(name, 0);
    }

    auto cores = GetCPUCores();
    for (auto &c : cores) {
      res[c->GetType().GetName()]++;
    }

    return res;
  }

  std::map<std::string, int>
  GetCoreCountPerType(const CPUCoreSet &core_set) const {
    std::map<std::string, int> res;
    for (const auto &[name, _] : _cpu_types) {
      res.emplace(name, 0);
    }

    auto cores = GetCPUCores(core_set);
    for (auto &c : cores) {
      res[c->GetType().GetName()]++;
    }

    return res;
  }

  std::map<std::string, int> GetThreadCapacityInfo() const {
    std::map<std::string, int> res;
    for (const auto &[name, cpu_type] : _cpu_types) {
      res.emplace(name, cpu_type->GetNumThreads());
    }

    return res;
  }

  std::map<std::string, std::vector<int>>
  GetThreadUsageInfo(const CPUThreadSet &thread_set) const {
    std::map<std::string, std::vector<int>> res;
    for (const auto &[name, cpu_type] : _cpu_types) {
      res.emplace(name, std::vector<int>(cpu_type->GetNumThreads()));
    }

    for (auto &c : GetCPUCores()) {
      auto name = c->GetType().GetName();

      auto threads = c->GetCPUThreads();
      int n = 0;
      for (auto &t : threads) {
        auto id = t->GetID();
        if (thread_set.At(id)) {
          n++;
        }
      }

      if (n > 0) {
        res[name][n - 1] += 1;
      }
    }
    return res;
  }

  CPUThreadSet GetCPUThreadSetFromThreadUsageInfo(
      const std::map<std::string, std::vector<int>> &thread_usage) const {
    CPUThreadSet res;
    for (const auto &[type, vdata] : thread_usage) {
      auto cpu_type = GetCPUType(type);
      assert(vdata.size() == cpu_type->GetNumThreads());
      const auto &indices_per_core = _indices_per_type.at(type);
      int core_idx = 0;
      for (int w = vdata.size() - 1; w >= 0; --w) {
        int n = vdata[w];
        while (n > 0) {
          const auto &indices = indices_per_core.at(core_idx);
          for (int i = 0; i <= w; ++i) {
            res.Set(indices.at(i));
          }
          --n;
          core_idx++;
        }
      }
    }
    return res;
  }

private:
  void AddCPUType(const std::string &name, int num_threads) {
    auto cpu_type = std::make_unique<CPUType>(name, num_threads);
    _cpu_types.insert({name, std::move(cpu_type)});
  }

  CPUType *GetCPUType(const std::string &name) const {
    return _cpu_types.at(name).get();
  }

  CPUCore *AddCore(const std::string &core_type) {
    CPUType *cpu_type_ = GetCPUType(core_type);
    int num = _cpu_cores.size();
    auto core = std::make_unique<CPUCore>(*this, *cpu_type_, num);
    auto raw_ptr = core.get();
    _cpu_cores.push_back(std::move(core));
    return raw_ptr;
  }

  void RegisterThread(int index, CPUThread *thread_ptr) {
    if (_cpu_threads.count(index) > 0)
      throw std::runtime_error("Several CPU Threads have the same affinity.");
    _cpu_threads.insert({index, thread_ptr});
  }

  void FinishConstruction() {
    for (const auto c : GetCPUCores()) {
      auto type = c->GetType().GetName();
      auto threads = c->GetCPUThreads();
      std::vector<int> core_indices{};
      for (const auto t : threads) {
        core_indices.push_back(t->GetID());
      }
      _indices_per_type[type].push_back(core_indices);
    }
  }

  friend class CPUCore;
  friend class YamlPlatformReader;

private:
  std::unique_ptr<EquivResAllocator> _equiv_res_allocator;
  std::map<std::string, std::unique_ptr<CPUType>> _cpu_types;
  std::vector<std::unique_ptr<CPUCore>> _cpu_cores;
  std::map<int, CPUThread *> _cpu_threads;

  std::map<std::string, std::vector<std::vector<int>>> _indices_per_type;
};

} /* namespace tetris */

#endif /* __PLATFORM_H__ */
