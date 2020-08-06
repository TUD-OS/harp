//
// Created by dylan on 10/07/2020.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <push_server.h>

class MockFeature : public TETRiS::Feature {
public:
    MOCK_METHOD(TETRiS::PushResponse, forward, (const TETRiS::PushRequest &msg), (const));
    MOCK_METHOD(bool, need_handshake, (), (const));
    MOCK_METHOD(TETRiS::FeatureID, handshake, ());
};


class PushServerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    TETRiS::PushServer push_server{""};
};

TEST_F(PushServerTest, CheckForwardingMechanism) {
    MockFeature mock_1, mock_2;
    EXPECT_CALL(mock_1, forward).Times(1).WillOnce([]() {
        auto response = TETRiS::PushResponse{};
        response.set_type(TETRiS::PushResponse::ACKNOWLEDGE);
        return response;
    });
    EXPECT_CALL(mock_2, forward).Times(0);

    push_server.add_subscriber(0, &mock_1);
    push_server.add_subscriber(1, &mock_2);

    auto request = TETRiS::PushRequest{};
    request.set_feature_id(0);
    auto response = push_server.forward(request);

    ASSERT_EQ(TETRiS::PushResponse::ACKNOWLEDGE, response.type());
}
