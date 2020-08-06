//
// Created by dylan on 06/08/2020.
//

#include "push_server.h"
#include "connection.h"

TETRiS::PushServer::PushServer(const std::string &socket_path) {
    server.open(socket_path);
}

void TETRiS::PushServer::add_subscriber(const TETRiS::FeatureID &feature_id, TETRiS::Feature *feature) {
    subscribers.emplace(feature_id, feature);
}

TETRiS::PushResponse TETRiS::PushServer::forward(const TETRiS::PushRequest &request) const {
    auto feature_id = request.feature_id();
    auto feature = subscribers.at(feature_id);
    return feature->forward(request);
}

void* TETRiS::PushServer::listening(void *args) {
    auto push_server = reinterpret_cast<PushServer*>(args);
    int cl;
    // Accept connection on the socket.
    while (true) {
        sockaddr_un in_sock{};
        socklen_t in_sock_size = sizeof(in_sock);
        int infd = ::accept(push_server->server.fd(), reinterpret_cast<sockaddr*>(&in_sock), &in_sock_size);
        if (infd == -1) {
            break;
        }

        /* Make the new socket non-blocking and add it to epoll. */
        std::unique_ptr<Connection> in_conn = std::make_unique<Connection>(infd, in_sock);

        // TODO: Receive, forward and send.


    }
    return nullptr;

}
