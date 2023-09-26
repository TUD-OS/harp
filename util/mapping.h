#ifndef __MAPPING_H__
#define __MAPPING_H__

#pragma once


#include "util/cpulist.h"
#include "util/platform/config.h"
#include "util/platform/cpu_sets.h"
#include "util/platform/equivalence.h"


#include <map>
#include <string>
#include <vector>

#include <sched.h>


namespace {

int cpu_nr_for_name(const std::string& name)
{
    auto i = cpu_map.find(name);
    if (i != cpu_map.end())
        return i->second;
    else
        return 0;
}

} /* Anonymous namespace */

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
   public:
    std::string     name;
    std::map<std::string, int> thread_map;
    RegionAffinities<int> region_map;
    std::map<std::string, double> characteristics_map;
    CPUThreadSet         cpus;

   private:
    Mapping(const Mapping& base, const std::map<int, int>& conv_map) :
        name{base.name}, thread_map{}, region_map{}, characteristics_map{base.characteristics_map}, cpus{}
    {
        // Convert thread CPU affinity.
        for (const auto& [name, orig_cpu] : base.thread_map) {
            if (conv_map.find(orig_cpu) != conv_map.end()) {
                auto new_cpu = conv_map.at(orig_cpu);
                thread_map.emplace(name, new_cpu);
                cpus.Set(new_cpu);
            } else {
                thread_map.emplace(name, orig_cpu);
                cpus.Set(orig_cpu);
            }
        }
        // Convert thread CPU affinity in parallel regions.
        for (const auto& [region_name, replicas] : base.region_map) {
            ReplicaAffinities<int> replica_affinities{};
            for (const auto& replica : replicas) {
                ProcessAffinities<int> process_affinities{};
                for (const auto& [process_name, orig_cpu] : replica) {
                    if (conv_map.find(orig_cpu) != conv_map.end()) {
                        auto new_cpu = conv_map.at(orig_cpu);
                        process_affinities.emplace(process_name, new_cpu);
                        cpus.Set(new_cpu);
                    } else {
                        process_affinities.emplace(process_name, orig_cpu);
                        cpus.Set(orig_cpu);
                    }
                }
                replica_affinities.push_back(process_affinities);
            }
            region_map.emplace(region_name, replica_affinities);
        }
    }

   public:
    Mapping() = default;

    Mapping(const std::string& name, const std::vector<std::pair<std::string, std::string>>& threads,
            const RegionAffinities<std::string>& region_threads,
            const std::vector<std::pair<std::string, std::string>>& characteristics) :
        name{name}, thread_map{}, characteristics_map{}, cpus{}
    {
        for (const auto& t : threads) {
            thread_map.emplace(t.first, cpu_nr_for_name(t.second));
            cpus.Set(cpu_nr_for_name(t.second));
        }

        for (const auto& [region_name, replicas] : region_threads) {
            ReplicaAffinities<int> replica_affinities{};
            for (const auto& replica : replicas) {
                ProcessAffinities<int> process_affinities{};
                for (const auto& [process_name, affinity] : replica) {
                    process_affinities.emplace(process_name, cpu_nr_for_name(affinity));
                    cpus.Set(cpu_nr_for_name(affinity));
                }

                replica_affinities.push_back(process_affinities);
            }
            region_map.emplace(region_name, replica_affinities);
        }

        for (const auto& c : characteristics) {
            characteristics_map.emplace(c.first, std::stod(c.second));
        }
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

};

#endif /* __MAPPING_H__ */
