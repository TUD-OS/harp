//
// Created by dylan on 06/08/2020.
//

#include "tetris_client.h"
#include "tetris_feature.h"
#include "push_message_listener.h"
#include "protobuf_util.h"

#include <memory>
#include <sstream>

std::unique_ptr<TETRiS::Client> TETRiS::ClientProvider::_instance;

void TETRiS::ClientProvider::initialize(const std::string &socket_path) {
    _instance = std::make_unique<TETRiS::Client>(socket_path);
}

void TETRiS::ClientProvider::finalize() { _instance.reset(); }

TETRiS::Client *TETRiS::ClientProvider::get_instance() { return _instance.get(); }

void TETRiS::Client::bind(TETRiS::Feature *feature) {
    // If the client is not connected to the server, do not bind the feature.
    if (!_managed)
        return;

    feature->accept(this);
    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();
        _push_message_listener.add_subscriber(feature_id, feature);
    }
}

TETRiS::ClientResponse TETRiS::Client::send(const TETRiS::ClientRequest &msg) {
    protobuf_util::Send(_tetris_server_connection->locked(), msg);
    auto response = protobuf_util::Receive<ClientResponse>(_tetris_server_connection->locked());
    return response;
}

TETRiS::Client::Client(const std::string &server_socket_path) : _push_message_listener(get_push_listener_socket_path()),
                                                                _managed(false) {
    _logger = debug::Logger::get();
    try {
        _tetris_server_connection = std::make_unique<Connection>(server_socket_path);
        _managed = send_new_client_command();
    } catch (std::exception &e) {
        _managed = false;
    }
}

std::string TETRiS::Client::get_push_listener_socket_path() {
    std::stringstream string_stream{};
    string_stream << "/tmp/tetris_push_listener_" << getpid();
    return string_stream.str();
}

std::map<std::string, std::string> retrieve_env_variables() {
    std::map<std::string, std::string> env_variables{};
    for (auto &env : {"TETRIS_MAPPING_TYPE", "TETRIS_MAPPING_TYPE", "TETRIS_COMPARE_CRITERIA",
                      "TETRIS_COMPARE_MORE_IS_BETTER", "TETRIS_PREFERRED_MAPPING", "TETRIS_FILTER_CRITERIA"}) {
        if (getenv(env))
            env_variables.emplace(env, getenv(env));
    }
    return env_variables;
}

bool TETRiS::Client::send_new_client_command() {

    auto env_variables = retrieve_env_variables();

    ClientRequest request{};
    request.set_type(ClientRequest::TETRIS_NEW_CLIENT);
    auto new_client_message = request.new_client();

    /* Send the new-client message to the server. */
    new_client_message.set_pid(getpid());
    char exec[100];
    memset(exec, 0, sizeof(exec));
    readlink("/proc/self/exe", exec, sizeof(exec));
    new_client_message.set_exec(exec);

    bool dynamic_client = false;
    try {
        auto mapping_type = env_variables.at("TETRIS_MAPPING_TYPE");
        if (mapping_type == "DYNAMIC") {
            _logger->info("Use dynamic/CFS mapping.\n");
            dynamic_client = true;
        } else if (mapping_type == "STATIC") {
            _logger->info("Use static TETRiS mapping.\n");
        } else {
            _logger->warning("Unknown mapping type: %s\n", mapping_type);
        }
    } catch (std::exception &e) { /* Do nothing */ }
    new_client_message.set_dynamic_client(dynamic_client);

    try {
        auto compare_criteria = env_variables.at("TETRIS_COMPARE_CRITERIA");
        _logger->info("Use given compare criteria -- %s.\n", compare_criteria);
        new_client_message.set_compare_criteria(compare_criteria);
    } catch (std::exception &e) {
        _logger->info("Use default compare criteria -- executionTime.\n");
        new_client_message.set_compare_criteria("executionTime");
    }

    auto compare_more_is_better = env_variables.find("TETRIS_COMPARE_MORE_IS_BETTER") != env_variables.end();
    if (compare_more_is_better)
        _logger->info("Use greater than comparison for criteria.\n");
    else
        _logger->info("Use less then comparison for criteria.\n");
    new_client_message.set_compare_more_is_better(compare_more_is_better);

    try {
        auto preferred_mapping = env_variables.at("TETRIS_PREFERRED_MAPPING");
        new_client_message.set_preferred_mapping(preferred_mapping);
    } catch (std::exception &e) {}

    try {
        auto filter_criteria = env_variables.at("TETRIS_FILTERED_CRITERIA");
        new_client_message.set_filter_criteria(filter_criteria);
    } catch (std::exception &e) {}

    auto response = send(request);

    if ((response.type() == ClientResponse::TETRIS_NEW_CLIENT_ACK) && response.has_new_client_ack()) {
        if (response.new_client_ack().managed())
            _logger->info("TETRIS-ID: %d\n", response.new_client_ack().id());
        else
            _logger->info("TETRIS-ID: not managed\n");
        return response.new_client_ack().managed();
    }

    return false;
}
