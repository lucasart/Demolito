#pragma once
#include <assert.h>
#include <pthread.h>

typedef pthread_mutex_t mtx_t;

enum {mtx_plain, mtx_recursive, mtx_timed};
enum {thrd_success, thrd_timedout, thrd_busy, thrd_error, thrd_nomem};

static inline int mtx_init(mtx_t *mtx, int type)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    if(type & mtx_timed)
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_TIMED_NP);

    if(type & mtx_recursive)
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    const int res = pthread_mutex_init(mtx, &attr) == 0 ? thrd_success : thrd_error;
    pthread_mutexattr_destroy(&attr);
    return res;
}

static inline void mtx_destroy(mtx_t *mtx)
{
    pthread_mutex_destroy(mtx);
}

static inline int mtx_lock(mtx_t *mtx)
{
    return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_unlock(mtx_t *mtx)
{
    return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}
