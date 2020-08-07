//
// Created by dylan on 06/08/2020.
//

#include "tetris_client.h"
#include "tetris_feature.h"
#include "push_server.h"
#include "protobuf_util.h"

#include <memory>
#include <sstream>

std::unique_ptr<TETRiS::Client> TETRiS::Client::_instance;

void TETRiS::Client::initialize(const std::string &server_socket_path) {
    _instance.reset(new TETRiS::Client{server_socket_path});
}

void TETRiS::Client::finalize() { _instance.reset(); }

TETRiS::Client *TETRiS::Client::get_instance() { return _instance.get(); }

void TETRiS::Client::bind(TETRiS::Feature *feature) {
    // If the client is not connected to the server, do not bind the feature.
    if (_tetris_server_connection->fd() == -1)
        return;

    feature->accept(this);
    if (feature->need_handshake()) {
        auto feature_id = feature->handshake();
        _push_server.add_subscriber(feature_id, feature);
    }
}

TETRiS::ClientResponse TETRiS::Client::send(const TETRiS::ClientRequest &msg) {
    protobuf_util::Send(_tetris_server_connection->locked(), msg);
    auto response = protobuf_util::Receive<ClientResponse>(_tetris_server_connection->locked());
    return response;
}

TETRiS::Client::Client(const std::string &server_socket_path) : _push_server(get_push_server_socket_path()) {
    _tetris_server_connection = std::make_unique<Connection>(server_socket_path);
}

std::string TETRiS::Client::get_push_server_socket_path() {
    std::stringstream string_stream{};
    string_stream << "/tmp/tetris_push_server_" << getpid();
    return string_stream.str();
}
