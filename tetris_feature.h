//
// Created by dylan on 06/08/2020.
//

#ifndef __TETRIS_FEATURE_H__
#define __TETRIS_FEATURE_H__

#include "proto/Tetris.pb.h"
#include <cstdint>

namespace TETRiS {
    // Predefining Client.
    class Client;

    /// \brief A feature ID is used to forward request from the push server to the corresponding feature.
    using FeatureID = uint32_t;

    /**
     * \brief TETRiS Feature abstract class.
     *
     * A TETRiS feature is a user of the TETRiS system. A feature is bound to the TETRiS client that runs on
     * the application side. This binding procedure allows the feature to communicate with the TETRiS server and to
     * receive push notifications if needed.
     */
    class Feature {
    public:
        /**
         * \brief Builds a feature.
         */
        Feature();

        /**
         * \brief Accepts the TETRiS client to bind the feature.
         * \param client Pointer to the client.
         */
        void accept(Client *client);

        /**
         * \brief Checks if the feature is bound to a TETRiS client.
         *
         * This method can be used to check if the feature is connected.
         *
         * \return true if bounded, false otherwise.
         */
        bool is_bound() const;

        /**
         * \brief Forwards a message to the feature.
         *
         * Called from the push server thread when a command is received.
         *
         * \param msg PushRequest received.
         * \return Response to the request.
         */
        virtual PushResponse forward(const PushRequest &msg) const = 0;

        /**
         * \brief Checks if the feature needs a handshake.
         *
         * A handshake is only needed to subscribe to push notifications from the server.
         *
         * \return true if the feature needs a handshake, false otherwise.
         */
        virtual bool need_handshake() const = 0;

        /**
         * \brief Sends a request for a handshake with the TETRiS server.
         * \return Allocated feature ID.
         */
        virtual FeatureID handshake() = 0;

    protected:
        /**
         * \brief Gets the TETRiS client instance.
         * \return Pointer to the TETRiS client.
         */
        Client *get_client() const;

    private:
        /// \brief Bounded TETRiS client instance.
        Client *_client;
    };

}


#endif //__TETRIS_FEATURE_H__
