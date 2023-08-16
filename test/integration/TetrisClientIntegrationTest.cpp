//
// Created by dylan on 07/08/2020.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <client/client.h>
#include <util/protobuf_util.h>
#include <util/socket.h>
#include <client/concrete_client.h>

#if 0

class MockFeature : public tetris::Feature {
public:
    MOCK_METHOD(tetris::PushResponse, forward, (const tetris::PushRequest &msg));
    MOCK_METHOD(bool, need_handshake, (), (const));
    MOCK_METHOD(tetris::FeatureID, handshake, ());
};

void *listening(void *args) {
    auto socket = reinterpret_cast<Socket *>(args);
    int cl;
    // Accept connection on the socket.
    sockaddr_un in_sock{};
    socklen_t in_sock_size = sizeof(in_sock);
    int infd = ::accept(socket->fd(), reinterpret_cast<sockaddr *>(&in_sock), &in_sock_size);
    if (infd == -1)
        return nullptr;
    // Handle the new connection.
    Connection in_conn(infd, in_sock);
    tetris::PullRequest request{};
    tetris::PullResponse response{};
    // Wait for the request.
    protobuf_util::Receive(in_conn.locked(), request);
    // Prepare the response.
    response.set_type(tetris::PullResponse::TETRIS_NEW_CLIENT_ACK);
    auto new_client_ack_msg = response.mutable_new_client_ack();
    new_client_ack_msg->set_managed(true);
    new_client_ack_msg->set_id(0);
    protobuf_util::Send(in_conn.locked(), response);
    return nullptr;
}


class TetrisClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup a mock TETRiS server.
        unlink("mock_tetris_server");
        _mock_tetris_server.open("mock_tetris_server");
        _mock_tetris_server.listening();
        pthread_create(&_listening_thread, nullptr, listening, &_mock_tetris_server);
        tetris::ClientProvider::initialize("mock_tetris_server");
    }

    void TearDown() override {
        tetris::ClientProvider::finalize();
        pthread_join(_listening_thread, nullptr);
    }
    Socket _mock_tetris_server;
    pthread_t _listening_thread;
};

tetris::PushResponse SendRequestToPushListener(const tetris::FeatureID& feature_id) {
    // Setup the connection with the push listener.
    Connection in_conn{tetris::ConcreteClient::get_push_listener_socket_path()};
    // Prepare the command.
    tetris::PushRequest request{};
    tetris::PushResponse response{};
    request.set_feature_id(feature_id);
    request.set_type(tetris::PushRequest::DPM_UPDATE_CONFIGURATION);
    // Send the command.
    protobuf_util::Send(in_conn.locked(), request);
    // Receive the response.
    protobuf_util::Receive(in_conn.locked(), response);

    return response;
}

TEST_F(TetrisClientIntegrationTest, CheckBinding) {
    MockFeature mock_1;
    EXPECT_CALL(mock_1, need_handshake).Times(1).WillOnce([]() { return false; });
    tetris::ClientProvider::get_instance()->bind(&mock_1);
    ASSERT_TRUE(mock_1.is_bounded());
}

TEST_F(TetrisClientIntegrationTest, CheckPushListenerForwarding) {
    MockFeature mock_1;
    tetris::FeatureID attributed_feature_id = 64;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = tetris::PushResponse{};
        response.set_type(tetris::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_1, need_handshake).Times(1).WillOnce([]() { return true; });
    EXPECT_CALL(mock_1, handshake).Times(1).WillOnce([attributed_feature_id]() { return attributed_feature_id; });
    // Bind the mock feature to the client.
    tetris::ClientProvider::get_instance()->bind(&mock_1);
    // Send a push request to the push listener.
    auto response = SendRequestToPushListener(attributed_feature_id);
    // Check that the answer from the mock feature is an acknowledge.
    ASSERT_EQ(tetris::PushResponse::ACKNOWLEDGE, response.type());
}

#endif
