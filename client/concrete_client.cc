//
// Created by dylan on 10/08/2020.
//

#include "concrete_client.h"
#include "client.h"
#include "util/protobuf_util.h"

#include <memory>
#include <sstream>

tetris::ConcreteClient::ConcreteClient(const std::string &server_socket_path)
        : Client(), _push_message_listener(get_push_listener_socket_path()),
          _managed(false), _communication_mutex()
{
    _logger = debug::Logger::get();
    try {
        _tetris_server_connection.connect(server_socket_path);
        _managed = send_new_client_command();
    } catch (std::exception &e) {
        _logger->info("No TETRiS server, TETRiS is unused.\n");
        _managed = false;
    }
}

void tetris::ConcreteClient::bind(tetris::Feature *feature)
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

tetris::PullResponse tetris::ConcreteClient::send(const tetris::PullRequest &request)
{
    tetris::PullResponse response{};
    _communication_mutex.lock();
    protobuf_util::Send(_tetris_server_connection.locked(), request);
    protobuf_util::Receive(_tetris_server_connection.locked(), response);
    _communication_mutex.unlock();
    return response;
}

std::string tetris::ConcreteClient::get_push_listener_socket_path()
{
    std::stringstream string_stream{};
    string_stream << "/tmp/tetris_push_listener_" << getpid();
    return string_stream.str();
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

bool tetris::ConcreteClient::send_new_client_command()
{

    auto env_variables = retrieve_env_variables();

    PullRequest request{};
    request.set_type(PullRequest::TETRIS_NEW_CLIENT);
    auto new_client_message = request.mutable_new_client();

    /* Send the new-client message to the server. */
    new_client_message->set_pid(getpid());
    char exec[100];
    memset(exec, 0, sizeof(exec));
    readlink("/proc/self/exe", exec, sizeof(exec));
    new_client_message->set_exec(exec);

    bool dynamic_client = false;
    auto mapping_type = env_variables.find("TETRIS_MAPPING_TYPE");
    if (mapping_type != env_variables.end()) {
        if (mapping_type->second == "DYNAMIC") {
            _logger->info("Use dynamic/CFS mapping.\n");
            dynamic_client = true;
        } else if (mapping_type->second == "STATIC") {
            _logger->info("Use static TETRiS mapping.\n");
        } else {
            _logger->warning("Unknown mapping type: %s\n", mapping_type->second);
        }
    }
    new_client_message->set_mapping_type(dynamic_client ? tetris::NewClient::DYNAMIC : tetris::NewClient::STATIC);

    auto compare_criteria = env_variables.find("TETRIS_COMPARE_CRITERIA");
    if (compare_criteria != env_variables.end()) {
        _logger->info("Use given compare criteria -- %s.\n", compare_criteria->second);
        new_client_message->set_compare_criteria(compare_criteria->second);
    } else {
        _logger->info("Use default compare criteria -- executionTime.\n");
        new_client_message->set_compare_criteria("executionTime");
    }

    auto compare_more_is_better = env_variables.find("TETRIS_COMPARE_MORE_IS_BETTER") != env_variables.end();
    if (compare_more_is_better)
        _logger->info("Use greater than comparison for criteria.\n");
    else
        _logger->info("Use less then comparison for criteria.\n");
    new_client_message->set_compare_more_is_better(compare_more_is_better);

    auto preferred_mapping = env_variables.find("TETRIS_PREFERRED_MAPPING");
    if (preferred_mapping != env_variables.end()) {
        new_client_message->set_preferred_mapping(preferred_mapping->second);
    }

    auto filter_criteria = env_variables.find("TETRIS_FILTER_CRITERIA");
    if (filter_criteria != env_variables.end()) {
        new_client_message->set_filter_criteria(filter_criteria->second);
    }

    // Send the command.
    auto response = send(request);

    // Process the TETRiS server response.
    if ((response.type() == PullResponse::TETRIS_NEW_CLIENT_ACK) && response.has_new_client_ack()) {
        if (response.new_client_ack().managed())
            _logger->info("TETRIS-ID: %d\n", response.new_client_ack().id());
        else
            _logger->info("TETRIS-ID: not managed\n");
        return response.new_client_ack().managed();
    }

    return false;
}