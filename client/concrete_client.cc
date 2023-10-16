//
// Created by dylan on 10/08/2020.
//

#include "concrete_client.h"
#include "client.h"
#include "util/protobuf_util.h"

#include <memory>
#include <sstream>

namespace tetris {

ConcreteClient::ConcreteClient(const std::string &server_socket_path)
        : Client(), _push_message_listener(get_push_listener_socket_path(), this),
          _managed(false), _communication_mutex()
{
    _logger = debug::Logger::get();
    try {
        _tetris_server_connection.connect(server_socket_path);
        _managed = register_client();
    } catch (std::exception &e) {
        _logger->info("No TETRiS server, TETRiS is unused.\n");
        _managed = false;
    }
}

void ConcreteClient::bind(Feature *feature)
{
    // If the client is not connected to the server, do not bind the feature.
    if (!_managed)
        return;
    // Bind the client to the feature.
    feature->accept(this);
    // If it needs a handshake, perform it.
    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();
        // Add the feature to the subscriber lists of the push listener.
        _push_message_listener.add_subscriber(feature_id, feature);
    }
}

void ConcreteClient::bind(MappingFeature *feature)
{
    if (!_managed)
        return;

    feature->accept(this);

    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();

        _push_message_listener.add_subscriber(feature_id, feature);
    }

    _mapping_features.push_back(feature);
}

ServerResponse ConcreteClient::send(const ClientMessage &message)
{
    ServerResponse response{};
    _communication_mutex.lock();
    protobuf_util::Send(_tetris_server_connection.locked(), message);
    protobuf_util::Receive(_tetris_server_connection.locked(), response);
    _communication_mutex.unlock();
    return response;
}

std::string ConcreteClient::get_push_listener_socket_path()
{
    std::stringstream string_stream{};
    string_stream << "/tmp/tetris_push_listener_" << getpid();
    return string_stream.str();
}

ClientResponse ConcreteClient::handle(const ServerMessage &msg)
{
    if (msg.has_activated_op_info()) {
        /* Call all mapping_features with the new mapping, so that they can adapt */
    }

    ClientResponse response{};
    response.set_type(tetris::ClientResponse::ACKNOWLEDGE);

    return response;
}

std::map<std::string, std::string> retrieve_env_variables()
{
    std::map<std::string, std::string> env_variables{};
    for (auto &env : {"TETRIS_MAPPING_TYPE", "TETRIS_MAPPING_TYPE", "TETRIS_COMPARE_CRITERIA",
                      "TETRIS_COMPARE_MORE_IS_BETTER", "TETRIS_PREFERRED_MAPPING", "TETRIS_FILTER_CRITERIA"}) {
        if (getenv(env))
            env_variables.emplace(env, getenv(env));
    }
    return env_variables;
}

bool ConcreteClient::register_client()
{
    RegistrationRequest request{};

    /* Send the new-client message to the server. */
    request.set_pid(getpid());
    char exec[512];
    memset(exec, 0, sizeof(exec));
    readlink("/proc/self/exe", exec, sizeof(exec));
    request.set_exec(exec);

    // Send the command.
    try {
        RegistrationResponse response{};
        _communication_mutex.lock();
        protobuf_util::Send(_tetris_server_connection.locked(), request);
        protobuf_util::Receive(_tetris_server_connection.locked(), response);
        _communication_mutex.unlock();

        // Process the TETRiS server response.
        _logger->info("TETRIS-ID: %d\n", response.id());
        return true;
    } catch (std::exception &e) {
        return false;
    }
}

}
