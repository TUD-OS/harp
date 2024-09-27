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

void MovableThreads::mapping_update(const MappingUpdate &mapping, const ConversionMap& /*unused*/)
{
    LOGGER->debug(" > Updating thread<->CPU assignments\n");

    /* Move the registered threads to the corresponding CPU */
    _active_mapping = std::make_unique<Mapping>(mapping);
    _available_cpus = _active_mapping->cpus;

    LOGGER->debug(" -> Using mapping %s\n", _active_mapping->name.c_str());

    {
        std::lock_guard<std::mutex> lock(_mtx);
        for (auto &t : _threads) {
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
        /* Before adding the thread first check if it was already registered before. In this case
         * only update the naming info */
        auto it = std::find_if(_threads.begin(), _threads.end(), [tid](const ThreadInfo& t) { return t.tid == tid; });
        if (it != _threads.end()) {
            it->name = name;
            it->named = true;

            /* Sort the thread list, so that named threads are always at the beginning */
            std::sort(_threads.begin(), _threads.end(), [](const auto &t1, const auto &t2) { return t1.named; });
        } else {
            auto ti = _threads.emplace(_threads.begin(), name, tid, false, true);
            ti->managed = true;

            map_thread(*ti);
        }
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

        if (it->named && it->assigned) {
            /* If this was a named thread, allow other unnamed threads to also
             * use this CPU now */
            _available_cpus.Set(it->cpu);
            for (auto &t : _threads)
                if (!t.named)
                    move_thread(t, _available_cpus);

        }

        return true;
    }

    return false;
}

void MovableThreads::map_thread(ThreadInfo &t)
{
    if (!_active_mapping)
        /* There is no mapping assigned for this application yet! */
        return;

    if (t.named) {
        LOGGER->debug(" -* Moving *named* thread %d ('%s')\n", t.tid, t.name.c_str());
        auto c = _active_mapping->cpu(t.name);

        LOGGER->debug(" --* Using CPUs: %s\n", string_util::join(c.GetList(), ",").c_str());
        if (!move_thread(t, c))
            LOGGER->warning("Failed to update thread %d\n", t.tid);

        _available_cpus^=c;
    } else {
        LOGGER->debug(" -* Moving *unnamed* thread %d ('%s')\n", t.tid, t.name.c_str());
        /* Unnamed threads can use all spare resources */
        if (_available_cpus.Size() != 0) {
            LOGGER->debug(" --* Using remaining CPUs: %s\n", string_util::join(_available_cpus.GetList(), ",").c_str());
            if (!move_thread(t, _available_cpus))
                LOGGER->warning("Failed to update thread %d\n", t.tid);
        } else {
            LOGGER->debug(" --* Using mapping CPUs: %s\n", string_util::join(_active_mapping->cpus.GetList(), ",").c_str());
            if (!move_thread(t, _active_mapping->cpus))
                LOGGER->warning("Failed to update thread %d\n", t.tid);
        }
    }
}

bool MovableThreads::move_thread(ThreadInfo &t, int cpu)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    if (sched_setaffinity(t.tid, sizeof(mask), &mask) != 0) {
        _logger->warning("Failed to set affinity for thread %d\n", t.tid);
        return false;
    }

    if (t.named) {
        t.cpu = cpu;
        t.assigned = true;
    }

    return true;
}

bool MovableThreads::move_thread(ThreadInfo &t, CPUThreadSet cpus)
{
    auto mask = cpus.ToCpuSetT();

    if (sched_setaffinity(t.tid, sizeof(mask), &mask) != 0) {
        _logger->warning("Failed to set affinity for thread %d\n", t.tid);
        return false;
    }

    if (t.named) {
        t.cpu = *cpus.begin();
        t.assigned = true;
    }

    return true;
}

} /* namespace tetris */
