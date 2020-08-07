//
// Created by dylan on 06/08/2020.
//

#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

#include "proto/Tetris.pb.h"
#include "push_message_listener.h"
#include "connection.h"
#include "debug_util.h"

namespace TETRiS {
    // Predefining Feature.
    class Feature;

    class Client {
    public:
        /**
         * \brief Builds a concrete client.
         */
        explicit Client(const std::string &server_socket_path);

        /**
         * \brief Binds a TETRiS feature to the TETRiS client.
         *
         * The binding procedure includes handshaking with the TETRiS server if the feature needs to subscribe to
         * push notifications.
         *
         * \param feature Pointer to the TETRiS feature.
         */
        virtual void bind(TETRiS::Feature *feature);

        /**
         * \brief Sends a client request to the TETRiS server.
         * \param msg Client request to send.
         * \return Response from the TETRiS server.
         */
        virtual ClientResponse send(const ClientRequest &msg);

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
        std::unique_ptr<Connection> _tetris_server_connection;

        /// \brief Pointer to the debug logger.
        debug::LoggerPtr _logger;

        /// \brief If true, the client is connected to the TETRiS server and is managed.
        bool _managed;
    };

    /**
     * \brief TETRiS Client singleton.
     */
    class ClientProvider {
    public:
        /**
         * \brief Initializes a TETRiS client.
         *
         * \param server_socket_path TETRiS server socket path.
         */
        static void initialize(const std::string &socket_path);

        /**
         * \brief Finalizes the TETRiS client.
         */
        static void finalize();

        /**
         * \brief Gets the TETRiS client instance.
         * \return Pointer to the TETRiS client instance.
         */
        static Client *get_instance();

    private:
        /// \brief Client instance.
        static std::unique_ptr<Client> _instance;

    };
}

#endif // __TETRIS_CLIENT_H__
