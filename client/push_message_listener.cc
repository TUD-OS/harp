//
// Created by dylan on 06/08/2020.
//

#include "push_message_listener.h"
#include "util/connection.h"
#include "util/protobuf_util.h"
#include "util/pthread_direct.h"
#include "proto/tetris.pb.h"


namespace tetris {

PushMessageListener::PushMessageListener(const std::string &socket_path, Client *client) :
    _listener_thread{0}, _client{client}
{
    _listening_socket.open(socket_path);
    _listening_socket.listening();
    direct_pthread_create(&_listener_thread, nullptr, listening, this);
}

void PushMessageListener::add_subscriber(const FeatureID &feature_id, Feature *feature)
{
    _subscribers.emplace(feature_id, feature);
}

ClientResponse PushMessageListener::forward(const ServerMessage &msg) const
{
    auto feature_id = msg.feature_id();
    if (feature_id == 0) {
        return _client->handle(msg);
    } else {
        auto feature = _subscribers.at(feature_id);
        return feature->handle(msg);
    }
}

void *PushMessageListener::listening(void *args)
{
    auto push_server = reinterpret_cast<PushMessageListener *>(args);

    // Accept connection on the socket.
    while (true) {
        sockaddr_un in_sock{};
        socklen_t in_sock_size = sizeof(in_sock);
        int infd = ::accept(push_server->_listening_socket.fd(), reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
        if (infd == -1) {
            break;
        }

        Connection in_conn(infd, in_sock);
        ServerMessage msg{};
        protobuf_util::Receive(in_conn.locked(), msg);
        auto response = push_server->forward(msg);
        protobuf_util::Send(in_conn.locked(), response);
    }

    return nullptr;
}

PushMessageListener::~PushMessageListener()
{
    shutdown(_listening_socket.fd(), SHUT_RDWR);
    pthread_join(_listener_thread, nullptr);
}

}
