#include "spmc_queue.h"

// !!! NOT TESTED !!!

void spmc_queue_create(spmc_queue *queue, void *data, size_t item_size, size_t capacity_log2)
{
    queue->data = data;
    queue->item_size = item_size;
    queue->capacity_log2 = capacity_log2;
    queue->head = 0;
    queue->tail = 0;
}

bool spmc_queue_push(spmc_queue *queue, void *item)
{
    uint32_t capacity = 1U << queue->capacity_log2;
    uint64_t mask = capacity - 1;

    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);
    assert(tail - head <= capacity); // No funny business here!
    if (tail - head == capacity)
        return false;
  
    uint32_t tail_index = (uint32_t) (tail & mask);
    memcpy((char*) queue->data + tail_index * queue->item_size, item, queue->item_size);

    assert(tail < UINT64_MAX); // We assume u64s don't overflow
    tail++;

    atomic_fetch_add(&queue->tail, tail);
    return true;
}

bool spmc_queue_pop(spmc_queue *queue, void *item)
{
    uint32_t capacity = 1U << queue->capacity_log2;
    uint64_t mask = capacity - 1;

    uint64_t head;
    do {
        // Read tail before head so that we underestimate
        // the item count instead of overestimating it
        uint64_t tail = atomic_load(&queue->tail);
        head = atomic_load(&queue->head);

        // In general tail can't be greater than head, but
        // here we are comparing head and tail from different
        // instances in time.
        if (head >= tail)
            return false;
        
        // It's possible that some other thread reads of writes
        // this slot under our feet, resulting in garbage being
        // copied in "item". If this happens, surely the CAS
        // condition will fail causing the discard of the read
        // value.
        uint32_t head_index = (uint32_t) (head & mask);
        memcpy(item, (char*) queue->data + head_index * queue->item_size, queue->item_size);

    } while (!atomic_compare_exchange_weak(&queue->head, &head, head+1));

    return true;
}