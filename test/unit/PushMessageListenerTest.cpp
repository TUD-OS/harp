//
// Created by dylan on 10/07/2020.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <push_message_listener.h>
#include <connection.h>
#include <protobuf_util.h>
#include <proto/tetris.pb.h>

#define SOCKET_PATH "/tmp/test_socket_tetris_push_listener"

class MockFeature : public tetris::Feature
{
public:
    MOCK_METHOD(tetris::PushResponse, forward, (const tetris::PushRequest &msg));
    MOCK_METHOD(bool, need_handshake, (), (const));
    MOCK_METHOD(tetris::FeatureID, handshake, ());
};

class PushMessageListenerTest : public ::testing::Test
{
protected:
    tetris::PushMessageListener push_listener{SOCKET_PATH};
};

TEST_F(PushMessageListenerTest, CheckForwardingMechanism)
{
    MockFeature mock_1, mock_2;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = tetris::PushResponse{};
        response.set_type(tetris::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_2, forward).Times(0);
    // Add the two mocks as subscribers to the push listener.
    push_listener.add_subscriber(0, &mock_1);
    push_listener.add_subscriber(1, &mock_2);
    // Send a push request to the push listener.
    auto request = tetris::PushRequest{};
    request.set_feature_id(0);
    auto response = push_listener.forward(request);
    // Assert the response is a acknowledge from mock_1.
    ASSERT_EQ(tetris::PushResponse::ACKNOWLEDGE, response.type());
}

tetris::PushResponse SendDummyRequest()
{
    Connection in_conn{SOCKET_PATH};

    // Prepare the command.
    tetris::PushRequest request{};
    tetris::PushResponse response{};
    request.set_feature_id(0);
    request.set_type(tetris::PushRequest::UPDATE_CONFIGURATION);
    // Send the request.
    protobuf_util::Send(in_conn.locked(), request);
    // Receive the response.
    protobuf_util::Receive<>(in_conn.locked(), response);

    return response;
}

TEST_F(PushMessageListenerTest, CheckListener)
{
    MockFeature mock_1, mock_2;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = tetris::PushResponse{};
        response.set_type(tetris::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_2, forward).Times(0);

    push_listener.add_subscriber(0, &mock_1);
    push_listener.add_subscriber(1, &mock_2);

    auto response = SendDummyRequest();

    ASSERT_EQ(tetris::PushResponse::ACKNOWLEDGE, response.type());
}