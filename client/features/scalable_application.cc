#include "scalable_application.h"

#include "client/client.h"

namespace tetris {

ScalableApplication::ScalableApplication(std::function<bool (int)> scale_cb) :
    _logger{debug::Logger::get()}, _scale_cb{scale_cb}
{}

bool ScalableApplication::need_handshake() const
{
    return false;
}

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

ClientResponse ScalableApplication::handle(const ServerMessage &msg)
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

void ScalableApplication::mapping_update(const MappingUpdate &mapping)
{
    LOGGER->debug(" > Updating application scaling\n");

    _active_mapping = std::make_unique<Mapping>(mapping);

    LOGGER->debug(" -> Using mapping %s\n", _active_mapping->name.c_str());
    LOGGER->debug(" -* New scaling factor: %d\n", _active_mapping->cpus.Size());

    if (!_scale_cb(_active_mapping->cpus.Size()))
        LOGGER->warning(" --* Failed to resize to the required scaling factor\n");
}

bool ScalableApplication::extend_mapping(MappingsInfo &mappings)
{
    return false;
}

}
