#include "pthread.h"

extern "C"
int direct_pthread_create(pthread_t *thread_id, const pthread_attr_t *attr, void *(*routine)(void *), void *arg);
