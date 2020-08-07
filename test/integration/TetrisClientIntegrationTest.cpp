//
// Created by dylan on 07/08/2020.
//


#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <tetris_client.h>
#include <tetris_feature.h>
#include <protobuf_util.h>


class MockFeature : public TETRiS::Feature {
public:
    MOCK_METHOD(TETRiS::PushResponse, forward, (const TETRiS::PushRequest &msg), (const));
    MOCK_METHOD(bool, need_handshake, (), (const));
    MOCK_METHOD(TETRiS::FeatureID, handshake, ());
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
    Connection in_conn(infd, in_sock);
    auto request = protobuf_util::Receive<TETRiS::ClientRequest>(in_conn.locked());
    auto response = TETRiS::ClientResponse{};
    response.set_type(TETRiS::ClientResponse::TETRIS_NEW_CLIENT_ACK);
    auto new_client_ack_msg = response.mutable_new_client_ack();
    new_client_ack_msg->set_managed(true);
    new_client_ack_msg->set_id(0);
    protobuf_util::Send(in_conn.locked(), response);
    return nullptr;
}


class TetrisClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        unlink("mock_tetris_server");
        _mock_tetris_server.open("mock_tetris_server");
        _mock_tetris_server.listening();
        pthread_create(&_listening_thread, nullptr, listening, &_mock_tetris_server);
        TETRiS::Client::initialize("mock_tetris_server");
    }

    void TearDown() override {
        TETRiS::Client::finalize();
        pthread_join(_listening_thread, nullptr);
    }
    Socket _mock_tetris_server;
    pthread_t _listening_thread;
};

TETRiS::PushResponse SendMessage(const TETRiS::FeatureID& feature_id) {
    Connection in_conn{TETRiS::Client::get_push_listener_socket_path()};

    // Prepare the command.
    TETRiS::PushRequest request{};
    request.set_feature_id(feature_id);
    request.set_type(TETRiS::PushRequest::UPDATE_CONFIGURATION);
    // Send the command.
    protobuf_util::Send(in_conn.locked(), request);
    // Receive the response.
    auto response = protobuf_util::Receive<TETRiS::PushResponse>(in_conn.locked());

    return response;
}

TEST_F(TetrisClientIntegrationTest, CheckBinding) {
    MockFeature mock_1;
    EXPECT_CALL(mock_1, need_handshake).Times(1).WillOnce([]() { return false; });
    TETRiS::Client::get_instance()->bind(&mock_1);
    ASSERT_TRUE(mock_1.is_bound());
}

TEST_F(TetrisClientIntegrationTest, CheckPushServerForwarding) {
    MockFeature mock_1;
    TETRiS::FeatureID attributed_feature_id = 64;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = TETRiS::PushResponse{};
        response.set_type(TETRiS::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_1, need_handshake).Times(1).WillOnce([]() { return true; });
    EXPECT_CALL(mock_1, handshake).Times(1).WillOnce([attributed_feature_id]() { return attributed_feature_id; });

    TETRiS::Client::get_instance()->bind(&mock_1);

    auto response = SendMessage(attributed_feature_id);
    ASSERT_EQ(TETRiS::PushResponse::ACKNOWLEDGE, response.type());
}
