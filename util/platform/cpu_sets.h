#ifndef __CPU_SETS_H__
#define __CPU_SETS_H__

#pragma once

#include <sched.h>

#include <initializer_list>
#include <set>
#include <string>
#include <vector>

#include "util/string_util.h"

namespace tetris {

template <class Derived> class CPUSetBase {
public:
  CPUSetBase() {}

  template <template <typename> class Container>
  CPUSetBase(const Container<int> &cpus) : CPUSetBase{} {
    for (const auto c : cpus)
      _set.insert(c);
  }

  CPUSetBase(const std::initializer_list<int> &cpus) : CPUSetBase{} {
    for (const auto c : cpus)
      _set.insert(c);
  }

#if 0
  CPUSetBase(const Derived &o) = default;
  CPUSetBase(Derived &&o) = default;

  Derived &operator=(const Derived &o) = default;
  Derived &operator=(Derived &&o) = default;
#endif

private:
  Derived &GetDerived() { return static_cast<Derived &>(*this); }

  const Derived &GetDerived() const {
    return static_cast<const Derived &>(*this);
  }

public:
  bool operator==(const Derived &o) const { return _set == o._set; }

  bool operator!=(const Derived &o) const { return !(*this == o); }

  Derived operator&(const Derived &o) const {
    Derived tmp = GetDerived();
    for (auto c : _set) {
      if (o._set.count(c) == 0) {
        tmp.Erase(c);
      }
    }
    return tmp;
  }

  Derived &operator&=(const Derived &o) {
    for (auto c : _set) {
      if (o._set.count(c) == 0) {
        this->Erase(c);
      }
    }

    return GetDerived();
  }

  Derived operator|(const Derived &o) const {
    Derived tmp = GetDerived();
    for (auto c : o._set) {
      tmp.Set(c);
    }
    return tmp;
  }

  Derived &operator|=(const Derived &o) {
    for (auto c : o._set) {
      this->Set(c);
    }
    return GetDerived();
  }

  Derived operator^(const Derived &o) const {
    Derived tmp = GetDerived();
    for (auto c : o._set) {
      if (tmp.At(c))
        tmp.Erase(c);
      else
        tmp.Set(c);
    }
    return tmp;
  }

  Derived &operator^=(const Derived &o) {
    for (auto c : o._set) {
      if (this->At(c))
        this->Erase(c);
      else
        this->Set(c);
    }
    return GetDerived();
  }

  bool At(int core_id) const { return _set.count(core_id) == 1; }

  void Set(int core_id) { _set.insert(core_id); }

  void Erase(int core_id) { _set.erase(core_id); }

  void Zero() { _set.clear(); }

  std::size_t Size() const { return _set.size(); }

  bool OverlapsWith(const Derived &o) const {
    Derived tmp = *this & o;
    return tmp.Size() != 0;
  }

  bool IsSubsetOf(const Derived &o) const {
    for (auto key : _set) {
      if (!o.At(key)) {
        return false;
      }
    }
    return true;
  }

  // Returns a list of CPU indices in the set.
  std::vector<int> GetList() const {
    std::vector<int> result;

    for (auto c : _set) {
      result.push_back(c);
    }

    return result;
  }

  std::string GetString() const {
    return "{" + string_util::join(_set, ", ") + "}";
  }

  // Make CPUSetBase compatible with range-based loops
  std::set<int>::iterator begin() { return _set.begin(); }

  std::set<int>::iterator end() { return _set.end(); }

  std::set<int>::const_iterator begin() const { return _set.begin(); }

  std::set<int>::const_iterator end() const { return _set.end(); }

protected:
  std::set<int> _set;
};

/**
 * \class CPUCoreSet
 * \brief Represents a set of CPU cores and provides operations for manipulating
 * CPU sets.
 *
 * All CPU cores are encoded by the integer number, an index of the core in the
 * platform description.
 */
class CPUCoreSet : public CPUSetBase<CPUCoreSet> {
public:
  CPUCoreSet() = default;

  CPUCoreSet(std::initializer_list<int> ilist)
      : CPUSetBase<CPUCoreSet>(ilist) {}

  template <template <typename> class Container>
  CPUCoreSet(const Container<int> &vec) : CPUSetBase<CPUCoreSet>(vec) {}

  // specific functionality or data members for CPUCoreSet
};

/**
 * \class CPUThreadSet
 * \brief Represents a set of CPU threads and provides operations for
 *  manipulating CPU sets.
 *
 * All CPU threads are encoded by the integer number, a cpu affinity.
 */
class CPUThreadSet : public CPUSetBase<CPUThreadSet> {
public:
  CPUThreadSet() = default;

  CPUThreadSet(std::initializer_list<int> ilist)
      : CPUSetBase<CPUThreadSet>(ilist) {}

  template <template <typename> class Container>
  CPUThreadSet(const Container<int> &vec) : CPUSetBase<CPUThreadSet>(vec) {}

  // specific functionality or data members for CPUThreadSet

  // Returns a cpu_set_t value
  cpu_set_t ToCpuSetT() const {
    cpu_set_t tmp;
    CPU_ZERO(&tmp);
    for (auto c : _set) {
      CPU_SET(c, &tmp);
    }
    return tmp;
  }
};

} /* namespace tetris */

#endif /* __CPU_SETS_H__ */
