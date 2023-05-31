//
// Created by dylan on 18/09/2020.
//

#ifndef __KNOB_DESCRIPTION_H__
#define __KNOB_DESCRIPTION_H__

#include <map>
#include <set>
#include <string>
#include <iostream>

#include "util/json.h"

class KnobDescription
{
public:
    /// \brief Defines a set of CPU name that a process should be constrained to.
    /// If empty, the process can be mapped everywhere.
    using CpuSpecification = std::set<std::string>;
    /// \brief Defines specifications for regular processes.
    using ProcessSpecifications = std::map<std::string, CpuSpecification>;
    /// \brief Defines the specification for a region.
    struct RegionBodySpecification
    {
        /// \brief Regular process specifications.
        ProcessSpecifications process_specifications;
        /// \brief Maximum number of replicas. If this field equals zero then there are no maximum number of replicas.
        unsigned int max_replicas{0};
    };
    /// \brief Defines specifications for parallel regions.
    using RegionSpecifications = std::map<std::string, RegionBodySpecification>;

    /**
     * \brief Specify if a characteristic is better if the value is lower or the opposite.
     */
    enum class CharacteristicComparison {
        LessIsBetter,
        MoreIsBetter
    };

    /// \brief Defines specifications for characteristics
    using CharacteristicSpecifications = std::map<std::string, CharacteristicComparison>;

    /// \brief Regular process specifications.
    ProcessSpecifications regular_process_specifications;
    /// \brief Parallel region specifications.
    RegionSpecifications region_specifications;
    /// \brief Mapping characteristic specifications.
    CharacteristicSpecifications characteristic_specifications;
    /// \brief Register if the parsed knob description is valid.

    /**
     * \brief Builds a knob description from a json structure.
     * \param json_mapping json structure to parse.
     */
    explicit KnobDescription(const nlohmann::json &json_knob_description);

    /**
     * \return true if the knob description is valid.
     */
    bool is_valid() const;

private:
    bool valid;
};

#endif //__KNOB_DESCRIPTION_H__
