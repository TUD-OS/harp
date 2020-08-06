//
// Created by dylan on 06/08/2020.
//

#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

#include "proto/Tetris.pb.h"

namespace TETRiS {
    class Feature;

    /**
     * \brief TETRiS Client singleton.
     */
    class Client {
    public:
        /**
         * \brief Initializes the TETRiS client library.
         *
         * Behind the scene, a thread is launched to listen on the push server.
         */
        static void initialize();

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

    private:
        /// \brief Client instance.
        static std::unique_ptr<Client> _instance;
    };
}

#endif // __TETRIS_CLIENT_H__
