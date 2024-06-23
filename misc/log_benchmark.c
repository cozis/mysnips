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
#include <unistd.h>
#endif

#define MSG_SIZE 20
#define NUM_THREADS 8
#define NUM_PRINTS_PER_THREAD 100000

uint64_t total_elapsed_cycles = 0;

os_threadreturn func(void*)
{
    char buf[MSG_SIZE+1];
    memset(buf, 'x', MSG_SIZE);
    buf[MSG_SIZE-1] = '\0';

    uint64_t elapsed_cycles = 0;
    for (int i = 0; i < NUM_PRINTS_PER_THREAD; i++) {
        uint64_t start = __rdtsc();
        log_write(buf);
        elapsed_cycles += __rdtsc() - start;
    }

    atomic_fetch_add(&total_elapsed_cycles, elapsed_cycles);
    return 0;
}

void human_readable_time_interval(uint64_t ns, char *dst, size_t max)
{
    if (ns > 1000000000)
        snprintf(dst, max, "%.1Lf s", (long double) ns / 1000000000);
    else if (ns > 1000000)
        snprintf(dst, max, "%.1Lf ms", (long double) ns / 1000000);
    else if (ns > 1000)
        snprintf(dst, max, "%.1Lf us", (long double) ns / 1000);
    else
        snprintf(dst, max, "%.1Lf ns", (long double) ns);
}


void print_profile_results_head()
{
    fprintf(stderr, "| %-30s | %-10s | %-10s | %-10s |\n",
        "Name", "Calls", "Total", "Latency");
}

void print_profile_results(profile_results_t res, long double ns_per_cycle)
{
    for (int i = 0; i < res.count; i++) {
        if (!res.array[i].name) continue;

        int call_count = res.array[i].call_count;
        uint64_t elapsed_cycles = res.array[i].elapsed_cycles;

        long double total_ns = ns_per_cycle * elapsed_cycles;
        long double latency_ns = total_ns / call_count;

        char total[128];        
        human_readable_time_interval(total_ns, total, sizeof(total));

        char latency[128];        
        human_readable_time_interval(latency_ns, latency, sizeof(latency));

        fprintf(stderr, "| %-30s | %-10d | %-10s | %-10s |\n",
            res.array[i].name, call_count, total, latency);
    }
}

int main(void)
{
    uint64_t start_cycles = __rdtsc();
    uint64_t start_ns = get_relative_time_ns();

    char file[] = "log_benchmark.txt";
    unlink(file);

    log_init(file);
    log_set_flush_timeout(1000);

    os_thread desc[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        os_thread_create(&desc[i], NULL, func);
    for (int i = 0; i < NUM_THREADS; i++)
        os_thread_join(desc[i]);
    log_quit();

    uint64_t end_cycles = __rdtsc();
    uint64_t end_ns = get_relative_time_ns();

    long double ns_per_cycle = (long double) (end_ns - start_ns) / (end_cycles - start_cycles);

    uint64_t cycles_per_log = total_elapsed_cycles / (NUM_THREADS * NUM_PRINTS_PER_THREAD);
    long double ns_per_log = cycles_per_log * ns_per_cycle;

    char temp[128];
    human_readable_time_interval(ns_per_log, temp, sizeof(temp));
    fprintf(stderr, "log_write -> %s\n", temp);

    print_profile_results_head();
    print_profile_results(log_profile_results(), ns_per_cycle);
    print_profile_results(spsc_queue_profile_results(), ns_per_cycle);
    print_profile_results(sync_profile_results(), ns_per_cycle);

    return 0;
}
