#include "utility_measure.h"

#include "client/client.h"
#include "util/debug_util.h"

namespace tetris {

UtilityMeasure::UtilityMeasure() :
    _logger{debug::Logger::get()}
{}

UtilityMeasure::~UtilityMeasure()
{}

FeatureID UtilityMeasure::handshake()
{
    tetris::ClientMessage msg;

    msg.set_type(tetris::ClientMessage::FEATURE_SUBSCRIBE);
    auto feature_info = msg.mutable_feature_info();
    feature_info->set_type(tetris::ClientMessage::FeatureInfo::UTILITY_MEASURE);

    auto response = this->get_client()->send(msg);
    if ((response.type() == tetris::ServerResponse::FEATURE_ACKNOWLEDGE) && response.has_feature_ack_info()) {
        return response.feature_ack_info().id();
    }

    return -1;
}

ClientResponse UtilityMeasure::handle(const ServerMessage &msg)
{
    tetris::ClientResponse response{};

    if (_utility_measures.size() > 0) {
        response.set_type(ClientResponse::UTILITY_UPDATE);
    
         response.set_utility(_utility_measures.back());
    } else {
        response.set_type(ClientResponse::UTILITY_RETRY);
    }

    return response;
}

void UtilityMeasure::update_utility(float new_measure)
{
    _utility_measures.push_back(new_measure);
}

void UtilityMeasure::clear_utility()
{
    _utility_measures.clear();
}

};
