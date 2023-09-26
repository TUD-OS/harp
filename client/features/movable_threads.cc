#include "movable_threads.h"

#include "client/client.h"

#include <sstream>

#include <sched.h>

namespace tetris {

MovableThreads::MovableThreads()
{
    _logger = debug::Logger::get();
}

bool MovableThreads::need_handshake() const
{
    return true;
}

FeatureID MovableThreads::handshake()
{
    tetris::ClientMessage msg;

    msg.set_type(tetris::ClientMessage::FEATURE_SUBSCRIBE);
    auto feature_info = msg.mutable_feature_info();
    feature_info->set_type(tetris::ClientMessage::FeatureInfo::MOVABLE_THREADS);

    auto response = this->get_client()->send(msg);
    if ((response.type() == tetris::ServerResponse::FEATURE_ACKNOWLEDGE) 
            && response.has_feature_ack_info()) {
        return response.feature_ack_info().id();
    }

    return -1;
}

ClientResponse MovableThreads::handle(const ServerMessage &msg)
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

void MovableThreads::mapping_update(const MappingUpdate &mapping)
{
    /* Move the registered threads to the corresponding CPU */
}

bool MovableThreads::extend_mapping(MappingsInfo &mappings)
{
    /* We don't need any extensions here, the parsed mappings should contain all necessary information */
    return false;
}

bool MovableThreads::register_thread(pid_t tid, const std::string& name)
{
    auto ti = _threads.emplace_back(name, tid, false);
    ti.managed = true;

    return true;
}

bool MovableThreads::register_thread(pid_t tid)
{
    std::stringstream ss;
    ss << "thread_" << tid;
    auto ti = _threads.emplace_back(ss.str(), tid, false);
    ti.managed = true;

    return true;
}

bool MovableThreads::unregister_thread(pid_t tid)
{
    auto it = std::find_if(std::begin(_threads), std::end(_threads),
            [tid] (const ThreadInfo& ti) { return ti.tid == tid; });

    if (it != _threads.end()) {
        _threads.erase(it);
        return true;
    }

    return false;
}

bool MovableThreads::move_thread(pid_t tid, int cpu)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    if (sched_setaffinity(tid, sizeof(mask), &mask) != 0) {
        _logger->warning("Failed to set affinity for thread %d\n", tid);
        return false;
    }

    return true;
}

} /* namespace tetris */
