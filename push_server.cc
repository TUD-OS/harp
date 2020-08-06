//
// Created by dylan on 06/08/2020.
//

#include "push_server.h"
#include "connection.h"
#include "protobuf_util.h"

TETRiS::PushServer::PushServer(const std::string &socket_path) : _listener_thread(0) {
    _server.open(socket_path);
    _server.listening();
    pthread_create(&_listener_thread, nullptr, listening, this);
}

void TETRiS::PushServer::add_subscriber(const TETRiS::FeatureID &feature_id, TETRiS::Feature *feature) {
    _subscribers.emplace(feature_id, feature);
}

TETRiS::PushResponse TETRiS::PushServer::forward(const TETRiS::PushRequest &request) const {
    auto feature_id = request.feature_id();
    auto feature = _subscribers.at(feature_id);
    return feature->forward(request);
}

void *TETRiS::PushServer::listening(void *args) {
    auto push_server = reinterpret_cast<PushServer *>(args);
    int cl;
    // Accept connection on the socket.
    while (true) {
        sockaddr_un in_sock{};
        socklen_t in_sock_size = sizeof(in_sock);
        int infd = ::accept(push_server->_server.fd(), reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
        if (infd == -1) {
            break;
        }

        Connection in_conn(infd, in_sock);
        auto request = protobuf_util::Receive<PushRequest>(in_conn.locked());
        auto response = push_server->forward(request);
        protobuf_util::Send(in_conn.locked(), response);
    }

    return nullptr;
}

TETRiS::PushServer::~PushServer() {
    shutdown(_server.fd(), SHUT_RDWR);
    pthread_join(_listener_thread, nullptr);
}
