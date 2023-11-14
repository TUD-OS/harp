#ifndef __MOVABLE_THREADS_H__
#define __MOVABLE_THREADS_H__

#include <memory>
#pragma once

#include "client/mapping_feature.h"

#include "proto/tetris.pb.h"
#include "util/debug_util.h"

#include <string>
#include <vector>
#include <mutex>

namespace tetris {

class MovableThreads : public MappingFeature
{
   private:
    struct ThreadInfo {
        std::string name;
        pid_t tid;
        int cpu;
        bool managed;
        bool named;
        bool assigned;

        ThreadInfo(const std::string& name, pid_t tid, bool managed, bool named = false) :
            name{name}, tid{tid}, cpu{0}, managed{managed}, named{named}, assigned{false}
        {}
    };

    debug::LoggerPtr _logger;
    std::vector<ThreadInfo> _threads;
    std::mutex _mtx;

    std::unique_ptr<Mapping> _active_mapping;
    CPUThreadSet _available_cpus;

   public:
    /* Constructor and Destructor */
    MovableThreads();

    virtual ~MovableThreads() = default;

   public:
    /* Feature interface */
    bool need_handshake() const override { return false; };

    /* MappingFeature interface */
    void mapping_update(const MappingUpdate &mapping, const ConversionMap &conv) override;
    bool extend_mapping(MappingsInfo &mappings) override;

   public:
    /* Own external interface */
    bool register_thread(pid_t tid, const std::string &name);
    bool register_thread(pid_t tid);
    bool unregister_thread(pid_t tid);

   private:
    /* Internal interface */
    void map_thread(ThreadInfo &t);
    bool move_thread(ThreadInfo &t, int cpu);
    bool move_thread(ThreadInfo &t, CPUThreadSet cpus);
};

} /* namespace tetris */

#endif /* __MOVABLE_THREADS_H__ */
