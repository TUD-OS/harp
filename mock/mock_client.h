#ifndef __MOCK_CLIENT_H__
#define __MOCK_CLIENT_H__

#include <memory>
#pragma once

#include "client/client.h"
#include "client/mapping_feature.h"
#include "util/connection.h"
#include "util/debug_util.h"
#include "util/platform/platform.h"

#include "mock_message_listener.h"

#include <vector>
#include <string>
#include <mutex>


namespace tetris {

class MockClient : public Client {
   public:
    /**
     * \brief Builds a concrete client.
     */
    explicit MockClient(const std::string &server_socket_path, const std::string &platform_desc_path);

    /**
     * \copydoc bind(TETRiS::Feature *feature)
     */
    void bind(tetris::Feature *feature) override;

    /**
     * \copydoc bind(TETRiS::MappingFeature *feature)
     */
    void bind(tetris::MappingFeature *feature) override;

    /**
     * \copydoc send(const ClientMessage &msg)
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

    ClientResponse handle(const MockServerMessage &msg);

   public:
    void add_thread(int tid);
    void del_thread(int tid);

   private:
    /**
     * \brief Sends a RegistrationRequest to the TETRiS server.
     * \return true if the TETRiS server handles this client, false otherwise.
     */
    bool register_client();

    /* The platform description of the system we run on */
    std::unique_ptr<Platform> _platform;
    std::unique_ptr<Mapping> _active_mapping;

    /// \brief Push message listener, listening for requests from the TETRiS server.
    MockMessageListener _mock_message_listener;

    /// \brief Permanent connection to the TETRiS server.
    Connection _tetris_server_connection;

    /// \brief Pointer to the debug logger.
    debug::LoggerPtr _logger;

    /// \brief If true, the client is connected to the TETRiS server and is managed.
    bool _managed;

    std::vector<MappingFeature*> _mapping_features;
};

} /* namespace tetris */

#endif /* __MOCK_CLIENT_H__ */
