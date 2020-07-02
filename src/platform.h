// Platform specific code
#pragma once
#include <inttypes.h>
#include <pthread.h>

#ifdef _WIN64
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    // Locks
    typedef CRITICAL_SECTION mtx_t;
    #define mtx_init(m, t) InitializeCriticalSection(m)
    #define mtx_destroy(m) DeleteCriticalSection(m)
    #define mtx_lock(m) EnterCriticalSection(m)
    #define mtx_unlock(m) LeaveCriticalSection(m)

    // Threads
    #define sleep_msec(msec) Sleep(msec)

    // Timer
    static inline int64_t system_msec(void) {
        LARGE_INTEGER t, f;
        QueryPerformanceCounter(&t);
        QueryPerformanceFrequency(&f);
        return 1000LL * t.QuadPart / f.QuadPart;
    }
#else
    // Locks
    typedef pthread_mutex_t mtx_t;
    #define mtx_init(m, t) pthread_mutex_init(m, NULL)
    #define mtx_destroy(m) pthread_mutex_destroy(m)
    #define mtx_lock(m) pthread_mutex_lock(m)
    #define mtx_unlock(m) pthread_mutex_unlock(m)

    // Threads
    #define sleep_msec(msec) nanosleep(&(struct timespec){.tv_sec = msec / 1000, \
        .tv_nsec = (msec % 1000) * 1000000LL}, NULL)

    // Timer
    static inline int64_t system_msec(void) {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
    }
#endif
