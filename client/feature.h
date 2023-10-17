//
// Created by dylan on 12/08/2020.
//

#ifndef __FEATURE_H__
#define __FEATURE_H__

#pragma once

#include "proto/tetris.pb.h"

namespace tetris {

// Forward definition of the TETRiS client class.
class Client;

/// \brief A feature ID is used to forward request from the push listener to the corresponding feature.
using FeatureID = uint32_t;

/**
 * \brief TETRiS Feature abstract class.
 *
 * A TETRiS feature is a user of the TETRiS system. A feature is bound to the TETRiS client which runs on
 * the application side. This binding procedure allows the feature to communicate with the TETRiS server and to
 * receive push notifications if needed.
 */
class Feature
{
public:
    /**
     * \brief Builds a feature.
     */
    Feature();

    /**
     * \brief Virtual default destructor.
     */
    virtual ~Feature() = default;

    /**
     * \brief Accepts the TETRiS client to bind the feature.
     * \param client Pointer to the client.
     */
    void accept(Client *client);

    /**
     * \brief Checks if the feature is bounded to a TETRiS client.
     *
     * This method can be used to check if the feature is connected.
     *
     * \return true if bounded, false otherwise.
     */
    bool is_bounded() const;

    /**
     * \brief Handle a mesage from the TETRiS server for this feature.
     *
     * Called from the push listener thread when a command is received.
     *
     * \param request PushRequest received.
     * \return PushResponse associated to the request.
     */
    virtual ClientResponse handle(const ServerMessage &msg) = 0;

    /**
     * \brief Checks if the feature needs a handshake.
     *
     * A handshake is only needed in order to subscribe to push notifications from the server.
     *
     * \return true if the feature needs a handshake, false otherwise.
     */
    virtual bool need_handshake() const = 0;

    /**
     * \brief Performs a handshake with the TETRiS server.
     * \return Allocated feature ID.
     */
    virtual FeatureID handshake() = 0;

protected:
    /**
     * \brief Gets the TETRiS client instance.
     * \return Pointer to the TETRiS client.
     */
    Client *get_client();

private:
    /// \brief Bounded TETRiS client instance.
    Client *_client;
};
}

#endif //__FEATURE_H__
