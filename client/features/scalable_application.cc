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
    tetris::PullRequest request{};

    request.set_type(tetris::PullRequest::SCALABLE_APPLICATION_SUBSCRIBE);

    auto response = this->get_client()->send(request);
    if ((response.type() == tetris::PullResponse::ACKNOWLEDGE) && response.has_feature_id()) {
        return response.feature_id();
    }

    return -1;
}

PushResponse ScalableApplication::forward(const PushRequest &request)
{
    tetris::PushResponse response{};
    response.set_type(tetris::PushResponse::ERROR);

    if (request.has_application_scale()) {
        auto target_scale = request.application_scale();

        if (_scale_cb(target_scale))
            response.set_type(tetris::PushResponse::ACKNOWLEDGE);
    }

    return response;
}

};
