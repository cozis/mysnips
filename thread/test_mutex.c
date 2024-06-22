#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "thread.h"
#include "sync.h"

#define NUM_THREADS 1000

static os_mutex_t mutex;
static uint64_t value = 0;
static int iter_per_thread = 1000000;

static os_threadreturn func(void *arg)
{
    (void) arg;
    for (int i = 0; i < iter_per_thread; i++) {
        os_mutex_lock(&mutex);
        value++;
        os_mutex_unlock(&mutex);
    }
    return 0;
}

int main(void)
{
    os_thread tids[NUM_THREADS];

    os_mutex_create(&mutex);

    uint64_t expected = iter_per_thread * NUM_THREADS;
    for (int i = 0; i < 8 * (int) sizeof(value); i++)
        os_thread_create(&tids[i], (void*) i, func);

    for (int i = 0; i < 8 * (int) sizeof(value); i++)
        os_thread_join(tids[i]);
    
    os_mutex_delete(&mutex);

    if (expected != value) {
        fprintf(stderr, "FAILED\n");
        abort();
    }
    fprintf(stderr, "PASSED\n");
    return 0;
}
