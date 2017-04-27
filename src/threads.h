// glibc doesn't have C11 threads.h yet. So write a crude replacement, for just
// what is needed, but exposing a C11 API to make the client code future-proof.
#pragma once
#include <pthread.h>

typedef pthread_mutex_t mtx_t;

enum {mtx_plain/*, mtx_recursive, mtx_timed*/};
enum {thrd_success/*, thrd_timedout, thrd_busy, thrd_error, thrd_nomem*/};

/* Return thread_success if ok */
#define mtx_init(m, t) pthread_mutex_init(m, NULL)
#define mtx_destroy(m) pthread_mutex_destroy(m)
#define mtx_lock(m) pthread_mutex_lock(m)
#define mtx_unlock(m) pthread_mutex_unlock(m)
