//
// Created by dylan on 18/09/2020.
//

#include "knob_description.h"

KnobDescription::KnobDescription(const nlohmann::json &json_knob_description) : valid{true} {
    // Explore the json structure and grab every information from the knob description.
    for (const auto &process : json_knob_description["processes"]) {
        if (process["type"] == "process") {
            /* Regular process. Get the name and potential core constraints. */
            std::string process_name = process["name"];
            std::set<std::string> process_cores{};
            if (process.find("cores") != process.end()) {
                for (const auto &cores : process["cores"])
                    process_cores.emplace(cores.get<std::string>());
            }
            regular_process_specifications.emplace(process_name, process_cores);
        } else if (process["type"] == "DLP") {
            /* Parallel region. Get the name and body description. */
            std::string region_name = process["name"];
            RegionBodySpecification body_specification{};
            for (const auto& process_replica : process["body"]) {
                /* Regular process as a replica. Get the name and potential core constraints. */
                std::string process_name = process_replica["name"];
                std::set<std::string> process_cores{};
                if (process_replica.find("cores") != process_replica.end()) {
                    for (const auto &cores : process_replica["cores"])
                        process_cores.emplace(cores.get<std::string>());
                }
                body_specification.process_specifications.emplace(process_name, process_cores);
            }
            /* Get the maximum number of replicas if defined. */
            if (process.find("max_replicas") != process.end())
                body_specification.max_replicas = process["max_replicas"].get<unsigned int>();
            region_specifications.emplace(region_name, body_specification);
        }
    }
    // If there are no processes at all, the knob description is incorrect.
    if (region_specifications.empty() && regular_process_specifications.empty())
        valid = false;
    // Grab every characteristic information.
    for (const auto &characteristic : json_knob_description["characteristics"].items()) {
        auto characteristic_name = characteristic.key();
        auto characteristic_compare = characteristic.value().get<std::string>();
        if (characteristic_compare == "less_is_better")
            characteristic_specifications.emplace(characteristic_name, CharacteristicComparison::LessIsBetter);
        else if (characteristic_compare == "more_is_better")
            characteristic_specifications.emplace(characteristic_name, CharacteristicComparison::MoreIsBetter);
        else
            // The characteristic specification is incorrect in regard to the given comparison type.
            valid = false;
    }
}

bool KnobDescription::is_valid() const
{
    return valid;
}
