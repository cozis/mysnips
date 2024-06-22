#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32)
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
typedef void *os_thread;
typedef unsigned long os_threadreturn;
#elif defined(__linux__)
#include <pthread.h>
typedef pthread_t os_thread;
typedef void *os_threadreturn;
#endif

uint64_t get_thread_id(void);

void            os_thread_create(os_thread *thread, void *arg, os_threadreturn (*func)(void*));
os_threadreturn os_thread_join(os_thread thread);
