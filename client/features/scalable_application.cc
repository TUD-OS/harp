#include "scalable_application.h"

#include "client/client.h"

namespace tetris {

ScalableApplication::ScalableApplication(std::function<bool (int)> scale_cb) :
    _logger{debug::Logger::get()}, _scale_cb{scale_cb}
{}

ScalableApplication::~ScalableApplication()
{}

FeatureID ScalableApplication::handshake()
{
    tetris::ClientMessage msg;

    msg.set_type(tetris::ClientMessage::FEATURE_SUBSCRIBE);
    auto feature_info = msg.mutable_feature_info();
    feature_info->set_type(tetris::ClientMessage::FeatureInfo::SCALE_APPLICATION);

    auto response = this->get_client()->send(msg);
    if ((response.type() == tetris::ServerResponse::FEATURE_ACKNOWLEDGE) && response.has_feature_ack_info()) {
        return response.feature_ack_info().id();
    }

    return -1;
}

ClientResponse ScalableApplication::forward(const ServerMessage &msg)
{
    tetris::ClientResponse response{};
    response.set_type(tetris::ClientResponse::ERROR);

    if (msg.has_scale_application_info()) {
        auto max_threads = msg.scale_application_info().max_threads();

        if (_scale_cb(max_threads))
            response.set_type(tetris::ClientResponse::ACKNOWLEDGE);
    }

    return response;
}

};
