#include "util/debug_util.h"

#include "client/client.h"
#include "client/concrete_client.h"
#include "client/feature.h"

#include "client/features/movable_threads.h"
#include "client/features/scalable_application.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


/***
 * Time Keeping
 ***/

template<typename T, typename Clock, typename Resolution>
class TimeKeeper
{
private:
    T &_total;
    typename Clock::time_point _start;
    bool _running;

public:
    TimeKeeper(T &total) :
            _total{total}, _start{}, _running{false}
    {
        start();
    }

    ~TimeKeeper()
    {
        stop();
    }

    void start()
    {
        if (!_running) {
            _start = Clock::now();
            _running = true;
        }
    }

    void stop()
    {
        if (_running) {
            auto end = Clock::now();
            _total += std::chrono::duration_cast<Resolution>(end - _start).count();
            _running = false;
        }
    }
};


/***
 * Thread management
 ***/

struct ThreadInfo
{
    pthread_t *pthread_id;
    pthread_mutex_t mtx;

    pid_t tid;
    char name[100];

    bool named = false;
    bool ready = false;

    bool managed = false;

    void *(*func)(void *);

    void *arg;
};

struct CheckerState
{
    enum Type {
        GOMP = 1,
        OMP = 2,
    };

    bool is_scalable = false;
    Type scale_type;
};

/***
 * Global variables
 ***/

using Timer = TimeKeeper<std::atomic_ulong, std::chrono::system_clock, std::chrono::nanoseconds>;

using ClientPtr = std::unique_ptr<tetris::ConcreteClient>;
using MovableThreadsPtr = std::unique_ptr<tetris::MovableThreads>;
using ScalableApplicationPtr = std::unique_ptr<tetris::ScalableApplication>;

using ThreadList = std::vector<ThreadInfo *>;
using ThreadListPtr = std::unique_ptr<ThreadList>;

debug::LoggerPtr logger;
ThreadListPtr threads;

ClientPtr tetris_client;
MovableThreadsPtr movable_threads;
ScalableApplicationPtr scalable_app;

std::atomic_ulong time_ns;

/***
 ***/
static int scalable_app_checker(struct dl_phdr_info *i, size_t size, void *data) {
    CheckerState *cstate = static_cast<CheckerState*>(data);

    if ((strstr(i->dlpi_name, "libomp") != NULL)) {
        logger->info(" -> Found scalable OpenMP application (libomp)\n");
        cstate->is_scalable = true;
        cstate->scale_type= CheckerState::OMP;
        return 1;
    } else if ((strstr(i->dlpi_name, "libgomp") != NULL)) {
        logger->info(" -> Found scalable OpenMP application (libgomp)\n");
        cstate->is_scalable = true;
        cstate->scale_type= CheckerState::GOMP;
        return 1;
    }
    return 0;
}

static bool is_scalable_app() {
    CheckerState cstate;
    logger->info(" -> Searching for scalable app\n");

    dl_iterate_phdr(scalable_app_checker, &cstate);

    return cstate.is_scalable;
}

bool scale_application_cb(int nr_threads)
{
    return true;
}

/***
 * Library setup and tierdown
 ***/

extern "C"
void __attribute__((constructor)) setup(void)
{
    Timer t{time_ns};

    time_ns = 0;
    logger = debug::Logger::get();
    threads = std::make_unique<ThreadList>();

    logger->info("Loading TETRIS support\n");

    tetris_client = std::make_unique<tetris::ConcreteClient>(tetris::SERVER_SOCKET);
    if (tetris_client->is_managed()) {
        logger->info("->> Managed by TETRIS <<-\n");

        /* Register with TETRiS that this client supports movable threads */
        logger->info("->> Register as application with movable threads\n");
        movable_threads = std::make_unique<tetris::MovableThreads>();
        tetris_client->bind(movable_threads.get());

        if (is_scalable_app()) {
            logger->info("->> Register as scalable application\n");
            scalable_app = std::make_unique<tetris::ScalableApplication>(scale_application_cb);
            tetris_client->bind(scalable_app.get());
        }
    } else {
        logger->info("->> NOT managed by TETRIS <<-\n");
    }
}

extern "C"
void __attribute__((destructor)) tierdown(void)
{
    Timer t{time_ns};
    tetris_client.reset();

    t.stop();

    unsigned long _ns = time_ns;
    unsigned long ms = _ns / 1000000;
    unsigned us = (_ns % 1000000) / 1000;
    unsigned ns = _ns % 1000;
    logger->always("Total time spent in TETRIS: %lu.%03u%03lu ms (%lu ns)\n", ms, us, ns, _ns);
}


/***
 * pthread wrapper
 ***/

static
void *thread_wrapper(void *arg)
{
    Timer t{time_ns};

    auto ti = static_cast<ThreadInfo *>(arg);

    pthread_mutex_lock(&ti->mtx);

    /* Update the tid information in the ThreadInfo struct for this thread. */
    ti->tid = syscall(SYS_gettid);
    ti->ready = true;

    if (ti->named && ti->ready)
        ti->managed = movable_threads->register_thread(ti->name, ti->tid);

    pthread_mutex_unlock(&ti->mtx);

    /* Call the actual function. */
    t.stop();
    void *ret = ti->func(ti->arg);

    /* If necessary we can do some tear down before returning */
    return ret;
}

extern "C"
int pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
                   void *(*routine)(void *), void *arg)
{
    using real_func_t = int (*)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);

    Timer t{time_ns};

    /* Get the real pthread_create function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_create"));

    if (real_func != nullptr) {
        if (tetris_client && tetris_client->is_managed()) {
            /* This program is managed by TETRIS. Accordingly create the
             * thread and wait until a name is assigned to it so that
             * the TETRIS server can move this thread to the appropriate
             * CPU. */

            /* We need to create the ThreadInfo struct for this thread and
             * call the wrapper function which will perform all the necessary
             * setup with the TETRIS server. */
            auto ti = new ThreadInfo{};
            ti->pthread_id = thread_id;
            pthread_mutex_init(&ti->mtx, nullptr);

            ti->func = routine;
            ti->arg = arg;

            /* We need to safe the information so we can find it in
             * later pthread* calls. */
            threads->push_back(ti);

            return real_func(thread_id, attr, thread_wrapper, ti);
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_create function. */
            return real_func(thread_id, attr, routine, arg);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_create function.\n");
        exit(-1);
    }
}

extern "C"
int pthread_setname_np(pthread_t thread_id, const char *name)
{
    using real_func_t = int (*)(pthread_t, const char *);

    Timer t{time_ns};

    /* Get the real pthread_setname_np function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_setname_np"));

    if (real_func != nullptr) {
        if (tetris_client && tetris_client->is_managed()) {
            /* Search for the ThreadInfo struct of this thread. */
            auto iti = std::find_if(threads->begin(), threads->end(), [&](ThreadInfo *ti) -> bool {
                return pthread_equal(*(ti->pthread_id), thread_id) != 0;
            });

            if (iti != threads->end()) {
                /* Ok we found the corresponding ThreadInfo. So first
                 * make the actual call and then signal the thread that
                 * it is properly setup now. */
                auto res = real_func(thread_id, name);

                auto ti = *iti;
                pthread_mutex_lock(&ti->mtx);
                strncpy(ti->name, name, sizeof(ti->name));
                ti->named = true;

                if (ti->named && ti->ready)
                    ti->managed = movable_threads->register_thread(ti->name, ti->tid);

                pthread_mutex_unlock(&ti->mtx);

                return res;
            } else {
                /* We could not find the corresponding ThreadInfo struct.
                 * Something went wrong here!. */
                logger->error("Failed to find appropriate ThreadInfo struct.\n");
                exit(-1);
            }
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_setname_np function. */
            return real_func(thread_id, name);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_setname_np function.\n");
        exit(-1);
    }
}

extern "C"
int pthread_setaffinity_np(pthread_t thread_id, size_t cpusetsize,
                           const cpu_set_t *cpuset)
{
    using real_func_t = int (*)(pthread_t, size_t, const cpu_set_t *);

    Timer t{time_ns};

    /* Get the real pthread_setaffinity_np function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_setaffinity_np"));

    if (real_func != nullptr) {
        if (tetris_client && tetris_client->is_managed()) {
            /* This program is managed by TETRIS. The TETRIS server
             * decides where to place this thread. So just ignore this
             * request. */
            return 0;
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_setaffinity_np function. */
            return real_func(thread_id, cpusetsize, cpuset);
        }

    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_setaffinity_np function.\n");
        exit(-1);
    }
}

