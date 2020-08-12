//
// Created by dylan on 06/08/2020.
//

#ifndef __PUSH_MESSAGE_LISTENER_H__
#define __PUSH_MESSAGE_LISTENER_H__

#include <map>
#include "tetris_client.h"
#include "socket.h"

namespace tetris {
    /**
     * \brief TETRiS Push Message Listener.
     *
     * A push message listener runs in a TETRiS client instance in order to receive request from the TETRiS server.
     * It handles the logic responsible for the redistribution of message to TETRiS features.
     */
    class PushMessageListener {
    public:
        /**
         * \brief Builds a PushMessageListener.
         *
         * Behind the scene, a thread is launched to listen on the push message listener.
         *
         * \param [in] socket_path Path to the socket to run the listener on.
         */
        explicit PushMessageListener(const std::string &socket_path);

        /**
         * \brief Destroys the push message listener, closing the socket and joining the listener thread.
         */
        ~PushMessageListener();

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
         * \brief Callback function call in listening thread.
         * \return None.
         */
        static void *listening(void *args);

        /// \brief Push message listening socket.
        Socket _listening_socket;

        /// \brief Listener thread id.
        pthread_t _listener_thread{};

        /// \brief Subscribers.
        std::map<FeatureID, Feature *> _subscribers;
    };
}

#endif //__PUSH_MESSAGE_LISTENER_H__
