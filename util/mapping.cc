#include "util/mapping.h"

#include "util/platform/platform.h"

namespace {

int cpu_nr_for_name(const Platform &platform, const std::string &name) {
  auto t_ptr = platform.FindCPUThread(name);
  if (t_ptr) {
    return t_ptr->GetID();
  };
  throw std::runtime_error("Unknown CPU thread");
}

} /* Anonymous namespace */


Mapping::Mapping(const Mapping& base, const std::map<int, int>& conv_map) :
        _platform{base._platform}, name{base.name}, thread_map{}, region_map{},
      characteristics_map{base.characteristics_map}, cpus{}
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

Mapping::Mapping(
    const Platform &platform,
    const tetris::ClientMessage::MappingsInfo::MappingData &mapping_data)
    : _platform{platform}, name{}, thread_map{}, characteristics_map{}, cpus{} {
  for (int j = 0; j < mapping_data.characteristics_size(); j++) {
    auto cur_c = mapping_data.characteristics(j);
    characteristics_map[cur_c.name()] = cur_c.value();
  }

  for (int j = 0; j < mapping_data.threads_size(); j++) {
    auto cur_t = mapping_data.threads(j);
    thread_map[cur_t.name()] = cpu_nr_for_name(platform, cur_t.cpu());
  }
}

Mapping::Mapping(const Platform& platform, const std::string& name,
            const std::vector<std::pair<std::string, std::string>>& threads,
            const RegionAffinities<std::string>& region_threads,
            const std::vector<std::pair<std::string, std::string>>& characteristics) :
        _platform{platform}, name{name}, thread_map{}, characteristics_map{}, cpus{}
{
    for (const auto& t : threads) {
        thread_map.emplace(t.first, cpu_nr_for_name(platform, t.second));
        cpus.Set(cpu_nr_for_name(platform, t.second));
    }

    for (const auto& [region_name, replicas] : region_threads) {
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
