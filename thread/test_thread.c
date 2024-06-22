#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "thread.h"

static _Atomic uint64_t value = 0;
static _Atomic int started_threads = 0;

static os_threadreturn func(void *arg)
{
    atomic_fetch_add(&started_threads, 1);

    fprintf(stderr, "Thread start %d\n", (int) get_thread_id());

    int i = (int) arg;
    typeof(value) mask = (typeof(value)) 1 << i;

    typeof(value) old;
    typeof(value) new;
    do {
        old = atomic_load(&value);
        new = old | mask;
    } while (!atomic_compare_exchange_weak(&value, &old, new));

    fprintf(stderr, "Thread end %d\n", (int) get_thread_id());
    return 0;
}

int main(void)
{
    os_thread tids[8 * sizeof(value)];

    for (int i = 0; i < 8 * (int) sizeof(value); i++)
        os_thread_create(&tids[i], (void*) i, func);

    for (int i = 0; i < 8 * (int) sizeof(value); i++)
        os_thread_join(tids[i]);

    if (value != ~0ULL || started_threads != 8 * sizeof(value)) {
        fprintf(stderr, "FAILED\n");
        abort();
    }
    fprintf(stderr, "PASSED\n");
    return 0;
}
