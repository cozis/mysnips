#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <stdatomic.h>
#include "log.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#include "../lockfree/spsc_queue.h"

PROFILE_GLOBAL_START;

#if defined(_WIN32)
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
typedef void *os_handle;
#define OS_STDOUT ((os_handle) -2)
#define OS_STDERR ((os_handle) -3)
#elif defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
typedef int os_handle;
#define OS_STDOUT 1
#define OS_STDERR 2
#else
#error "Unknown platform"
#endif

#ifndef LOG_BUFFER_LOG2
#define LOG_BUFFER_LOG2 16
#endif

#define LOG_BUFFER_SIZE (1ULL << LOG_BUFFER_LOG2)

typedef struct thread_buffer_t thread_buffer_t;
struct thread_buffer_t {
    thread_buffer_t *next;
    semaphore_t sem;
    spsc_queue_t queue;
    char data[LOG_BUFFER_SIZE];
};

static os_thread    flush_thread_id;
static _Atomic bool can_terminate;
static os_condvar_t can_flush_event;
static os_mutex_t   can_flush_mutex;
static thread_buffer_t *_Atomic global_thread_local_buffer_list;
static _Thread_local thread_buffer_t *thread_local_buffer = NULL;

#define DEST_FILE_NAME_MAX (1<<12)

static os_mutex_t config_mutex;
static bool       config_changed;
static int        config_timeout_ms;
static char       config_dest_file[DEST_FILE_NAME_MAX];

#define OS_INVALID ((os_handle) -1)

#define LOG_PRINT_ERRORS

#ifdef LOG_PRINT_ERRORS
#include <stdio.h>
void abort_(const char *file, int line)
{
    fprintf(stderr, "Aborted %s:%d\n", file, line);
    abort();
}
#define abort() abort_(__FILE__, __LINE__)
#endif

// Only opens for appending
static os_handle os_open(const char *file)
{
    PROFILE_START;

    #if defined(_WIN32)

	os_handle handle = CreateFileA(file, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == OS_STDOUT || handle == OS_STDERR)
        abort();

    PROFILE_END;
    return handle;

    #elif defined(__linux__)

    // TODO: Choose a better mode than 666
    int fd = open(file, O_WRONLY | O_APPEND | O_CREAT, 0666);

    PROFILE_END;
    return fd;

    #else
    (void) file;

    PROFILE_END;
    return OS_INVALID;

    #endif
}

static void os_close(os_handle handle)
{
    PROFILE_START;

    #if defined(_WIN32)
    CloseHandle(handle);
    #elif defined(__linux__)
    close(handle);
    #else
    (void) handle;
    #endif

    PROFILE_END;
}

static int os_write(os_handle handle, const void *data, size_t size)
{
    PROFILE_START

    if (size > INT_MAX) size = INT_MAX;

#if defined(_WIN32)

    if (0) {}
    else if (handle == OS_STDOUT) handle = GetStdHandle(STD_OUTPUT_HANDLE);
    else if (handle == OS_STDERR) handle = GetStdHandle(STD_ERROR_HANDLE);

    unsigned long written;
    if (!WriteFile(handle, data, size, &written, NULL))
        return -1;

    PROFILE_END;
    return (int) written;

#elif defined(__linux__)

    size_t written = 0;
    do {
        int ret = write(handle, data + written, size - written);
        if (ret < 0) {
            if (errno == EINTR)
                ret = 0;
            else
                return -1;
        }
        written += ret;
    } while (written < size);

    PROFILE_END;
    return written;

#else

    PROFILE_END;
    return -1;
#endif
}

void log_set_flush_timeout(int timeout_ms)
{
    PROFILE_START;

    os_mutex_lock(&config_mutex);
    config_timeout_ms = timeout_ms;
    config_changed = true;
    os_mutex_unlock(&config_mutex);

    PROFILE_END;
}

void log_set_dest_file(const char *dest_file)
{
    PROFILE_START;

    size_t len = strlen(dest_file);
    if (len >= DEST_FILE_NAME_MAX)
        abort();

    os_mutex_lock(&config_mutex);
    strncpy(config_dest_file, dest_file, DEST_FILE_NAME_MAX);
    config_changed = true;
    os_mutex_unlock(&config_mutex);

    PROFILE_END;
}

static void wakeup_flush_routine(void)
{
    PROFILE_START;

    os_mutex_lock(&can_flush_mutex);
    os_condvar_signal(&can_flush_event);
    os_mutex_unlock(&can_flush_mutex);

    PROFILE_END;
}

static void flush_thread_local_buffers(os_handle handle)
{
    PROFILE_START;

    thread_buffer_t *cursor = atomic_load(&global_thread_local_buffer_list);
    while (cursor) {

        void *ptr1, *ptr2;
        int   num1, num2;
        spsc_queue_peek_0(&cursor->queue, &ptr1, &num1);
        spsc_queue_peek_1(&cursor->queue, &ptr2, &num2);

        if (num1 + num2 > 0) {

            if (os_write(handle, ptr1, num1) != num1) abort();
            if (os_write(handle, ptr2, num2) != num2) abort();

            spsc_queue_multi_pop(&cursor->queue, NULL, num1 + num2);
            semaphore_signal(&cursor->sem, num1 + num2);
        }

        cursor = cursor->next;
    }

    PROFILE_END;
}

static os_threadreturn flush_routine(void*)
{
    PROFILE_START;

    char dest_file[1<<12];
    int timeout_ms;

    os_mutex_lock(&config_mutex);
    timeout_ms = config_timeout_ms;
    memcpy(dest_file, config_dest_file, DEST_FILE_NAME_MAX);
    os_mutex_unlock(&config_mutex);

    while (!atomic_load(&can_terminate)) {

        os_mutex_lock(&can_flush_mutex);
        os_condvar_wait(&can_flush_event, &can_flush_mutex, timeout_ms);
        os_mutex_unlock(&can_flush_mutex);

        os_mutex_lock(&config_mutex);
        if (config_changed) {
            timeout_ms = config_timeout_ms;
            memcpy(dest_file, config_dest_file, DEST_FILE_NAME_MAX);
            config_changed = false;
        }
        os_mutex_unlock(&config_mutex);

        os_handle handle = os_open(dest_file);
        if (handle == OS_INVALID) abort();
        flush_thread_local_buffers(handle);
        os_close(handle);
    }

    os_handle handle = os_open(dest_file);
    if (handle == OS_INVALID) abort();
    flush_thread_local_buffers(handle);
    os_close(handle);

    PROFILE_END;
    return 0;
}

void log_write(char *data)
{
    PROFILE_START;

    log_write2(data, strlen(data));

    PROFILE_END;
}

void log_write2(char *data, size_t size)
{
    PROFILE_START;

    if (size > 1ULL << LOG_BUFFER_LOG2)
        abort(); // Can't have writes larger than the buffer size

    // If there is no thread-local buffer, make one
    if (thread_local_buffer == NULL) {

        // Allocate the buffer
        thread_buffer_t *b = malloc(sizeof(thread_buffer_t));
        if (b == NULL) abort();
        
        // Init the buffer
        b->next = NULL;
        spsc_queue_init(&b->queue, b->data, LOG_BUFFER_LOG2, sizeof(char));
        semaphore_create(&b->sem, 1ULL << LOG_BUFFER_LOG2);

        // Append to the global list
        thread_buffer_t *old_head;
        thread_buffer_t *new_head;
        do {
            old_head = atomic_load(&global_thread_local_buffer_list);
            b->next = old_head;
            new_head = b;
        } while (!atomic_compare_exchange_weak(&global_thread_local_buffer_list, &old_head, new_head));

        // Use it as this thread's buffer
        thread_local_buffer = b;
    }
    assert(thread_local_buffer);

    {
        int free = spsc_queue_capacity(&thread_local_buffer->queue)
                 - spsc_queue_size(&thread_local_buffer->queue);
        if ((size_t) free < size) wakeup_flush_routine();

        semaphore_wait(&thread_local_buffer->sem, size, -1);
        spsc_queue_multi_push(&thread_local_buffer->queue, data, size);
    }

    PROFILE_END;
}

void log_init(const char *dest_file)
{
    PROFILE_START;

    if (dest_file == NULL || strlen(dest_file) >= DEST_FILE_NAME_MAX)
        abort();

    can_terminate = false;

    os_mutex_create(&config_mutex);
    config_changed = false;
    config_timeout_ms = 1000;
    strncpy(config_dest_file, dest_file, DEST_FILE_NAME_MAX);

    os_condvar_create(&can_flush_event);
    os_mutex_create(&can_flush_mutex);
    atomic_store(&global_thread_local_buffer_list, NULL);

    os_thread_create(&flush_thread_id, NULL, flush_routine);

    PROFILE_END;
}

void log_quit(void)
{
    PROFILE_START;

    atomic_store(&can_terminate, 1);
    wakeup_flush_routine();
    os_thread_join(flush_thread_id);

    thread_buffer_t *cursor = atomic_load(&global_thread_local_buffer_list);
    while (cursor) {
        thread_buffer_t *next = cursor->next;
        semaphore_delete(&cursor->sem);
        free(cursor);
        cursor = next;
    }
    global_thread_local_buffer_list = NULL;
    os_condvar_delete(&can_flush_event);
    os_mutex_delete(&can_flush_mutex);

    os_mutex_delete(&config_mutex);

    PROFILE_END;
}

PROFILE_GLOBAL_END;

profile_results_t log_profile_results(void)
{
    return PROFILE_RESULTS;
}