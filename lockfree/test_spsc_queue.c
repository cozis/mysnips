#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "spsc_queue.h"
#include "../thread/thread.h"

#define QUEUE_SIZE_LOG2 10

char  input[1<<20];
char output[1<<20];

int  input_used = 0;
int output_used = 0;

char queue_data[1<<QUEUE_SIZE_LOG2];
spsc_queue_t queue;

_Atomic int done = 0;

os_threadreturn writer_func(void*)
{
    while (output_used < (int) sizeof(output)) {

        int remain = (int) sizeof(output) - output_used;
        int cap = spsc_queue_capacity(&queue);
        int limit = remain < cap ? remain : cap;

        int count = 1 + rand() % limit;

        for (int i = 0; i < count; i++)
            output[output_used + i] = rand() & 0xFF;

        while (!spsc_queue_multi_push(&queue, output + output_used, count));

        output_used += count;
    }
    atomic_store(&done, 1);
    return 0;
}

os_threadreturn reader_func(void*)
{
    for (;;) {
        int count = spsc_queue_size(&queue);
        if (count == 0) {
            if (done) break;
            continue;
        }
        assert(input_used + count <= (int) sizeof(input));

        bool ok = spsc_queue_multi_pop(&queue, input + input_used, count);
        assert(ok);

        input_used += count;
    }
    return 0;
}

int main(void)
{
    os_thread writer_id;
    os_thread reader_id;
    spsc_queue_init(&queue, queue_data, QUEUE_SIZE_LOG2, sizeof(char));
    os_thread_create(&writer_id, NULL, writer_func);
    os_thread_create(&reader_id, NULL, reader_func);
    os_thread_join(writer_id);
    os_thread_join(reader_id);
    fprintf(stderr, "Produced: %d\n", output_used);
    fprintf(stderr, "Consumed: %d\n", input_used);

    if (input_used != output_used) abort();
    if (memcmp(input, output, input_used))
        abort();
    else
        fprintf(stderr, "Match\n");
    return 0;
}
