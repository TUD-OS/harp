//
// Created by dylan on 06/08/2020.
//

#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

#include <string>
#include <memory>

namespace TETRiS {

    // Predefining classes.
    class Client;
    class ClientRequest;
    class ClientResponse;
    class PushRequest;
    class PushResponse;

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
         * Called from the push listener thread when a command is received.
         *
         * \param msg PushRequest received.
         * \return PushResponse to the request.
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

    /**
     * \brief TETRiS client interface.
     */
    class Client {
    public:
        virtual ~Client() = default;

        /**
         * \brief Binds a TETRiS feature to the TETRiS client.
         *
         * The binding procedure includes handshaking with the TETRiS server if the feature needs to subscribe to
         * push notifications.
         *
         * \param feature Pointer to the TETRiS feature.
         */
        virtual void bind(TETRiS::Feature *feature) = 0;

        /**
         * \brief Sends a client request to the TETRiS server.
         * \param msg Client request to send.
         * \return Response from the TETRiS server.
         */
        virtual ClientResponse send(const ClientRequest &msg) = 0;
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
