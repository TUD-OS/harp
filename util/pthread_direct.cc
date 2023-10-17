#include "util/pthread_direct.h"

#include <dlfcn.h>
#include <stdlib.h>


/* We need to have our own pthread create function in order to circumvent the LD_PRELOAD fetch that we make
 * at a different point. */
extern "C"
int direct_pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
                   void *(*routine)(void *), void *arg)
{
    using real_func_t = int (*)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);

    /* Get the real pthread_create function. */
    real_func_t real_func = nullptr;
    real_func = reinterpret_cast<real_func_t>(dlsym(RTLD_NEXT, "pthread_create"));

    if (real_func != nullptr) {
        return real_func(thread_id, attr, routine, arg);
    } else {
        /* Something went wrong while getting the function. ABORT */
        exit(-1);
    }
}

