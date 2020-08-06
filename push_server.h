//
// Created by dylan on 06/08/2020.
//

#ifndef __PUSH_SERVER_H__
#define __PUSH_SERVER_H__

#include "tetris_feature.h"
#include "socket.h"

namespace TETRiS {
    /**
     * \brief TETRiS Push Server.
     *
     * A push server runs in a TETRiS client instance in order to receive request from the TETRiS server. It handles
     * logic for redistributing message to the corresponding TETRiS feature.
     */
    class PushServer {
    public:
        /**
         * \brief Builds a PushServer.
         *
         * Behind the scene, a thread is launched to listen on the push server.
         *
         * \param [in] socket_path Path to the socket to run the server on.
         */
        explicit PushServer(const std::string &socket_path);

        /**
         * \brief Adds a TETRiS feature with its id to the subscribers map.
         * \param feature_id Feature ID.
         * \param feature Pointer to feature.
         */
        void add_subscriber(const FeatureID &feature_id, Feature *feature);

        /**
         * \brief Forwards the push request to the specified feature.
         * \param request Request to forward.
         * \return Forwarded response.
         */
        PushResponse forward(const PushRequest &request) const;

    private:
        /**
         * \brief Thread listener.
         * \return None.
         */
        static void *listening(void *args);

        /// \brief Push server socket.
        Socket server;

        /// \brief Subscribers.
        std::map<FeatureID, Feature *> subscribers;
    };
}

#endif //__PUSH_SERVER_H__
