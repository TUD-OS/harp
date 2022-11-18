#ifndef __MOVABLE_THREADS_H__
#define __MOVABLE_THREADS_H__

#pragma once

#include "client/feature.h"

#include "proto/tetris.pb.h"
#include "util/debug_util.h"

#include <string>
#include <vector>

namespace tetris {

class MovableThreads : public Feature
{
   private:
    struct ThreadInfo {
        std::string name;
        pid_t tid;
        bool managed;

        ThreadInfo(const std::string& name, pid_t tid, bool managed) :
            name{name}, tid{tid}, managed{managed}
        {}
    };

    debug::LoggerPtr _logger;
    std::vector<ThreadInfo> _threads;

   public:
    /* Constructor and Destructor */
    MovableThreads();

    virtual ~MovableThreads();

   public:
    /* Feature interface */
    bool need_handshake() const { return true; }

    FeatureID handshake();

    PushResponse forward(const PushRequest &request);

   public:
    /* Own external interface */
    bool register_thread(const std::string &name, pid_t tid);

   private:
    /* Internal interface */
    bool move_thread(pid_t tid, int cpu);
};

} /* namespace tetris */

#endif /* __MOVABLE_THREADS_H__ */
