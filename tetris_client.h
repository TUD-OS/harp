#ifndef __TETRIS_CLIENT_H__
#define __TETRIS_CLIENT_H__

#include "proto/tetris.pb.h"
#include "feature.h"

#include <string>
#include <memory>

namespace tetris {

/// \brief Path of the TETRiS server socket.
static const std::string SERVER_SOCKET{"/tmp/tetris_socket"};

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
    virtual void bind(tetris::Feature *feature) = 0;

    /**
     * \brief Sends a client request to the TETRiS server.
     * \param msg Client request to send.
     * \return Response from the TETRiS server.
     */
    virtual PullResponse send(const PullRequest &msg) = 0;
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
