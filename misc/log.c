#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include "log.h"
#include "threads.h"
#include "spsc_queue.h"

#define thrlocal _Thread_local
#define UNREACHABLE assert(0)

#ifdef _WIN32
#define OS_WINDOWS 1
#define OS_LINUX   0
#define OS_OTHER   0
#elif defined(__linux__)
#define OS_WINDOWS 0
#define OS_LINUX   1
#define OS_OTHER   0
#else
#define OS_WINDOWS 0
#define OS_LINUX   0
#define OS_OTHER   1
#endif

#if OS_WINDOWS

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

typedef void *os_handle;
#define OS_STDOUT ((os_handle) -2)
#define OS_STDERR ((os_handle) -3)

#elif OS_LINUX

typedef int os_handle;
#define OS_STDOUT 1
#define OS_STDERR 2

typedef void *os_thread_return_type;

#else

#error "Unknown platform"
#endif

#define OS_INVALID ((os_handle) -1)

// Only opens for appending
static os_handle os_open(const char *file)
{
    #if OS_WINDOWS
	os_handle handle = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == OS_STDOUT || handle == OS_STDERR) abort();
    return handle;
    #endif

    #if OS_LINUX
    int fd = open(file, );
    #endif
}

static void os_close(os_handle handle)
{
    #if OS_WINDOWS
    CloseHandle(handle);
    #endif

    #if OS_LINUX
    close(handle);
    #endif
}

static int os_write(os_handle handle, const void *data, size_t size)
{
    if (size > INT_MAX) size = INT_MAX;

    #if OS_WINDOWS
    if (handle == OS_STDOUT) handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == OS_STDERR) handle = GetStdHandle(STD_ERROR_HANDLE);
    unsigned long written;
    if (!WriteFile(handle, data, size, &written, NULL)) return -1;
    return (int) written;
    #endif

    #if OS_WRITE
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
    return written;
    #endif

    #if OS_OTHER
    return -1;
    #endif
}

void logfatalraw2(const char *str, size_t len)
{
    os_write(OS_STDERR, str, len);
    abort();
}

void logfatalraw(const char *str)
{
    os_write(OS_STDERR, str, strlen(str));
    abort();
}

void logfatalfv(const char *fmt, va_list args)
{
    char buffer[1<<12];
    int num = vsnprintf(buffer, sizeof(buffer), fmt, args);
    if (num > 0)
        logfatalraw2(buffer, num);
    else
        abort();
}

void logfatalf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logfatalfv(fmt, args);
    va_end(args);
}

static logmallocfn mallocfn = malloc;
static logfreefn     freefn = free;
static void        *userptr = NULL;

static void *logmalloc(size_t num)
{
    if (mallocfn == NULL)
        return NULL;
    return mallocfn(num, userptr);
}

static void logfree(void *ptr, size_t num)
{
    if (freefn) freefn(ptr, num, userptr);
}

typedef struct thrctx thrctx;
struct thrctx {
    thrctx *next;
    semaphore_t sem;
    spsc_queue_t queue;
    char data[];
};

static size_t caplog2 = 1<<12;
static struct thrctx *context_list;

static bool startedlogging(void)
{
    return context_list != NULL;
}

void logsetalloc(logmallocfn mallocfn_,
                 logfreefn freefn_,
                 void *userptr_)
{
    mallocfn = mallocfn_;
    freefn = freefn_;
    userptr = userptr_;    
}

void lograw(const char *str)
{
    lograw2(str, strlen(str));
}

static thrctx *get_or_create_context(void)
{
    static thrlocal thrctx *context = NULL;
    if (context == NULL) {

        size_t capacity = 1ULL << caplog2;
        
        thrctx *c = logmalloc(capacity);
        if (c == NULL) return;
        
        semaphore_create(&c->sem, capacity);
        spsc_queue_init(&c->queue, c->data, caplog2, sizeof(char));
        c->next = NULL;

        thrctx *head;
        do {
            head = context_list;
            c->next = head;
        } while (!atomic_compare_exchange_weak(&context_list, &head, c));

        context = c;
    }
    return context;
}

void lograw2(const char *str, size_t len)
{
    thrctx *context = get_or_create_context();
    semaphore_wait(&context->sem, len, -1);
    spsc_queue_multi_push(&context->queue, str, len);
}

void logf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logfv(fmt, args);
    va_end(args);
}

void logfv(const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);

    char buffer[1<<12];
    int num = vsnprintf(buffer, sizeof(buffer), fmt, args);
    if (num < 0) {
        va_end(args2);
        logfatalf("Invalid format string [%s]", fmt);
        UNREACHABLE;
    }

    if (num < sizeof(buffer))
        lograw2(buffer, (size_t) num);
    else {
        char *buffer2 = logmalloc(num+1);
        if (buffer2 == NULL) {
            va_end(args2);
            logfatalraw("Out of memory");
            UNREACHABLE;
        }
        vsnprintf(buffer2, num + 1, fmt, args2);
        va_end(args2);
        lograw2(buffer2, (size_t) num);
        logfree(buffer2, num+1);
    }
}

static os_mutex_t   flush_mutex;
static os_condvar_t flush_cond;
static int flush_interval_ms = 1000;

void logconfig(logconfigname name, int value)
{
    if (startedlogging()) {
        logfatalraw("Logger must be configured before the first log");
        UNREACHABLE;
    }

    switch (name) {
        
        case LOG_FLUSH_INTERVAL_MS:
        flush_interval_ms = value;
        break;

        case LOG_BUFFER_SIZE_LOG2:
        caplog2 = value;
        break;
        
        default:
        logfatalf("Unknown config %d\n", name);
        UNREACHABLE;
        break;
    }
}

static void flush_thread_buffer(thrctx *c, os_handle file)
{
    void *ptr_0, *ptr_1;
    int   num_0,  num_1;
    spsc_queue_peek_0(&c->queue, &ptr_0, &num_0);
    spsc_queue_peek_1(&c->queue, &ptr_1, &num_1);

    int ret;
    int written = 0;

    ret = os_write(file, ptr_0, num_0);
    if (ret < 0) abort();
    written += ret;

    ret = os_write(file, ptr_1, num_1);
    if (ret < 0) abort();
    written += ret;

    spsc_queue_multi_pop(&c->queue, NULL, written);
    semaphore_signal(&c->sem, written);
}

static void flush_all_thread_buffers(char *file_name)
{
    os_handle file = os_open(file_name);
    if (file == OS_INVALID) {
        logfatalf("Could not open '%s'\n", file_name);
        UNREACHABLE;
    }

    thrctx *c = context_list;
    while (c) {
        flush_thread_buffer(c, file);
        c = c->next;
    }

    os_close(file);
}

static os_thread_return_type flush_routine(void *arg)
{
    char file_name[] = "log.txt";

    for (;;) {

        os_mutex_lock(&flush_mutex);
        os_condvar_wait(&flush_cond, &flush_mutex, flush_interval_ms);
        os_mutex_unlock(&flush_mutex);

        flush_all_thread_buffers(file_name);
    }
}
