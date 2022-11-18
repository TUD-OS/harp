#ifndef __SCALABLE_APPLICATION_H__
#define __SCALABLE_APPLICATION_H__

#pragma once

#include "client/feature.h"

#include "proto/tetris.pb.h"
#include "util/debug_util.h"

#include <functional>

namespace tetris {

class ScalableApplication : public Feature
{
   private:
    debug::LoggerPtr _logger;

    std::function<bool (int)> _scale_cb;

   public:
    /* Constructor and Destructor */
    ScalableApplication(std::function<bool (int)> scale_cb);

    virtual ~ScalableApplication();

   public:
    /* Feature interface */
    bool need_handshake() const { return true; }

    FeatureID handshake();

    PushResponse forward(const PushRequest &request);
};

} /* namespace tetris */

#endif /* __SCALABLE_APPLICATION_H__ */
