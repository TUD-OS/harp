#include "movable_threads.h"

#include "client/client.h"

#include <sched.h>

namespace tetris {

MovableThreads::MovableThreads()
{
    _logger = debug::Logger::get();
}

MovableThreads::~MovableThreads()
{}

FeatureID MovableThreads::handshake()
{
    tetris::PullRequest request{};

    request.set_type(tetris::PullRequest::MOVABLE_THREADS_SUBSCRIBE);

    auto response = this->get_client()->send(request);
    if ((response.type() == tetris::PullResponse::ACKNOWLEDGE) && response.has_feature_id()) {
        return response.feature_id();
    }

    return -1;
}

PushResponse MovableThreads::forward(const PushRequest &request)
{
    bool success = true;

    for (auto thread_assigment : request.thread_assignments()) {
        success &= move_thread(thread_assigment.tid(), thread_assigment.cpu());
    }

    tetris::PushResponse response{};

    if (success) {
        response.set_type(tetris::PushResponse::ACKNOWLEDGE);
    } else {
        response.set_type(tetris::PushResponse::ERROR);
    }

    return response;
}

bool MovableThreads::register_thread(const std::string &name, pid_t tid)
{
    tetris::PullRequest request{};

    /* Send the new-thread message to the server. */
    request.set_type(tetris::PullRequest::TETRIS_NEW_THREAD);
    auto new_thread_message = request.mutable_new_thread();
    new_thread_message->set_tid(tid);
    new_thread_message->set_name(name);

    auto thread_info = _threads.emplace_back(name, tid, false);

    auto response = this->get_client()->send(request);
    if ((response.type() == PullResponse::TETRIS_NEW_THREAD_ACK) && response.has_new_thread_ack()) {
        if (response.new_thread_ack().managed()) {
            _logger->info("Thread %s (%d) managed by TETRiS\n", name.c_str(), tid);
            thread_info.managed = true;
        } else {
            _logger->info("Thread %s (%d) NOT managed by TETRiS\n", name.c_str(), tid);
        }

        return response.new_thread_ack().managed();
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
