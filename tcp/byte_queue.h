#ifndef BYTE_QUEUE_H
#define BYTE_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char  *data;
    size_t head;
    size_t size;
    size_t capacity;
} ByteQueue;

void   byte_queue_init(ByteQueue *q);
void   byte_queue_free(ByteQueue *q);
size_t byte_queue_used_space(ByteQueue *q);
size_t byte_queue_free_space(ByteQueue *q);
char  *byte_queue_start_write(ByteQueue *q);
char  *byte_queue_start_read(ByteQueue *q);
void   byte_queue_end_write(ByteQueue *q, size_t num);
void   byte_queue_end_read(ByteQueue *q, size_t num);
bool   byte_queue_ensure_min_free_space(ByteQueue *q, size_t num);

#endif