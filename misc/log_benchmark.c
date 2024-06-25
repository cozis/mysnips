#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <x86intrin.h>
#include "log.h"
#include "../thread/sync.h"
#include "../lockfree/spsc_queue.h"
#include "../time/clock.h"
#include "../thread/thread.h"

#if defined(__linux__)
#include <errno.h>
#include <unistd.h>
#endif

#define MSG_SIZE 20
#define NUM_THREADS 1000
#define NUM_PRINTS_PER_THREAD 1000

FILE *libc_file;

uint64_t total_elapsed_cycles = 0;
uint64_t total_elapsed_cycles_libc = 0;

os_threadreturn func(void*)
{
    char buf[MSG_SIZE+1];
    memset(buf, 'x', MSG_SIZE);
    buf[MSG_SIZE-1] = '\0';

    uint64_t elapsed_cycles = 0;
    for (int i = 0; i < NUM_PRINTS_PER_THREAD; i++) {
        uint64_t start = __rdtsc();
        log_writef("%s", buf);
        elapsed_cycles += __rdtsc() - start;
    }

    atomic_fetch_add(&total_elapsed_cycles, elapsed_cycles);
    return 0;
}

os_threadreturn func2(void*)
{
    char buf[MSG_SIZE+1];
    memset(buf, 'x', MSG_SIZE);
    buf[MSG_SIZE-1] = '\0';

    uint64_t elapsed_cycles = 0;
    for (int i = 0; i < NUM_PRINTS_PER_THREAD; i++) {
        uint64_t start = __rdtsc();
        fprintf(libc_file, "%s", buf);
        elapsed_cycles += __rdtsc() - start;
    }

    atomic_fetch_add(&total_elapsed_cycles_libc, elapsed_cycles);
    return 0;
}

int main(void)
{
    uint64_t start_cycles = __rdtsc();
    uint64_t start_ns = get_relative_time_ns();

    char file[] = "log_benchmark.txt";

    os_thread desc[NUM_THREADS];
    
    {
        unlink(file);

        log_init(file);
        log_set_flush_timeout(1000);

        for (int i = 0; i < NUM_THREADS; i++)
           os_thread_create(&desc[i], NULL, func);
        for (int i = 0; i < NUM_THREADS; i++)
            os_thread_join(desc[i]);
        log_quit();
    }

    {
        unlink(file);
        libc_file = fopen(file, "wb");
        if (!libc_file) abort();
        for (int i = 0; i < NUM_THREADS; i++)
            os_thread_create(&desc[i], NULL, func2);
        for (int i = 0; i < NUM_THREADS; i++)
            os_thread_join(desc[i]);
        fclose(libc_file);
    }

    uint64_t end_cycles = __rdtsc();
    uint64_t end_ns = get_relative_time_ns();

    long double ns_per_cycle = (long double) (end_ns - start_ns) / (end_cycles - start_cycles);

    uint64_t cycles_per_log = total_elapsed_cycles / (NUM_THREADS * NUM_PRINTS_PER_THREAD);
    long double ns_per_log = cycles_per_log * ns_per_cycle;

    uint64_t cycles_per_fwrite = total_elapsed_cycles_libc / (NUM_THREADS * NUM_PRINTS_PER_THREAD);
    long double ns_per_fwrite = cycles_per_fwrite * ns_per_cycle;

    char temp[128];
    human_readable_time_interval(ns_per_log, temp, sizeof(temp));
    fprintf(stderr, "log_writef -> %s\n", temp);

    human_readable_time_interval(ns_per_fwrite, temp, sizeof(temp));
    fprintf(stderr, "fprintf -> %s\n", temp);

    profile_results_t prof_results[] = {
        log_profile_results(),
        spsc_queue_profile_results(),
        sync_profile_results(),
    };
    print_profile_results(prof_results, sizeof(prof_results)/sizeof(prof_results[0]), ns_per_cycle);

    return 0;
}
