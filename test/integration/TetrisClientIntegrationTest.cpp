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

class TetrisClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        unlink("mock_tetris_server");
        _mock_tetris_server.open("mock_tetris_server");
        _mock_tetris_server.listening();
        TETRiS::Client::initialize("mock_tetris_server");
    }

    void TearDown() override {
        TETRiS::Client::finalize();
    }
    Socket _mock_tetris_server;
};

TETRiS::PushResponse SendMessage(const TETRiS::FeatureID& feature_id) {
    Connection in_conn{TETRiS::Client::get_push_server_socket_path()};

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
