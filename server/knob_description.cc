//
// Created by dylan on 18/09/2020.
//

#include "knob_description.h"

KnobDescription::KnobDescription(const nlohmann::json& json_knob_description)
    : valid{true} {
  // Explore the json structure and grab every information from the knob
  // description.
  for (const auto& process : json_knob_description["processes"]) {
    if (process["type"] == "process") {
      /* Regular process. Get the name and potential core constraints. */
      std::string process_name = process["name"];
      std::set<std::string> process_cores{};
      if (process.find("cores") != process.end()) {
        for (const auto& cores : process["cores"])
          process_cores.emplace(cores.get<std::string>());
      }
      regular_process_specifications.emplace(process_name, process_cores);
    } else if (process["type"] == "DLP") {
      /* Parallel region. Get the name and body description. */
      std::string region_name = process["name"];
      RegionBodySpecification body_specification{};
      for (const auto& process_replica : process["body"]) {
        /* Regular process as a replica. Get the name and potential core
         * constraints. */
        std::string process_name = process_replica["name"];
        std::set<std::string> process_cores{};
        if (process_replica.find("cores") != process_replica.end()) {
          for (const auto& cores : process_replica["cores"])
            process_cores.emplace(cores.get<std::string>());
        }
        body_specification.process_specifications.emplace(process_name,
                                                          process_cores);
      }
      /* Get the maximum number of replicas if defined. */
      if (process.find("max_replicas") != process.end())
        body_specification.max_replicas =
            process["max_replicas"].get<unsigned int>();
      region_specifications.emplace(region_name, body_specification);
    }
  }
  // If there are no processes at all, the knob description is incorrect.
  if (region_specifications.empty() && regular_process_specifications.empty())
    valid = false;
  // Grab every characteristic information.
  for (const auto& characteristic :
       json_knob_description["characteristics"].items()) {
    auto characteristic_name = characteristic.key();
    auto characteristic_compare = characteristic.value().get<std::string>();
    if (characteristic_compare == "less_is_better")
      characteristic_specifications.emplace(
          characteristic_name, CharacteristicComparison::LessIsBetter);
    else if (characteristic_compare == "more_is_better")
      characteristic_specifications.emplace(
          characteristic_name, CharacteristicComparison::MoreIsBetter);
    else
      // The characteristic specification is incorrect in regard to the given
      // comparison type.
      valid = false;
  }
}

bool KnobDescription::is_valid() const { return valid; }

/**
 * \brief Checks if a mapping is valid based on the current knob description.
 *
 * \param mapping The mapping to be validated.
 * \return true If the mapping is valid according to this knob description.
 * \return false Otherwise.
 */
bool KnobDescription::is_mapping_valid(const Mapping& mapping) {
  // Check the validity of the region mappings.
  for (const auto& [region_name, region_body] : this->region_specifications) {
    // The maximum number of replicas allowed for this region.
    auto max_nb_replicas = region_body.max_replicas;

    // Search for the region in the region map.
    auto region_map_entry = mapping.region_map.find(region_name);

    // If the region is not in the map, the mapping is invalid.
    if (region_map_entry == mapping.region_map.end()) return false;

    // Check that the number of replicas does not exceed the maximum.
    auto nb_replicas = region_map_entry->second.size();
    if (max_nb_replicas != 0 && max_nb_replicas < nb_replicas) return false;

    // Check that each replica is valid according to the process specifications.
    auto replicas = region_map_entry->second;
    for (const auto& replica : replicas) {
      if (!check_validity_process(region_body.process_specifications, replica))
        return false;
    }
  }

  // Check the validity of the characteristic mappings.
  for (const auto& characteristic_item : this->characteristic_specifications) {
    auto characteristic_name = characteristic_item.first;
    auto characteristic_map_entry =
        mapping.characteristics_map.find(characteristic_name);

    // If the characteristic is not in the map, the mapping is invalid.
    if (characteristic_map_entry == mapping.characteristics_map.end())
      return false;
  }

  // Check the validity of the regular process mappings.
  return check_validity_process(this->regular_process_specifications,
                                mapping.thread_map);
}

/**
 * @brief Checks the validity of a process according to its specifications and
 * affinities.
 *
 * @param specifications The specifications for the process.
 * @param process_affinities The affinities for the process.
 * @return true If the process is valid according to the specifications and
 * affinities.
 * @return false Otherwise.
 */
bool KnobDescription::check_validity_process(
    const ProcessSpecifications& specifications,
    const ProcessAffinities<int>& process_affinities) {
  // Check each process in the specifications.
  for (const auto& [process_name, core_affinities] : specifications) {
    auto thread_map_entry = process_affinities.find(process_name);

    // If the process is not in the affinities, it is invalid.
    if (thread_map_entry == process_affinities.end()) return false;

    // Check that the core affinity is valid.
    auto core_affinity = thread_map_entry->second;
    if (!core_affinities.empty()) {
      // Create a set of valid CPU numbers from the affinities.
      std::set<int> cpu_set{};
      std::for_each(core_affinities.begin(), core_affinities.end(),
                    [&cpu_set](const auto& affinity) {
                      cpu_set.emplace(cpu_nr_for_name(
                          affinity));  // cpu_nr_for_name should be accessible
                    });

      // If the core affinity is not in the set of valid CPU numbers, it is
      // invalid.
      if (cpu_set.find(core_affinity) == cpu_set.end()) return false;
    }
  }
  return true;
}
