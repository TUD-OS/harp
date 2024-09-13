#ifndef __UTILITY_MEASURE_H__
#define __UTILITY_MEASURE_H__

#pragma once

#include "client/feature.h"

#include "proto/tetris.pb.h"
#include "util/debug_util.h"

#include <vector>

namespace tetris {

class UtilityMeasure : public Feature
{
   private:
    debug::LoggerPtr _logger;

    std::vector<float> _utility_measures;

   public:
    /* Constructor and Destructor */
    UtilityMeasure();

    virtual ~UtilityMeasure();

   public:
    /* Feature interface */
    bool need_handshake() const override { return true; }

    FeatureID handshake() override;

    ClientResponse handle(const ServerMessage &msg) override;

   public:
    /* Public feature interface */
    void update_utility(float utility);

    void clear_utility();
};

} /* namespace tetris */

#endif /* __APPLICATION_PROGRESS_H__ */
