#include "util/debug_util.h"
#include "mock/mock_client.h"

#include "client/features/movable_threads.h"
#include "client/features/scalable_application.h"

#include <oneapi/tbb/global_control.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/perf_event.h>
#include <asm/unistd.h>


using namespace tetris;


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
 * Global Variables and Constants
 ***/

using Timer = TimeKeeper<std::atomic_ulong, std::chrono::system_clock, std::chrono::nanoseconds>;
using TimerPtr = std::unique_ptr<Timer>;

using ClientPtr = std::unique_ptr<tetris::MockClient>;
using MovableThreadsPtr = std::unique_ptr<tetris::MovableThreads>;
using ScalableApplicationPtr = std::unique_ptr<tetris::ScalableApplication>;

debug::LoggerPtr logger;
ClientPtr tetris_client;
MovableThreadsPtr movable_threads;
ScalableApplicationPtr scalable_app;

std::atomic_ulong tetris_time_ns;
std::atomic_ulong total_time_ns;

TimerPtr total_timer;


/***
 * Checking for scalable application
 ***/
struct CheckerState
{
    enum Type {
        GOMP = 1,
        OMP = 2,
        INTEL_TBB = 3,
    };

    bool is_scalable = false;
    Type scale_type;
};

enum ParallelLibrary {
    NONE = 0,
    GOMP = 1,
    OMP = 2,
    INTEL_TBB = 3
};

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
    } else if ((strstr( i->dlpi_name, "libtbb") != NULL)) {
        logger->info(" -> Found scalable Intel TBB application (libtbb)\n");
        cstate->is_scalable = true;
        cstate->scale_type = CheckerState::INTEL_TBB;
        return 1;
    }

    return 0;
}

static ParallelLibrary is_scalable_app() {
    CheckerState cstate;
    logger->info(" -> Searching for scalable app\n");

    dl_iterate_phdr(scalable_app_checker, &cstate);

    if (cstate.is_scalable)
        return static_cast<ParallelLibrary>(cstate.scale_type);
    else
        return ParallelLibrary::NONE;
}

std::atomic_int parallel_threads;

bool scale_application_cb_omp(int nr_threads)
{
    logger->debug("Setting scaling factor to %d\n", nr_threads);
    parallel_threads = nr_threads;
    return true;
}

bool scale_application_cb_tbb(int nr_threads)
{
    static oneapi::tbb::global_control *global_limit = nullptr;

    logger->debug("Setting scaling factor TBB to %d\n", nr_threads);

    if (global_limit) {
        delete global_limit;
    }

    global_limit = new oneapi::tbb::global_control(oneapi::tbb::global_control::max_allowed_parallelism, nr_threads);
    return true;
}


/***
 * Library setup and tierdown
 ***/

extern "C"
void __attribute__((constructor)) setup(void)
{
    /* Start the total timer */
    total_time_ns = 0;
    total_timer = std::make_unique<Timer>(total_time_ns);

    /* Start our local TETRiS overhead timer */
    tetris_time_ns = 0;
    Timer t{tetris_time_ns};

    logger = debug::Logger::get();

    logger->info("=> Welcome to the TETRiS MOCK client! <=\n");
    logger->info("Loading TETRIS support\n");
    bool tetris_possible = true;

    /* Get the platform from the environment */
    if (!getenv("TETRIS_PLATFORM")) {
        logger->error("Missing TETRIS_PLATFORM definition!\n");
        tetris_possible = false;
    }
    std::string platform_path = getenv("TETRIS_PLATFORM");

    /* Check for using perf for RAPL registers */
    if (geteuid() != 0 && getuid() != 0) {
        std::ifstream perf_paranoa("/proc/sys/kernel/perf_event_paranoid");
        int paranoa;
        perf_paranoa >> paranoa;

        if (paranoa != -1) {
            logger->warning("Energy measurements might not be possible!\n");
        }
    }

    if (tetris_possible){
        /* Initialize all the features */
        movable_threads = std::make_unique<tetris::MovableThreads>();
        auto parallel_library = is_scalable_app();
        if (parallel_library != ParallelLibrary::NONE) {
            parallel_threads = 0;
            if ((parallel_library == ParallelLibrary::GOMP) || (parallel_library == ParallelLibrary::OMP)) {
                scalable_app = std::make_unique<tetris::ScalableApplication>(scale_application_cb_omp);
            } else if (parallel_library == ParallelLibrary::INTEL_TBB) {
                scalable_app = std::make_unique<tetris::ScalableApplication>(scale_application_cb_tbb);
            }
        }

        /* Connect the client */
        tetris_client = std::make_unique<tetris::MockClient>(tetris::SERVER_SOCKET, platform_path);
        if (tetris_client->is_managed()) {
            logger->info("->> Managed by TETRIS <<-\n");

            /* Register with TETRiS that this client supports movable threads */
            if (movable_threads) {
                logger->info("->> Register as application with movable threads\n");
                tetris_client->bind(movable_threads.get());

                /* Register the main thread */
                movable_threads->register_thread(getpid());
            }

            if (scalable_app) {
                logger->info("->> Register as scalable application\n");
                tetris_client->bind(scalable_app.get());
            }
        } else {
            logger->info("->> NOT managed by TETRIS <<-\n");
        }
    } else {
        logger->info("Prerequisites not met for proper TETRiS support!\n");
    }
}

extern "C"
void __attribute__((destructor)) tierdown(void)
{
    if (!tetris_client)
        return;

    Timer t{tetris_time_ns};

    /* Get time and energy measurements */
    total_timer->stop();

    struct tms us_time;
    times(&us_time);

    unsigned long total_ns = total_time_ns;
    unsigned long tetris_ns = tetris_time_ns;
    unsigned long user_ms = (us_time.tms_utime * 1000UL)/sysconf(_SC_CLK_TCK);
    unsigned long system_ms = (us_time.tms_stime * 1000UL)/sysconf(_SC_CLK_TCK);

    tetris_client.reset();

    unsigned long _ns = tetris_time_ns;
    unsigned long ms = _ns / 1000000;
    unsigned us = (_ns % 1000000) / 1000;
    unsigned ns = _ns % 1000;
    logger->always("Total time spent in TETRIS: %lu.%03u%03lu ms (%lu ns)\n", ms, us, ns, _ns);

    logger->always("total(ns);tetris(ns);user(ms);system(ms)\n%lu;%lu;%lu;%lu\n", total_ns, tetris_ns, user_ms, system_ms);
}


/***
 * pthread wrapper
 ***/

struct ThreadInfo
{
    pthread_t *pthread_id;
    pid_t tid;
    char name[100];

    void *(*func)(void *);

    void *arg;
};

static
void *thread_wrapper(void *arg)
{
    Timer t{tetris_time_ns};
    ThreadInfo *ti = static_cast<ThreadInfo*>(arg);

    /* Update the tid information in the ThreadInfo struct for this thread. */
    auto tid = syscall(SYS_gettid);
    auto tfunc = ti->func;
    auto targ = ti->arg;
    delete ti;

    movable_threads->register_thread(tid);

    /* Call the actual function. */
    t.stop();
    void *ret = tfunc(targ);

    movable_threads->unregister_thread(tid);

    /* If necessary we can do some tear down before returning */
    return ret;
}

extern "C"
int pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
                   void *(*routine)(void *), void *arg)
{
    using real_func_t = int (*)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);

    Timer t{tetris_time_ns};

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

            ti->func = routine;
            ti->arg = arg;

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
[[noreturn]] void pthread_exit(void *retval)
{
    using real_func_t = void (*)(void *);

    Timer t{tetris_time_ns};

    /* Get the real pthread_create function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_exit"));

    if (real_func != nullptr) {
        if (tetris_client && tetris_client->is_managed()) {
            auto tid = syscall(SYS_gettid);
            movable_threads->unregister_thread(tid);

            t.stop();
            real_func(retval);
        } else {
            /* The program is NOT managed by TETRIS. Just call the real
             * pthread_create function. */

            t.stop();
            real_func(retval);
        }
    } else {
        /* Something went wrong while getting the function. ABORT */
        logger->error("Failed to get real pthread_create function.\n");
        exit(-1);
    }
}


/***
 * libgomp wrappers
 ***/
extern "C"
void GOMP_parallel (void (*fn) (void*), void *data, unsigned int num_threads, unsigned int flags)
{
    using real_func_t = void (*)(void (*) (void*), void *, unsigned int, unsigned int);

    Timer t{tetris_time_ns};

    /* Get the real GOMP_parallel function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "GOMP_parallel"));

    if (real_func) {
        if (parallel_threads != 0) {
            /* Call the function with our internal parallel thread count if already set */
            unsigned int own_num_threads = parallel_threads;
            t.stop();
            real_func(fn, data, own_num_threads, flags);
        } else {
            /* Otherwise use the given num_threads as parallel thread count */
            t.stop();
            real_func(fn, data, num_threads, flags);
        }
    } else {
        logger->error("Failed to get real GOMP_parallel function.\n");
        exit(-1);
    }
}
