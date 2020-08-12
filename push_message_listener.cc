//
// Created by dylan on 06/08/2020.
//

#include "push_message_listener.h"
#include "connection.h"
#include "protobuf_util.h"
#include "proto/tetris.pb.h"

tetris::PushMessageListener::PushMessageListener(const std::string &socket_path) : _listener_thread(0) {
    _listening_socket.open(socket_path);
    _listening_socket.listening();
    pthread_create(&_listener_thread, nullptr, listening, this);
}

void tetris::PushMessageListener::add_subscriber(const tetris::FeatureID &feature_id, tetris::Feature *feature) {
    _subscribers.emplace(feature_id, feature);
}

tetris::PushResponse tetris::PushMessageListener::forward(const tetris::PushRequest &request) const {
    auto feature_id = request.feature_id();
    auto feature = _subscribers.at(feature_id);
    return feature->forward(request);
}

void *tetris::PushMessageListener::listening(void *args) {
    auto push_server = reinterpret_cast<PushMessageListener *>(args);
    int cl;
    // Accept connection on the socket.
    while (true) {
        sockaddr_un in_sock{};
        socklen_t in_sock_size = sizeof(in_sock);
        int infd = ::accept(push_server->_listening_socket.fd(), reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
        if (infd == -1) {
            break;
        }

        Connection in_conn(infd, in_sock);
        tetris::PushRequest request{};
        protobuf_util::Receive(in_conn.locked(), request);
        auto response = push_server->forward(request);
        protobuf_util::Send(in_conn.locked(), response);
    }

    return nullptr;
}

tetris::PushMessageListener::~PushMessageListener() {
    shutdown(_listening_socket.fd(), SHUT_RDWR);
    pthread_join(_listener_thread, nullptr);
}
