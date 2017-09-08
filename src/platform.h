// Platform specific code
#pragma once

#ifdef _WIN64
    // Windows
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
    typedef HANDLE thrd_t;
    #define thrd_create(thrd, func, args) *thrd = CreateThread(NULL, 0, \
        (LPTHREAD_START_ROUTINE)func, args, 0, NULL)
    #define thrd_join(thrd, dummy) WaitForSingleObject(thrd, INFINITE), CloseHandle(thrd)
    #define thrd_sleep(msec) Sleep(msec)

    // Timer
    static inline int64_t system_msec() {
        LARGE_INTEGER t, f;
        QueryPerformanceCounter(&t);
        QueryPerformanceFrequency(&f);
        return 1000LL * t.QuadPart / f.QuadPart;
    }
#else
    // POSIX
    #include <pthread.h>

    // Locks
    typedef pthread_mutex_t mtx_t;
    #define mtx_init(m, t) pthread_mutex_init(m, NULL)
    #define mtx_destroy(m) pthread_mutex_destroy(m)
    #define mtx_lock(m) pthread_mutex_lock(m)
    #define mtx_unlock(m) pthread_mutex_unlock(m)

    // Threads
    typedef pthread_t thrd_t;
    #define thrd_create(thrd, func, args) pthread_create(thrd, 0, (void*(*)(void*))func, args)
    #define thrd_join(thrd, dummy) pthread_join(thrd, NULL)
    #define thrd_sleep(msec) nanosleep(&(struct timespec){.tv_sec = msec / 1000, \
        .tv_nsec = (msec % 1000) * 1000000LL}, NULL)

    // Timer
    static inline int64_t system_msec() {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
    }
#endif
