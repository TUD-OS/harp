#include "client/features/scalable_application.h"
#include "util/debug_util.h"

#include <dlfcn.h>


std::atomic_int parallel_threads;

/* OMP scaling callback */
bool scale_application_cb_omp(int nr_threads)
{
    LOGGER->debug("Setting scaling factor to %d\n", nr_threads);
    parallel_threads = nr_threads;
    return true;
}

class OMPScaler : public tetris::ScalableApplication {
public:
    OMPScaler() : ScalableApplication{scale_application_cb_omp}
    {}

    int current_scale() const {
        return parallel_threads;
    }
};

/* Function used by libtetrisclientlegacy to get this scaler */
extern "C"
tetris::ScalableApplication* get_application_scaler() {
    OMPScaler *instance = new OMPScaler;

    return instance;
}
