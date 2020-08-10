//
// Created by dylan on 10/08/2020.
//

#ifndef __CONCRETE_CLIENT_H__
#define __CONCRETE_CLIENT_H__

#include "tetris_client.h"
#include "push_message_listener.h"
#include "connection.h"
#include "debug_util.h"

namespace TETRiS {

    class ConcreteClient : public Client {
    public:
        /**
         * \brief Builds a concrete client.
         */
        explicit ConcreteClient(const std::string &server_socket_path);

        /**
         * \copydoc bind(TETRiS::Feature *feature)
         */
        void bind(TETRiS::Feature *feature) override;

        /**
         * \copydoc send(const ClientRequest &msg)
         */
        ClientResponse send(const ClientRequest &msg) override;

        /**
         * \brief Builds the socket path for the push server based on the application PID.
         * \return socket path for the push server.
         */
        static std::string get_push_listener_socket_path();

    private:
        /**
         * \brief Sends a NewClient command to the TETRiS server.
         * \return true if the TETRiS manaager handles this client.
         */
        bool send_new_client_command();

        /// \brief Push message listener, listening for requests from the TETRiS server.
        PushMessageListener _push_message_listener;

        /// \brief Permanent connection to the TETRiS server.
        Connection _tetris_server_connection;

        /// \brief Pointer to the debug logger.
        debug::LoggerPtr _logger;

        /// \brief If true, the client is connected to the TETRiS server and is managed.
        bool _managed;
    };
}


#endif //__CONCRETE_CLIENT_H__
