//
// Created by dylan on 06/08/2020.
//

#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

#include "proto/Tetris.pb.h"
#include "push_server.h"
#include "connection.h"

namespace TETRiS {
    // Predefining Feature.
    class Feature;

    /**
     * \brief TETRiS Client singleton.
     */
    class Client {
    public:
        /**
         * \brief Initializes the TETRiS client library.
         *
         * \param server_socket_path TETRiS server socket path.
         */
        static void initialize(const std::string &server_socket_path);

        /**
         * \brief Finalizes the TETRiS client library.
         */
        static void finalize();

        /**
         * \brief Gets the TETRiS client instance.
         * \return Pointer to the TETRiS client instance.
         */
        static Client *get_instance();

        /**
         * \brief Binds a TETRiS feature to the TETRiS client.
         *
         * The binding procedure includes handshaking with the TETRiS server if the feature needs to subscribe to
         * push notifications.
         *
         * \param feature Pointer to the TETRiS feature.
         */
        void bind(TETRiS::Feature *feature);

        /**
         * \brief Sends a client request to the TETRiS server.
         * \param msg Client request to send.
         * \return Response from the TETRiS server.
         */
        ClientResponse send(const ClientRequest &msg);

        /**
         * \brief Builds the socket path for the push server based on the application PID.
         * \return socket path for the push server.
         */
        static std::string get_push_server_socket_path();

    private:
        /**
         * \brief Default constructor for building Client.
         */
        Client(const std::string &server_socket_path);

        /// \brief Client instance.
        static std::unique_ptr<Client> _instance;

        /// \brief Push server, listening for requests from the TETRiS server.
        PushServer _push_server;

        /// \brief Permanent connection to the TETRiS server.
        std::unique_ptr<Connection> _tetris_server_connection;
    };
}

#endif // __TETRIS_CLIENT_H__
