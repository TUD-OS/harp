#include "movable_threads.h"

#include "client/client.h"
#include "util/debug_util.h"
#include "util/string_util.h"

#include <memory>
#include <mutex>
#include <sstream>

#include <sched.h>

namespace tetris {

MovableThreads::MovableThreads() :
    _active_mapping{nullptr}
{
    _logger = debug::Logger::get();
}

bool MovableThreads::need_handshake() const
{
    return false;
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
    LOGGER->debug(" > Updating thread<->CPU assignments\n");

    /* Move the registered threads to the corresponding CPU */
    _active_mapping = std::make_unique<Mapping>(mapping);
    _assigned_threads.clear();

    LOGGER->debug(" -> Using mapping %s\n", _active_mapping->name.c_str());

    {
        std::lock_guard<std::mutex> lock(_mtx);
        for (const auto &t : _threads) {
            map_thread(t);
        }
    }
}

bool MovableThreads::extend_mapping(MappingsInfo &mappings)
{
    /* We don't need any extensions here, the parsed mappings should contain all necessary information */
    return false;
}

bool MovableThreads::register_thread(pid_t tid, const std::string& name)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        auto ti = _threads.emplace_back(name, tid, false, true);
        ti.managed = true;

        map_thread(ti);
    }

    LOGGER->debug("Thread registered: %d, %s\n", tid, name.c_str());

    return true;
}

bool MovableThreads::register_thread(pid_t tid)
{
    std::stringstream ss;
    ss << "thread_" << tid;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        auto ti = _threads.emplace_back(ss.str(), tid, false);
        ti.managed = true;

        map_thread(ti);
    }

    LOGGER->debug("Thread registered: %d, %s\n", tid, ss.str().c_str());

    return true;
}

bool MovableThreads::unregister_thread(pid_t tid)
{
    std::lock_guard<std::mutex> lock(_mtx);

    auto it = std::find_if(std::begin(_threads), std::end(_threads),
            [tid] (const ThreadInfo& ti) { return ti.tid == tid; });

    if (it != _threads.end()) {
        _threads.erase(it);

        LOGGER->debug("Thread unregistered: %d\n", tid);

        /* Update the mapping of the existing threads */
        _assigned_threads.clear();
        for (auto &t : _threads) {
            map_thread(t);
        }

        return true;
    }

    return false;
}

void MovableThreads::map_thread(const ThreadInfo &t)
{
    if (!_active_mapping)
        /* There is no mapping assigned for this application yet! */
        return;

    if (t.named) {
        LOGGER->debug(" -* Moving *named* thread %d ('%s')\n", t.tid, t.name.c_str());
        auto c = _active_mapping->cpu(t.name);

        LOGGER->debug(" --* Using CPUs: %s\n", string_util::join(c.GetList(), ",").c_str());
        if (!move_thread(t.tid, c))
            LOGGER->warning("Failed to update *named* thread %d\n", t.tid);

        _assigned_threads.push_back(t.name);
    } else {
        LOGGER->debug(" -* Moving *unnamed* thread %d ('%s')\n", t.tid, t.name.c_str());
        /* Go through the threads map in the mapping and use a CPU assignment that isn't used yet */
        bool assigned = false;
        for (const auto &candidate : _active_mapping->thread_map) {
            if (std::find_if(_assigned_threads.begin(), _assigned_threads.end(),
                        [candidate](const std::string &assigned_t) { return candidate.first == assigned_t; }) != _assigned_threads.end())
                continue;

            LOGGER->debug(" --* Using CPUs: %d\n", candidate.second);
            if (!move_thread(t.tid, candidate.second))
                LOGGER->warning("Failed to update *unnamed* thread %d\n", t.tid);

            _assigned_threads.push_back(candidate.first);
            assigned = true;
            break;
        }

        if (!assigned) {
            LOGGER->warning(" -* Can't find available thread mapping for %d --> Reuse\n", t.tid);

            auto it = _active_mapping->thread_map.begin();
            auto &[_, cpus] = *it;
            if (!move_thread(t.tid, cpus))
                LOGGER->warning("Failed to update *unnamed* thread %d\n", t.tid);
        }
    }
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

bool MovableThreads::move_thread(pid_t tid, CPUThreadSet cpus)
{
    auto mask = cpus.ToCpuSetT();

    if (sched_setaffinity(tid, sizeof(mask), &mask) != 0) {
        _logger->warning("Failed to set affinity for thread %d\n", tid);
        return false;
    }

    return true;
}

} /* namespace tetris */
