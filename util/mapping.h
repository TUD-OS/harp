#ifndef __MAPPING_H__
#define __MAPPING_H__

#pragma once

#include "util/platform/cpu_sets.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>


namespace tetris {

/* Forward declaration to avoid circular dependencies */
class Platform;
class OperatingPoint;


/// \brief ProcessAffinities stores process names and corresponding CPU affinity.
template <typename T>
using ProcessAffinities = std::map<std::string, T>;

/// \brief ReplicasAffinities stores processes inside a region replica with CPU affinities.
template <typename T>
using ReplicaAffinities = std::vector<ProcessAffinities<T>>;

/// \brief RegionAffinities stores replicas inside a region with CPU affinities.
template <typename T>
using RegionAffinities = std::map<std::string, ReplicaAffinities<T>>;


class Mapping
{
   private:
    const Platform& _platform;
   public:
    std::string     name;
    std::map<std::string, int> thread_map;
    RegionAffinities<int> region_map;
    std::map<std::string, double> characteristics_map;
    CPUThreadSet         cpus;

   public:
    explicit Mapping(const Platform& platform): _platform{platform}, name{},
             thread_map{}, region_map{}, characteristics_map{}, cpus{}
    {}

    Mapping(const Platform& platform, const std::string& name,
            const std::vector<std::pair<std::string, std::string>>& threads,
            const RegionAffinities<std::string>& regions,
            const std::vector<std::pair<std::string, std::string>>& characteristics);

    Mapping(const Mapping& base, const std::map<int, int>& conv_map);

    Mapping& operator=(const Mapping& other) {
      if (&_platform != &other._platform) {
        throw std::runtime_error{"The platform object of the new mapping differs "
                                 "from the current one."};
      }
      name = other.name;
      thread_map = other.thread_map;
      region_map = other.region_map;
      characteristics_map = other.characteristics_map;
      cpus = other.cpus;
      return *this;
    }

    CPUThreadSet cpu(const std::string& thread) const
    {
        auto it = thread_map.find(thread);
        if (it != thread_map.end())
            return {it->second};
        else
            /* If we don't know this thread we will enable all cores of this mapping */
            return cpus;
    }

    double characteristic(const std::string& criteria) const
    {
        if (characteristics_map.find(criteria) != characteristics_map.end())
            return characteristics_map.at(criteria);

        throw std::runtime_error("Unknown characteristic criteria.");
    }

    OperatingPoint op() const;
};

} /* namespace tetris */

#endif /* __MAPPING_H__ */
