#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "feature.h"
#include "mapping_feature.h"

#include <memory>
#include <string>

namespace tetris {

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
    virtual void bind(tetris::Feature *feature) = 0;

    /**
     * \brief Binds a TETRiS Mapping feature to the TETRiS client.
     *
     * Mapping features are special extensions of clients that allow the client to do more advanced mapping
     * changes. These features will be activated whenever the mapping of the client changes. If necessary,
     * they might register also with the TETRiS server
     *
     * \param mapping_feature Pointer to the TETRiS Mapping feature.
     */
    virtual void bind(tetris::MappingFeature *mapping_feature) = 0;

    /**
     * \brief Sends a client request to the TETRiS server.
     * \param msg Client request to send.
     * \return Response from the TETRiS server.
     */
    virtual ServerResponse send(const ClientMessage &msg) = 0;

    /**
     * \brief Handle messages from the TETRiS server that need to be directly handled by the client
     *        (send with featureID 0)
     *
     * \param msg Message from the server that needs to be handled.
     * \return Response that should be sent back to the server.
     */
    virtual ClientResponse handle(const ServerMessage &msg) = 0;
};

/**
 * \brief TETRiS client singleton.
 *
 * Provides a instance of a ConcreteClient.
 */
class ClientProvider {
public:
    /**
     * \brief Initializes a TETRiS client.
     *
     * \param server_socket_path Path of the TETRiS server socket.
     */
    static void initialize(const std::string &socket_path,
                           const std::string &platform_path,
                           const std::string &mapping_path,
                           bool mapping_coarse_grained);

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

} // namespace tetris

#endif // __CLIENT_H__
