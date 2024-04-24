#ifndef __SCALABLE_APPLICATION_H__
#define __SCALABLE_APPLICATION_H__

#pragma once

#include "client/mapping_feature.h"

#include "proto/tetris.pb.h"
#include "util/debug_util.h"

#include <functional>

namespace tetris {

class ScalableApplication : public MappingFeature
{
   private:
    debug::LoggerPtr _logger;

    std::function<bool (int)> _scale_cb;

    std::unique_ptr<Mapping> _active_mapping;

   public:
    /* Constructor and Destructor */
    ScalableApplication(std::function<bool (int)> scale_cb);

    virtual ~ScalableApplication() = default;

   public:
    /* Feature interface */
    bool need_handshake() const override { return false; };

    void mapping_update(const MappingUpdate &mapping, const ConversionMap &conv) override;

    bool extend_mapping(MappingsInfo &mappings) override;

    virtual int current_scale() const {
        return 0;
    }
};

} /* namespace tetris */

#endif /* __SCALABLE_APPLICATION_H__ */
