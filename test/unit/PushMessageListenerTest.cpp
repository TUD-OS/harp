//
// Created by dylan on 10/07/2020.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <push_message_listener.h>
#include <connection.h>
#include <protobuf_util.h>

#define SOCKET_PATH "/tmp/test_socket_tetris_push_listener"

class MockFeature : public TETRiS::Feature {
public:
    MOCK_METHOD(TETRiS::PushResponse, forward, (const TETRiS::PushRequest &msg), (const));
    MOCK_METHOD(bool, need_handshake, (), (const));
    MOCK_METHOD(TETRiS::FeatureID, handshake, ());
};

class PushMessageListenerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    TETRiS::PushMessageListener push_listener{SOCKET_PATH};
};

TEST_F(PushMessageListenerTest, CheckForwardingMechanism) {
    MockFeature mock_1, mock_2;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = TETRiS::PushResponse{};
        response.set_type(TETRiS::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_2, forward).Times(0);

    push_listener.add_subscriber(0, &mock_1);
    push_listener.add_subscriber(1, &mock_2);

    auto request = TETRiS::PushRequest{};
    request.set_feature_id(0);
    auto response = push_listener.forward(request);

    ASSERT_EQ(TETRiS::PushResponse::ACKNOWLEDGE, response.type());
}

TETRiS::PushResponse SendDummyMessage() {
    Connection in_conn{SOCKET_PATH};

    // Prepare the command.
    TETRiS::PushRequest request{};
    request.set_feature_id(0);
    request.set_type(TETRiS::PushRequest::UPDATE_CONFIGURATION);
    // Send the command.
    protobuf_util::Send(in_conn.locked(), request);
    // Receive the response.
    auto response = protobuf_util::Receive<TETRiS::PushResponse>(in_conn.locked());

    return response;
}

TEST_F(PushMessageListenerTest, CheckListener) {
    MockFeature mock_1, mock_2;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = TETRiS::PushResponse{};
        response.set_type(TETRiS::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_2, forward).Times(0);

    push_listener.add_subscriber(0, &mock_1);
    push_listener.add_subscriber(1, &mock_2);

    auto response = SendDummyMessage();

    ASSERT_EQ(TETRiS::PushResponse::ACKNOWLEDGE, response.type());
}