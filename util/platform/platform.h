#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#pragma once

#include "util/platform/cpu_sets.h"

class CPUCore;

class CPUType {
public:
  CPUType(const std::string &name, int num_threads)
      : _name(name), _num_threads(num_threads) {}

  CPUType(const CPUType &) = delete;

  CPUType(CPUType &&) = default;
  CPUType &operator=(CPUType &&) = default;

public:
  std::string GetName() const { return _name; }

  int GetNumThreads() const { return _num_threads; }

private:
  std::string _name;
  int _num_threads;
};

class CPUThread {
public:
  CPUThread(CPUCore &core, const std::string &name, int thread_id)
      : _core(core), _name(name), _id(thread_id) {}

  CPUThread(const CPUThread &) = delete;

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

class CPUCore {
public:
  CPUCore(CPUType &type, int core_id) : _type(type), _id(core_id) {}

  CPUCore(const CPUCore &) = delete;

  CPUCore(CPUCore &&) = default;
  CPUCore &operator=(CPUCore &&) = default;

public:
  int GetID() { return _id; }
  const CPUType &GetType() { return _type; }

  std::vector<std::reference_wrapper<CPUThread>> GetCPUThreads() {
    std::vector<std::reference_wrapper<CPUThread>> res;
    for (auto &t : _threads) {
      res.push_back(std::ref(t));
    }
    return res;
  }

private:
  void AddThread(const std::string &name, int affinity) {
    _threads.emplace_back(*this, name, affinity);
  }

  friend class YamlPlatformReader;
  friend class Platform;

private:
  CPUType &_type;
  int _id;
  std::vector<CPUThread> _threads;
};

class Platform {
public:
  std::vector<std::reference_wrapper<CPUCore>> GetCPUCores() {
    std::vector<std::reference_wrapper<CPUCore>> cpu_list;
    for (auto &core : _cpu_cores) {
      cpu_list.push_back(std::ref(core));
    }
    return cpu_list;
  }

  std::vector<std::reference_wrapper<CPUCore>>
  GetCPUCores(const CPUCoreSet &core_set) {
    std::vector<std::reference_wrapper<CPUCore>> cpu_list;

    for (auto index : core_set) {
      if (index < 0 || index >= _cpu_cores.size()) {
        throw std::out_of_range("Invalid CPU core index encountered.");
      }
      cpu_list.push_back(std::ref(_cpu_cores[index]));
    }

    return cpu_list;
  }

  std::vector<std::reference_wrapper<CPUThread>>
  GetCPUThreads(const CPUThreadSet &thread_set) {
    std::vector<std::reference_wrapper<CPUThread>> res;

    for (auto index : thread_set) {
      CPUThread *thread_ptr = FindCPUThread(index);
      if (thread_ptr == nullptr) {
        throw std::out_of_range("Invalid CPU thread affinity encountered.");
      }
      res.push_back(std::ref(*thread_ptr));
    }

    return res;
  }

private:
  void AddCPUType(const std::string &name, int num_threads) {
    _cpu_types.try_emplace(name, name, num_threads);
  }

  CPUType &GetCPUType(const std::string &name) { return _cpu_types.at(name); }

  CPUCore &AddCore(const std::string &core_type) {
    CPUType &cpu_type_ = GetCPUType(core_type);
    int num = _cpu_cores.size();
    _cpu_cores.emplace_back(cpu_type_, num);
    return _cpu_cores.back();
  }

  friend class YamlPlatformReader;

private:
  std::map<std::string, CPUType> _cpu_types;
  std::vector<CPUCore> _cpu_cores;
};

#endif /* __PLATFORM_H__ */
