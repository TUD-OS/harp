#ifndef __APPLICATION_PROGRESS_H__
#define __APPLICATION_PROGRESS_H__

#pragma once

#include "client/feature.h"

#include "proto/tetris.pb.h"
#include "util/debug_util.h"

namespace tetris {

class ApplicationProgress : public Feature
{
   private:
    debug::LoggerPtr _logger;

    float _progress;

   public:
    /* Constructor and Destructor */
    ApplicationProgress();

    virtual ~ApplicationProgress();

   public:
    /* Feature interface */
    bool need_handshake() const override { return true; }

    FeatureID handshake() override;

    ClientResponse handle(const ServerMessage &msg) override;

   public:
    /* Public feature interface */
    void update_progress(float progress)
    {
        _progress = progress;
        _logger->debug("Updated application progress to %f\n", progress);
    }
};

} /* namespace tetris */

#endif /* __APPLICATION_PROGRESS_H__ */
