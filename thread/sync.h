#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
typedef CRITICAL_SECTION   os_mutex_t;
typedef CONDITION_VARIABLE os_condvar_t;
#elif defined(__linux__)
#include <pthread.h>
#include <semaphore.h>
typedef pthread_mutex_t os_mutex_t;
typedef pthread_cond_t  os_condvar_t;
#endif

typedef struct {
#ifdef _WIN32
    void *data;
#else
    sem_t data;
#endif
} os_semaphore_t;

typedef struct {
    int count;
    os_mutex_t mutex;
    os_condvar_t cond;
} semaphore_t;

void os_mutex_create(os_mutex_t *mutex);
void os_mutex_delete(os_mutex_t *mutex);
void os_mutex_lock  (os_mutex_t *mutex);
void os_mutex_unlock(os_mutex_t *mutex);

void os_condvar_create(os_condvar_t *condvar);
void os_condvar_delete(os_condvar_t *condvar);
bool os_condvar_wait  (os_condvar_t *condvar, os_mutex_t *mutex, int timeout_ms);
void os_condvar_signal(os_condvar_t *condvar);

void semaphore_create(semaphore_t *sem, int count);
void semaphore_delete(semaphore_t *sem);
bool semaphore_wait  (semaphore_t *sem, int count, int timeout_ms);
void semaphore_signal(semaphore_t *sem, int count);

bool os_semaphore_create(os_semaphore_t *sem, int count, int max);
bool os_semaphore_delete(os_semaphore_t *sem);
bool os_semaphore_wait  (os_semaphore_t *sem);
bool os_semaphore_signal(os_semaphore_t *sem);
