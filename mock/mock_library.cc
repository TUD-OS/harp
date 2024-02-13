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
 * Support for measuring RAPL energy
 ***/
namespace rapl {

enum class msr_nr : unsigned int {
    PKG = 0x611,
    CORE = 0x639,
    DRAM = 0x619,
    GPU = 0x641,

    UNIT = 0x606
};

enum class msr_offset : unsigned int {
    PKG = 0,
    CORE = PKG,
    DRAM = PKG,
    GPU = PKG,

    UNIT = 8
};

enum class msr_mask : unsigned int {
    PKG = 0xffffffff,
    CORE = PKG,
    DRAM = PKG,
    GPU = PKG,

    UNIT = 0x1f00
};

template <typename EnumClass>
auto to_underlying(const EnumClass &e) -> typename std::underlying_type<EnumClass>::type
{
    return static_cast<typename std::underlying_type<EnumClass>::type>(e);
}

static unsigned int read_msr(const msr_nr &nr, const msr_offset &offset, const msr_mask &mask)
{
    int msr = open("/dev/cpu/0/msr", O_RDONLY);

    if (msr > 0) {
        uint64_t val;

        lseek(msr, to_underlying(nr), SEEK_SET);
        read(msr, &val, sizeof(uint64_t));
        close(msr);

        return (val & to_underlying(mask)) >> to_underlying(offset);
    } else {
        throw std::runtime_error{"Failed to open msr file!"};
    }
}

struct Value {
   public:
    unsigned long pkg;
    unsigned long core;
    unsigned long dram;
    unsigned long gpu;

    static Value read();
};

Value Value::read()
{
    Value val;

    val.pkg = read_msr(msr_nr::PKG, msr_offset::PKG, msr_mask::PKG);
    val.core = read_msr(msr_nr::CORE, msr_offset::CORE, msr_mask::CORE);
    val.dram = read_msr(msr_nr::DRAM, msr_offset::DRAM, msr_mask::DRAM);
    val.gpu = read_msr(msr_nr::GPU, msr_offset::GPU, msr_mask::GPU);

    return val;
}

class Unit {
   private:
    static bool _initialized;
    static unsigned int _unit;

   public:
    static unsigned int get();
};

bool Unit::_initialized = false;
unsigned int Unit::_unit = 1;

unsigned int Unit::get()
{
    if (!_initialized) {
        auto val = read_msr(msr_nr::UNIT, msr_offset::UNIT, msr_mask::UNIT);

        _unit = 1000000 / (1 << val);
        _initialized = true;
    }

    return _unit;
}

unsigned long calculate_consumed(const unsigned long start, const unsigned long end)
{
    if (start <= end)
    {
        return end - start;
    } else {
        // rapl overflow
        return (1UL << 32) - start + end;
    }
}


struct Energy
{
    unsigned long long package;
    unsigned long long core;
    unsigned long long dram;
    unsigned long long gpu;
    unsigned long long big;
    unsigned long long little;

    Energy operator+(const Energy &o) const;
    Energy &operator+=(const Energy &o);
    Energy operator-(const Energy &o) const;
    Energy &operator-=(const Energy &o);
};

Energy Energy::operator+(const Energy &o) const
{
    Energy e{*this};
    e += o;

    return e;
}

Energy &Energy::operator+=(const Energy &o)
{
    package += o.package;
    core += o.core;
    dram += o.dram;
    gpu += o.gpu;
    big += o.big;
    little += o.little;

    return *this;
}

Energy Energy::operator-(const Energy &o) const
{
    Energy e{*this};
    e -= o;

    return e;
}

Energy &Energy::operator-=(const Energy &o)
{
    package -= o.package;
    core -= o.core;
    dram -= o.dram;
    gpu -= o.gpu;
    big -= o.big;
    little -= o.little;

    return *this;
}

Energy consumed_energy(const Value &start, const Value &end)
{
    Energy e;

    e.package = calculate_consumed(start.pkg, end.pkg) * Unit::get();
    e.core = calculate_consumed(start.core, end.core) * Unit::get();
    e.dram = calculate_consumed(start.dram, end.dram) * Unit::get();
    e.gpu = calculate_consumed(start.gpu, end.gpu) * Unit::get();

    return e;
}

class Measure
{
   public:
    virtual ~Measure() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual Energy energy() = 0;
};

class RAPLMeasure : public Measure
{
   private:
    Energy _accum_energy;
    Value _last_rapl;
    bool _running;

   public:
    RAPLMeasure();

    void start();
    void stop();
    void reset();

    Energy energy();
};

RAPLMeasure::RAPLMeasure() :
    _accum_energy{}, _last_rapl{}, _running{false}
{}

void RAPLMeasure::start()
{
    if (_running)
        return;

    _last_rapl = rapl::Value::read();
    _running = true;
}

void RAPLMeasure::stop()
{
    if (!_running)
        return;

    auto current_rapl = rapl::Value::read();
    _accum_energy += consumed_energy(_last_rapl, current_rapl);
    _running = false;
}

void RAPLMeasure::reset()
{
    _accum_energy = {};

    if (this->_running)
        _last_rapl = rapl::Value::read();
}

Energy RAPLMeasure::energy()
{
    if (this->_running) {
        auto current_rapl = rapl::Value::read();

        _accum_energy += consumed_energy(_last_rapl, current_rapl);
        _last_rapl = current_rapl;
    }

    return _accum_energy;
}

class PerfMeasure : public Measure
{
   private:
    bool _running;
    int _fd;
    unsigned int _counter_id;

   public:
    PerfMeasure();
    ~PerfMeasure();

    void start();
    void stop();
    void reset();

    Energy energy();
};

PerfMeasure::PerfMeasure() : _running{false}, _fd{-1}
{
}

PerfMeasure::~PerfMeasure()
{
    stop();
}

void PerfMeasure::start()
{
    if (_running)
        return;

    /* Get the event type */
    FILE *etype_file = fopen("/sys/bus/event_source/devices/power/type", "r");
    int event_type =  0;
    fscanf(etype_file, "%d", &event_type);
    fclose(etype_file);

    /* Get the event sub type for PKG energy */
    FILE *stype_file = fopen("/sys/bus/event_source/devices/power/events/energy-pkg", "r");
    int event_sub_type = 0;
    fscanf(stype_file, "event=%x", &event_sub_type);
    fclose(stype_file);

    /* Now initialize the perf event for the RAPL counters */
    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(pea));

    pea.size = sizeof(struct perf_event_attr);
    pea.type = event_type;
    pea.config = event_sub_type;
    pea.disabled = 1;
    pea.exclude_kernel = 0;

    int tmp_fd = syscall(__NR_perf_event_open, &pea, -1, 0, -1, 0);
    if (tmp_fd == -1) {
        logger->error("Can't start perf measurements!\n");
        return;
    } else {
        _fd = tmp_fd;
        if(ioctl(_fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
            logger->error("Failed to enable perf measurements!\n");
            return;
        }
    }

    _running = true;
}

void PerfMeasure::stop()
{
    if (!_running)
        return;

    close(_fd);
    _fd = -1;
    _counter_id = -1;
    _running = false;
}

void PerfMeasure::reset()
{}

Energy PerfMeasure::energy()
{
    if (!_running)
        return {};

    /* Get the scale for the values */
    std::ifstream scale_file("/sys/bus/event_source/devices/power/events/energy-pkg.scale");
    double scale = 0;
    scale_file >> scale;

    Energy result;
    unsigned long val;
    if (read(_fd, &val, sizeof(val)) == -1) {
        logger->error("Failed to read RAPL values from perf\n");
        return result;
    }

    /* Scale the values to uJ */
    result.package = val * (scale * 1000000.0);

    return result;
}


class PowercapMeasure : public Measure
{
   private:
    bool _running;
    unsigned long _last_value;

   public:
    PowercapMeasure();

    void start();
    void stop();
    void reset();

    Energy energy();
};

PowercapMeasure::PowercapMeasure() :
    _running{false}, _last_value{0}
{}

void PowercapMeasure::start()
{
    if (_running)
        return;

    std::ifstream pfile{"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj"};
    pfile >> _last_value;
    _running = true;
}

void PowercapMeasure::stop()
{
    if (!_running)
        return;

    _last_value = 0;
    _running = false;
}

void PowercapMeasure::reset()
{}

Energy PowercapMeasure::energy()
{
    Energy result;

    if (_running) {
        std::ifstream pfile{"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj"};
        unsigned long cur_value;
        pfile >> cur_value;

        if (cur_value < _last_value) {
            std::ifstream max_file{"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/max_energy_range_uj"};
            unsigned long max_value;
            max_file >> max_value;

            result.package = max_value - _last_value + cur_value;
        } else {
            result.package = cur_value - _last_value;
        }
    }

    return result;
}

class OdroidMeasure : public Measure
{
   private:
    bool _running;
    std::array<float, 3> _last_values;

    std::vector<std::string> sensors = {
        "/sys/bus/i2c/devices/0-0040", /* big */
        "/sys/bus/i2c/devices/0-0041", /* dram */
        "/sys/bus/i2c/devices/0-0045"  /* little */
    };

   public:
    OdroidMeasure();

    void start();
    void stop();
    void reset();

    Energy energy();
};

OdroidMeasure::OdroidMeasure() : _running{false}
{}

void OdroidMeasure::start()
{
    if (_running)
        return;

    int i = 0;
    for (const auto &s : sensors) {
        std::ofstream enable{s + "/enable"};
        enable << "1";
        std::ifstream joules{s + "/sensor_J"};
        joules >> _last_values[i];
        i++;
    }

    _running = true;
}

void OdroidMeasure::stop()
{
    if (!_running)
        return;

    for (const auto &s : sensors) {
        std::ofstream enable{s + "/enable"};
        enable << "0";
    }

    _running = false;
}

void OdroidMeasure::reset()
{
    if (!_running)
        return;

    int i = 0;
    for (const auto &s : sensors) {
        std::ifstream joules{s + "/sensor_J"};
        joules >> _last_values[i];
        i++;
    }
}

Energy OdroidMeasure::energy()
{
    Energy result;

    if (_running) {
        std::array<float, 3> cur_values;
        int i = 0;
        for (const auto &s : sensors) {
            std::ifstream joules{s + "/sensor_J"};
            joules >> cur_values[i];
            i++;
        }

        result.big = (cur_values[0] - _last_values[0]) * 1000000;
        result.dram = (cur_values[1] - _last_values[1]) * 1000000;
        result.little = (cur_values[2] - _last_values[2]) * 1000000;
        result.package = result.big + result.little + result.dram;
    }

    return result;
}

class NoMeasure : public Measure
{
   public:
    void start() {}
    void stop() {}
    void reset() {}

    Energy energy() { return {}; }
};

} /* namespace rapl */


using MeasurePtr = std::unique_ptr<rapl::Measure>;
MeasurePtr energy_measure;


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

    if (getenv("TETRIS_NOMEASURE")) {
        energy_measure = std::make_unique<rapl::NoMeasure>();
    } else {
        if (platform_path.find("odroid") != std::string::npos) {
            logger->debug("Using Odroid on-board sensors for energy measurements\n");
            energy_measure = std::make_unique<rapl::OdroidMeasure>();
        } else {
            /* Check for making RAPL measurements */
            int fd;
            if ((fd = open("/dev/cpu/0/msr", O_RDONLY)) > 0) {     /* Try directly using the MSR */
                close(fd);

                logger->debug("Using RAPL-MSR read for energy measurements\n");
                energy_measure = std::make_unique<rapl::RAPLMeasure>();
            } else {
                logger->debug("Can't open '/dev/cpu/0/msr' for RAPL-MSR based energy measurements!\n");

                if ((fd = open("/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj", O_RDONLY)) > 0) {    /* Try using the powercap interface */
                    close(fd);

                    logger->debug("Using powercap framework for energy measurements\n");
                    energy_measure = std::make_unique<rapl::PowercapMeasure>();
                } else {
                    logger->error("Can't use powercap framework for energy measurements!\n");

                    /* Check for using perf for RAPL registers */
                    std::ifstream perf_paranoa("/proc/sys/kernel/perf_event_paranoid");
                    int paranoa;
                    perf_paranoa >> paranoa;

                    if (paranoa != -1 || geteuid() == 0) {
                        logger->error("Can't use perf for energy measurements!\n");
                        logger->error("No possibility found to make energy measurements!!\n");
                        tetris_possible = false;
                    } else {
                        logger->debug("Using perf for RAPL measurements\n");
                        energy_measure = std::make_unique<rapl::PerfMeasure>();
                    }
                }
            }
        }
    }

    if (tetris_possible){
        /* Start energy measurements */
        energy_measure->start();

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
    auto e= energy_measure->energy();

    struct tms us_time;
    times(&us_time);

    unsigned long total_ns = total_time_ns;
    unsigned long tetris_ns = tetris_time_ns;
    unsigned long user_ms = (us_time.tms_utime * 1000UL)/sysconf(_SC_CLK_TCK);
    unsigned long system_ms = (us_time.tms_stime * 1000UL)/sysconf(_SC_CLK_TCK);
    unsigned long energy_pkg = e.package;

    tetris_client.reset();

    unsigned long _ns = tetris_time_ns;
    unsigned long ms = _ns / 1000000;
    unsigned us = (_ns % 1000000) / 1000;
    unsigned ns = _ns % 1000;
    logger->always("Total time spent in TETRIS: %lu.%03u%03lu ms (%lu ns)\n", ms, us, ns, _ns);

    logger->always("total(ns);tetris(ns);user(ms);system(ms);pkg(uJ)\n%lu;%lu;%lu;%lu;%lu\n", total_ns, tetris_ns, user_ms, system_ms, energy_pkg);
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
