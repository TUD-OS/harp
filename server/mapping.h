#ifndef __MAPPING_H__
#define __MAPPING_H__

#pragma once


#include "cpulist.h"
#include "config.h"
#include "equivalence.h"
#include "knob_description.h"


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
    CPUList         cpus;

   private:
    Mapping(const Mapping& base, const std::map<int, int>& conv_map) :
        name{base.name}, thread_map{}, region_map{}, characteristics_map{base.characteristics_map}, cpus{}
    {
        // Convert thread CPU affinity.
        for (const auto& [name, orig_cpu] : base.thread_map) {
            if (conv_map.find(orig_cpu) != conv_map.end()) {
                thread_map.emplace(name, conv_map.at(orig_cpu));
                cpus.set(conv_map.at(orig_cpu));
            } else {
                thread_map.emplace(name, orig_cpu);
                cpus.set(orig_cpu);
            }
        }
        // Convert thread CPU affinity in parallel regions.
        for (const auto& [region_name, replicas] : base.region_map) {
            ReplicaAffinities<int> replica_affinities{};
            for (const auto& replica : replicas) {
                ProcessAffinities<int> process_affinities{};
                for (const auto& [process_name, orig_cpu] : replica) {
                    if (conv_map.find(orig_cpu) != conv_map.end()) {
                        process_affinities.emplace(process_name, conv_map.at(orig_cpu));
                        cpus.set(conv_map.at(orig_cpu));
                    } else {
                        process_affinities.emplace(process_name, orig_cpu);
                        cpus.set(orig_cpu);
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
        name{name}, thread_map{}, region_map{}, characteristics_map{}, cpus{}
    {
        for (const auto& t : threads) {
            thread_map.emplace(t.first, cpu_nr_for_name(t.second));
            cpus.set(cpu_nr_for_name(t.second));
        }

        for (const auto& [region_name, replicas] : region_threads) {
            ReplicaAffinities<int> replica_affinities{};
            for (const auto& replica : replicas) {
                ProcessAffinities<int> process_affinities{};
                for (const auto& [process_name, affinity] : replica) {
                    process_affinities.emplace(process_name, cpu_nr_for_name(affinity));
                    cpus.set(cpu_nr_for_name(affinity));
                }
                replica_affinities.push_back(process_affinities);
            }
            region_map.emplace(region_name, replica_affinities);
        }

        for (const auto& c : characteristics) {
            characteristics_map.emplace(c.first, std::stod(c.second));
        }
    }

    CPUList cpu(const std::string& thread) const
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

    std::vector<Mapping> equivalent_mappings() const
    {

        for (const auto& equiv : equivalences)  {
            if (equiv.is_in_equalence_class(cpus)) {
                std::vector<Mapping> result;

                for (const auto& conv_map : equiv.equivalent_mappings(cpus)) {
                    result.push_back(Mapping{*this, conv_map});
                }

                return result;
            }
        }

        throw std::runtime_error("Can't determine the mapping's equivalence class.");
    }

    const Equivalence& equivalence_class() const
    {
        for (const auto& equiv : equivalences) {
            if (equiv.is_in_equalence_class(cpus))
                return equiv;
        }

        throw std::runtime_error("Can't determine the mapping's equivalence class.");
    }

    bool is_valid(const KnobDescription& knob_description) {
        // Check validity of region mappings.
        for (const auto& [region_name, region_body] : knob_description.region_specifications) {
            auto max_nb_replicas = region_body.max_nb_replicas;
            // Search for the region in the region map.
            // If the region cannot be found in the region map, then the mapping does not respect the
            // knob description, returning false.
            auto region_map_entry = region_map.find(region_name);
            if (region_map_entry == region_map.end())
                return false;
            // Check if the maximum number of replicas has not been reached.
            auto nb_replicas = region_map_entry->second.size();
            if (max_nb_replicas != 0 && max_nb_replicas < nb_replicas)
                return false;
            // Check for cores/mapping consistency
            auto replicas = region_map_entry->second;
            for (const auto& replica : replicas) {
                if (!check_validity_process(region_body.process_specifications, replica))
                    return false;
            }
        }
        // Check validity of regular process mappings.
        return check_validity_process(knob_description.regular_process_specifications, thread_map);
    }

    bool check_validity_process(const KnobDescription::ProcessSpecifications &specifications, const ProcessAffinities<int> &process_affinities)
    {
        for (const auto& [process_name, core_affinities] : specifications) {
            auto thread_map_entry = process_affinities.find(process_name);
            if (thread_map_entry == process_affinities.end())
                return false;
            // Check core validity
            auto core_affinity = thread_map_entry->second;
            if (!core_affinities.empty()) {
                std::set<int> cpu_set{};
                std::for_each(core_affinities.begin(), core_affinities.end(),
                              [&cpu_set](const auto& affinity) {
                                  cpu_set.emplace(cpu_nr_for_name(affinity));
                              });
                if (cpu_set.find(core_affinity) == cpu_set.end())
                    return false;
            }
        }
        return true;
    }

};

#endif /* __MAPPING_H__ */
