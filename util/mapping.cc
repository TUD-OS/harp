#include "util/mapping.h"

#include "util/platform/platform.h"
#include "util/operating_point.h"

namespace {

int cpu_nr_for_name(const tetris::Platform &platform, const std::string &name) {
  auto t_ptr = platform.FindCPUThread(name);
  if (t_ptr) {
    return t_ptr->GetID();
  };
  throw std::runtime_error("Unknown CPU thread");
}

} /* Anonymous namespace */


namespace tetris {

Mapping::Mapping(const Mapping& base, const std::map<int, int>& conv_map) :
        _platform{base._platform}, name{base.name}, thread_map{}, region_map{},
      characteristics_map{base.characteristics_map}, cpus{}
{
    // Convert thread CPU affinity.
    for (const auto& [name, orig_t_cpu] : base.thread_map) {
        if (conv_map.find(orig_t_cpu) != conv_map.end()) {
            auto new_cpu = conv_map.at(orig_t_cpu);
            thread_map.emplace(name, new_cpu);
            cpus.Set(new_cpu);
        } else {
            thread_map.emplace(name, orig_t_cpu);
            cpus.Set(orig_t_cpu);
        }

    }

    // Convert thread CPU affinity in parallel regions.
    for (const auto& [region_name, replicas] : base.region_map) {
        ReplicaAffinities<int> replica_affinities{};
        for (const auto& replica : replicas) {
            ProcessAffinities<int> process_affinities{};
            for (const auto& [process_name, orig_r_cpu] : replica) {
                if (conv_map.find(orig_r_cpu) != conv_map.end()) {
                    auto new_cpu = conv_map.at(orig_r_cpu);
                    process_affinities.emplace(process_name, new_cpu);
                    cpus.Set(new_cpu);
                } else {
                    cpus.Set(orig_r_cpu);
                    process_affinities.emplace(process_name, orig_r_cpu);
                }
            }
            replica_affinities.push_back(process_affinities);
        }
        region_map.emplace(region_name, replica_affinities);
    }
}

Mapping::Mapping(const Platform& platform, const std::string& name,
            const std::vector<std::pair<std::string, std::string>>& threads,
            const RegionAffinities<std::string>& regions,
            const std::vector<std::pair<std::string, std::string>>& characteristics) :
        _platform{platform}, name{name}, thread_map{}, characteristics_map{}, cpus{}
{
    for (const auto& t : threads) {
        thread_map.emplace(t.first, cpu_nr_for_name(platform, t.second));
        cpus.Set(cpu_nr_for_name(platform, t.second));
    }

    for (const auto& [region_name, replicas] : regions) {
        ReplicaAffinities<int> replica_affinities{};
        for (const auto& replica : replicas) {
            ProcessAffinities<int> process_affinities{};
            for (const auto& [process_name, affinity] : replica) {
                process_affinities.emplace(process_name, cpu_nr_for_name(platform, affinity));
                cpus.Set(cpu_nr_for_name(platform, affinity));
            }

            replica_affinities.push_back(process_affinities);
        }
        region_map.emplace(region_name, replica_affinities);
    }

    for (const auto& c : characteristics) {
        characteristics_map.emplace(c.first, std::stod(c.second));
    }
}

OperatingPoint Mapping::op() const
{
  auto core_set = _platform.ToCPUCoreSet(cpus);
  auto cores_count = _platform.GetCoreCountPerType(core_set);
  OperatingPoint::Configuration config{name, cpus, cores_count};
  double utility = characteristics_map.at("utility");
  double power = characteristics_map.at("power");
  OperatingPoint::Metrics metrics{utility, power};
  return OperatingPoint{config, metrics};
}

} /* namespace tetris */
