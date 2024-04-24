#include "client/features/scalable_application.h"
#include "util/debug_util.h"

#include <oneapi/tbb/global_control.h>

static std::atomic_int parallel_threads;

/* Intel TBB scaling callback */
bool scale_application_cb_tbb(int nr_threads)
{
    static oneapi::tbb::global_control *global_limit = nullptr;

    LOGGER->debug("Setting scaling factor TBB to %d\n", nr_threads);

    if (global_limit) {
        delete global_limit;
    }

    parallel_threads = nr_threads;

    global_limit = new oneapi::tbb::global_control(oneapi::tbb::global_control::max_allowed_parallelism, nr_threads);

    return true;
}


class TBBScaler : public tetris::ScalableApplication {
public:
    TBBScaler() : ScalableApplication{scale_application_cb_tbb}
    {}

    int current_scale() const {
        return parallel_threads;
    }
};

/* Function used by libtetrisclientlegacy to get this scaler */
extern "C"
tetris::ScalableApplication* get_application_scaler() {
    TBBScaler *instance = new TBBScaler;

    return instance;
}
