#ifndef __CONCRETE_CLIENT_H__
#define __CONCRETE_CLIENT_H__

#include "client.h"
#include "push_message_listener.h"
#include "util/connection.h"
#include "util/debug_util.h"

namespace tetris {

/**
 * \brief TETRiS concrete client.
 *
 * This implementation embeds a basic socket connection with the TETRiS server and a push listener.
 */
class ConcreteClient : public Client
{
public:
    /**
     * \brief Builds a concrete client.
     */
    explicit ConcreteClient(const std::string &server_socket_path);

    /**
     * \copydoc bind(TETRiS::Feature *feature)
     */
    void bind(tetris::Feature *feature) override;

    /**
     * \copydoc send(const ClientRequest &msg)
     */
    PullResponse send(const PullRequest &msg) override;

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

private:
    /**
     * \brief Sends a NewClient command to the TETRiS server.
     * \return true if the TETRiS server handles this client, false otherwise.
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

    /// \brief Mutex preventing multiple features to send/receive through the server socket at the same time.
    std::mutex _communication_mutex;
};
}

#endif //__CONCRETE_CLIENT_H__
