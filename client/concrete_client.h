#ifndef __CONCRETE_CLIENT_H__
#define __CONCRETE_CLIENT_H__

#include "client.h"
#include "mapping_feature.h"
#include "push_message_listener.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "util/platform/platform.h"

#include <memory>
#include <string>
#include <vector>

namespace tetris {

/**
 * \brief TETRiS concrete client.
 *
 * This implementation embeds a basic socket connection with the TETRiS server
 * and a push listener.
 */
class ConcreteClient : public Client {
public:
    /**
     * \brief Builds a concrete client.
     */
    explicit ConcreteClient(const std::string &server_socket_path,
                            const std::string &platform_desc_path,
                            const std::string &mapping_path,
                            bool mapping_coarse_grained);

    /**
     * \copydoc bind(TETRiS::Feature *feature)
     */
    void bind(tetris::Feature *feature) override;

    /**
     * \copydoc bind(TETRiS::MappingFeature *feature)
     */
    void bind(tetris::MappingFeature *feature) override;

    /**
     * \copydoc send(const ClientRequest &msg)
     */
    ServerResponse send(const ClientMessage &msg) override;

    /**
     * \brief Builds the socket path for the push listener based on the application PID.
     * \return socket path for the push server.
     */
    static std::string get_push_listener_socket_path();

    /**
     * \brief Get the information whether this client is managed by the server or not.
     * \return is the client managed by the TETRiS server or not.
     */
    bool is_managed() { return _managed; }

    ClientResponse handle(const ServerMessage &msg) override;

private:
    /**
     * \brief Sends a RegistrationRequest to the TETRiS server.
     * \return true if the TETRiS server handles this client, false otherwise.
     */
    bool register_client();

    /// \brief Push message listener, listening for requests from the TETRiS server.
    PushMessageListener _push_message_listener;

    /// \brief Permanent connection to the TETRiS server.
    Connection _tetris_server_connection;

    /// \brief Pointer to the debug logger.
    debug::LoggerPtr _logger;

    /// \brief If true, the client is connected to the TETRiS server and is managed.
    bool _managed;

    /// \brief Mutex preventing multiple features to send/receive through the server socket at the same time.
    std::mutex _communication_mutex;

    /// \brief List of bind MappingFeatures
    std::vector<tetris::MappingFeature*> _mapping_features;

    std::unique_ptr<Platform> _platform;
    std::unique_ptr<Mapping> _active_mapping;

    /// \brief List of available Mappings for this client
    std::vector<Mapping> _mappings;

    /// \brief Flag whether mappings are coarse-grained
    bool _mapping_coarse_grained;
};
} // namespace tetris

#endif //__CONCRETE_CLIENT_H__
