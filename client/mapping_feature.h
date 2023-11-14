#ifndef __MAPPING_FEATURE_H__
#define __MAPPING_FEATURE_H__

#include <stdexcept>
#pragma once

#include "proto/tetris.pb.h"
#include "feature.h"

#include "util/mapping.h"


namespace tetris {

// Forward definition of the TETRiS client class.
class Client;

using MappingUpdate = Mapping;
using ConversionMap = std::map<int, int>;
using MappingsInfo = ClientMessage::OperatingPointsInfo;

/**
 * \brief TETRiS Mapping Feature abstract class.
 *
 * A TETRiS Mapping feature is an extension of the normal mapping procedure that allows clients to dynamically extend
 * mapping information before they are sent to the TETRiS server and also react to potential mapping changes of the
 * client's mapping in order to implement more complex mapping features.
 */
class MappingFeature : public Feature
{
public:
    /**
     * \brief Builds a feature.
     */
    MappingFeature() = default;

    /**
     * \brief Virtual default destructor.
     */
    virtual ~MappingFeature() = default;

    /**
     * \brief Handle mapping changes for the client
     *
     * When the client gets an updated mapping info, handle the changes accordingly.
     * 
     * \param mapping The updated and converted mapping received from the TETRiS server.
     * \param conv The conversion map that contains the information on which CPUs should be used.
     *             The conversion map is already applied to the given mapping argument.
     */
    virtual void mapping_update(const MappingUpdate &mapping, const ConversionMap &conv)
    {}

    /**
     * \brief Extend mapping information before sending them to the TETRiS server
     *
     * Update or extend mapping information before the client sends them to the TETRiS server.
     *
     * \param mapping_data current state of the mapping information
     * \return true if mapping information was updated, false otherwise
     */
    virtual bool extend_mapping(MappingsInfo &mappings)
    {
        return false;
    }

public:
    /* Provide default implementation for the original Feature interface for MappingFeatures
     * that don't want to communicate with the server */
    ClientResponse handle(const ServerMessage &msg) override
    {
        throw std::runtime_error("Not implemented!\n");
    }


    FeatureID handshake() override
    {
        throw std::runtime_error("Not implemented!\n");
    }
};

} /* namespace tetris */

#endif // __MAPPING_FEATURE_H__
