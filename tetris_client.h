#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

#include <string>
#include <memory>

namespace TETRiS {

/// \brief Path of the TETRiS server socket.
static const std::string SERVER_SOCKET{"/tmp/tetris_socket"};

// Predefining classes.
class Client;

class ClientRequest;

class ClientResponse;

class PushRequest;

class PushResponse;

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
     * \brief Forwards a message to the feature.
     *
     * Called from the push listener thread when a command is received.
     *
     * \param request PushRequest received.
     * \return PushResponse associated to the request.
     */
    virtual PushResponse forward(const PushRequest &request) = 0;

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
    Client *get_client() const;

private:
    /// \brief Bounded TETRiS client instance.
    Client *_client;
};

/**
 * \brief TETRiS client interface.
 */
class Client
{
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
 * \brief TETRiS client singleton.
 *
 * Provides a instance of a ConcreteClient.
 */
class ClientProvider
{
public:
    /**
     * \brief Initializes a TETRiS client.
     *
     * \param server_socket_path Path of the TETRiS server socket.
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
