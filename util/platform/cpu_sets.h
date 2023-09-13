#ifndef __CPU_SETS_H__
#define __CPU_SETS_H__

#pragma once

/**
 * \class CPUCoreSet
 * \brief Represents a set of CPU cores and provides operations for manipulating
 * CPU sets.
 *
 * All CPU cores are encoded by the integer number, an index of the core in the
 * platform description.
 */
class CPUCoreSet {
public:
  CPUCoreSet() {}

  template <template <typename> class Container>
  CPUCoreSet(const Container<int> &cpus) : CPUCoreSet{} {
    for (const auto c : cpus)
      _set.insert(c);
  }

  CPUCoreSet(const std::initializer_list<int> &cpus) : CPUCoreSet{} {
    for (const auto c : cpus)
      _set.insert(c);
  }

  CPUCoreSet(const CPUCoreSet &o) = default;
  CPUCoreSet(CPUCoreSet &&o) = default;

  CPUCoreSet &operator=(const CPUCoreSet &o) = default;
  CPUCoreSet &operator=(CPUCoreSet &&o) = default;

  bool operator==(const CPUCoreSet &o) const { return _set == o._set; }

  bool operator!=(const CPUCoreSet &o) const { return !(*this == o); }

  CPUCoreSet operator&(const CPUCoreSet &o) const {
    CPUCoreSet tmp{*this};
    for (auto c : _set) {
      if (o._set.count(c) == 0) {
        tmp.Erase(c);
      }
    }
    return tmp;
  }

  CPUCoreSet &operator&=(const CPUCoreSet &o) {
    CPUCoreSet tmp{*this};
    for (auto c : _set) {
      if (o._set.count(c) == 0) {
        tmp.Erase(c);
      }
    }
    *this = tmp;

    return *this;
  }

  CPUCoreSet operator|(const CPUCoreSet &o) const {
    CPUCoreSet tmp{*this};
    for (auto c : o._set) {
      tmp.Set(c);
    }
    return tmp;
  }

  CPUCoreSet operator|=(const CPUCoreSet &o) {
    for (auto c : o._set) {
      this->Set(c);
    }
    return *this;
  }

  void Set(int core_id) { _set.insert(core_id); }

  void Erase(int core_id) { _set.erase(core_id); }

  void Zero() { _set.clear(); }

  std::size_t Size() { return _set.size(); }

  bool OverlapsWith(const CPUCoreSet &o) const {
    CPUCoreSet tmp = *this & o;
    return tmp.Size() != 0;
  }

  // Returns a list of CPU indices in the set.
  std::vector<int> GetCoreList() const {
    std::vector<int> result;

    for (auto c : _set) {
      result.push_back(c);
    }

    return result;
  }

  // Make CPUCoreSet compatible with range-based loops
  std::set<int>::iterator begin() { return _set.begin(); }

  std::set<int>::iterator end() { return _set.end(); }

  std::set<int>::const_iterator begin() const { return _set.begin(); }

  std::set<int>::const_iterator end() const { return _set.end(); }

private:
  std::set<int> _set;
};

class CPUThreadset {};

#endif /* __CPULIST_H__ */
