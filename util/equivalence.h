#ifndef __EQUIVALENCE_H__
#define __EQUIVALENCE_H__

#pragma once

#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "util/cpulist.h"

/**
 * \class EqualCPUS
 * \brief Represents a set of CPUs that are considered equivalent.
 *
 * This class is used to manage and manipulate a group of CPUs
 * that are considered equal in terms of their capabilities.
 */
// TODO (RK): In my opinion, the name of this class does not accurately
// represent its semantics. This class essentially acts as a wrapper for a
// CPUList, with an additional std::vector<int> that represents the same CPU
// list, along with a method to convert one EqualCPUS object to another.
// However, the actual CPU list could include non-equivalent cores in one
// object, such as CORTEX-A7 and CORTEX-A15, which contradicts the implied
// meaning of 'EqualCPUS'.

class EqualCPUS {
 private:
  CPUList _cpulist;
  std::vector<int> _cpus;

  friend class Equivalence;

  // Generates a conversion map from this set to another set of equivalent CPUs.
  std::map<int, int> conversion_map(const EqualCPUS& other) const {
    if (other._cpulist == _cpulist) return {};

    if (_cpus.size() != other._cpus.size())
      throw std::runtime_error{
          "Can't generate conversion map for mapping of different equivalence "
          "classes."};

    std::map<int, int> result;

    for (size_t i = 0; i < _cpus.size(); ++i) {
      if (_cpus[i] != other._cpus[i]) result.emplace(_cpus[i], other._cpus[i]);
    }

    return result;
  }

 public:
  EqualCPUS(const std::initializer_list<int>& cpus)
      : _cpulist{cpus}, _cpus{cpus} {}

  bool operator==(const CPUList& o) const { return _cpulist == o; }
};

bool operator==(const CPUList& c, const EqualCPUS& ec);

/**
 * \class Equivalence
 * \brief Represents an equivalence class of CPU sets.
 *
 * This class is used to manage a collection of sets of CPUs that
 * are considered interchangeable or equivalent in terms of their performance
 * characteristics.
 */
class Equivalence {
 private:
  std::string _name;
  std::vector<EqualCPUS> _equalcpus;

 public:
  Equivalence(const std::string& name,
              const std::initializer_list<EqualCPUS>& equalcpulists)
      : _name{name}, _equalcpus{equalcpulists} {}

  const std::string name() const { return _name; }

  // Checks if a given CPUList is part of the equivalence class.
  bool is_in_equalence_class(const CPUList& cpulist) const {
    for (const auto& ecpus : _equalcpus) {
      if (cpulist == ecpus) return true;
    }

    return false;
  }

  /**
   * \brief Get a list of equivalent mappings within the equivalence class for a
   * given CPUList.
   *
   * This function checks if the given CPUList is part of this equivalence
   * class, and if so, constructs a list of conversion maps from the matching
   * EqualCPUS instance to all other EqualCPUS instances within the equivalence
   * class. Each conversion map consists of pairs where each key-value pair
   * represents a CPU in the source EqualCPUS and its corresponding CPU in the
   * target EqualCPUS.
   *
   * \param cpulist The CPUList to be checked and for which equivalent mappings
   * are to be generated. \return A vector of maps where each map is a
   * conversion map from the given CPUList to another possible CPU configuration
   * within the equivalence class. \throws std::runtime_error if the provided
   * CPUList does not match any of the EqualCPUS instances within this
   * equivalence class.
   */
  std::vector<std::map<int, int>> equivalent_mappings(
      const CPUList& cpulist) const {
    for (const auto& ecpus : _equalcpus) {
      if (cpulist == ecpus) {
        std::vector<std::map<int, int>> result;

        for (const auto& other : _equalcpus)
          result.push_back(ecpus.conversion_map(other));

        return result;
      }
    }

    throw std::runtime_error{
        "This mapping is not part of this equivalence class."};
  }
};

#endif /* __EQUIVALENCE_H__ */
