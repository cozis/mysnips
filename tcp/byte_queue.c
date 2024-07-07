#include <stdlib.h>
#include <string.h>
#include "byte_queue.h"

void byte_queue_init(ByteQueue *q)
{
    q->data = NULL;
    q->head = 0;
    q->size = 0;
    q->capacity = 0;
}

void byte_queue_free(ByteQueue *q)
{
    free(q->data);
}

size_t byte_queue_used_space(ByteQueue *q)
{
    return q->size;
}

size_t byte_queue_free_space(ByteQueue *q)
{
    return q->capacity - q->size - q->head;
}

bool byte_queue_ensure_min_free_space(ByteQueue *q, size_t num)
{
    size_t total_free_space = q->capacity - q->size;
    size_t free_space_after_data = q->capacity - q->size - q->head;

    if (free_space_after_data < num) {
        if (total_free_space < num) {
            // Resize required

            size_t capacity = 2 * q->capacity;
            if (capacity - q->size < num) capacity = q->size + num;

            char *data = malloc(capacity);
            if (!data) return false;

            if (q->size > 0)
                memcpy(data, q->data + q->head, q->size);

            free(q->data);
            q->data = data;
            q->capacity = capacity;

        } else {
            // Move required
            memmove(q->data, q->data + q->head, q->size);
            q->head = 0;
        }
    }

    return true;
}

char *byte_queue_start_write(ByteQueue *q)
{
    return q->data + q->head + q->size;
}

void byte_queue_end_write(ByteQueue *q, size_t num)
{
    q->size += num;
}

char *byte_queue_start_read(ByteQueue *q)
{
    return q->data + q->head;
}

void byte_queue_end_read(ByteQueue *q, size_t num)
{
    q->head += num;
    q->size -= num;
}