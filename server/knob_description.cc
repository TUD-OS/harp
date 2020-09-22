//
// Created by dylan on 18/09/2020.
//

#include "knob_description.h"

KnobDescription::KnobDescription(const nlohmann::json &json_knob_description) {
    // Explore the json structure and grab every information from the knob description.
    for (const auto &process : json_knob_description) {
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
                body_specification.max_nb_replicas = process["max_replicas"].get<unsigned int>();
            region_specifications.emplace(region_name, body_specification);
        }
    }
}