#ifndef SPMC_QUEUE_H
#define SPMC_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct {
    void  *data;
    size_t item_size;
    size_t capacity_log2;
    _Atomic uint64_t head;
    _Atomic uint64_t tail;
} spmc_queue;

void spmc_queue_create(spmc_queue *queue, void *data, size_t item_size, size_t capacity_log2);
bool spmc_queue_push(spmc_queue *queue, void *item);
bool spmc_queue_pop(spmc_queue *queue, void *item);

#endif