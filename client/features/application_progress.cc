#include "application_progress.h"

#include "client/client.h"

namespace tetris {

ApplicationProgress::ApplicationProgress() :
    _logger{debug::Logger::get()}, _progress{0}
{}

ApplicationProgress::~ApplicationProgress()
{}

FeatureID ApplicationProgress::handshake()
{
    tetris::ClientMessage msg;

    msg.set_type(tetris::ClientMessage::FEATURE_SUBSCRIBE);
    auto feature_info = msg.mutable_feature_info();
    feature_info->set_type(tetris::ClientMessage::FeatureInfo::APPLICATION_PROGRESS);

    auto response = this->get_client()->send(msg);
    if ((response.type() == tetris::ServerResponse::FEATURE_ACKNOWLEDGE) && response.has_feature_ack_info()) {
        return response.feature_ack_info().id();
    }

    return -1;
}

ClientResponse ApplicationProgress::handle(const ServerMessage &msg)
{
    bool success = true;

    tetris::ClientResponse response{};

    if (success) {
        response.set_type(tetris::ClientResponse::ACKNOWLEDGE);
    } else {
        response.set_type(tetris::ClientResponse::ERROR);
    }
    return response;
}

};
