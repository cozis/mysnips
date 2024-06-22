#include <stdatomic.h>

#ifdef _WIN32
#else
#include <stdlib.h>
#endif

#include "thread.h"

void os_thread_create(os_thread *thread, void *arg, os_threadreturn (*func)(void*))
{
    #if defined(_WIN32)
    os_thread thread_ = CreateThread(NULL, 0, func, arg, 0, NULL);
    if (thread_ == INVALID_HANDLE_VALUE)
        abort();
    *thread = thread_;
    #elif defined(__linux__)
    int ret = pthread_create(thread, NULL, func, arg);
    if (ret) abort();
    #endif
}

os_threadreturn os_thread_join(os_thread thread)
{
    #if defined(_WIN32)
    os_threadreturn result;
    WaitForSingleObject(thread, INFINITE);
    if (!GetExitCodeThread(thread, &result))
        abort();
    CloseHandle(thread);
    return result;
    #elif defined(__linux__)
    os_threadreturn result;
    int ret = pthread_join(thread, &result);
    if (ret) abort();
    return result;
    #else
    (void) thread;
    #endif
}

uint64_t get_thread_id(void)
{
    static _Atomic uint64_t next_id = 1;
    static _Thread_local uint64_t id = 0;
    if (id == 0) id = atomic_fetch_add(&next_id, 1);
    return id;
}
