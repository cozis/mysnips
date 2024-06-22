/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */

#include "spsc_queue.h"

void spsc_queue_init(spsc_queue_t *queue, void *items,
                     uint32_t num_items_log2, uint32_t item_size)
{
    queue->items = items;
    queue->item_size = item_size;
    queue->cap_log2 = num_items_log2;
    queue->head = 0;
    queue->tail = 0;
}

int spsc_queue_capacity(spsc_queue_t *queue)
{
    return 1U << queue->cap_log2;
}

int spsc_queue_size(spsc_queue_t *queue)
{
    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);
    return tail - head;
}

static void *item_addr(spsc_queue_t *queue, int index)
{
    return (char*) queue->items + index * queue->item_size;
}

static void store_item(spsc_queue_t *queue, int index, void *src)
{
    assert(index >= 0 && index < queue->cap_log2);
    void *dst = item_addr(queue, index);
    memcpy(dst, src, queue->item_size);
}

static void read_item(spsc_queue_t *queue, int index, void *dst)
{
    assert(index >= 0 && index < queue->cap_log2);
    void *src = item_addr(queue, index);
    memcpy(dst, src, queue->item_size);
}

bool spsc_queue_push(spsc_queue_t *queue, void *src)
{
    uint32_t cap = 1U << queue->cap_log2;
    uint64_t mask = cap - 1;

    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);
    uint32_t used = tail - head;
    if (used == cap) return false;
    
    if (src) store_item(queue, tail & mask, src);

    // We assume u64s don't overflow
    assert(queue->tail < UINT64_MAX);

    atomic_store(&queue->tail, queue->tail + 1);
    return true;
}

bool spsc_queue_multi_push(spsc_queue_t *queue, void *arr, int num)
{
    assert(num > 0);

    uint32_t cap = 1U << queue->cap_log2;
    uint64_t mask = cap - 1;

    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);
    uint32_t used = tail - head;
    if (used + (uint32_t) num >= cap) return false;

    // We assume u64s don't overflow
    assert(tail < UINT64_MAX - num);

    for (int i = 0; i < num; i++) {
        void *src = (char*) arr + i * queue->item_size;
        store_item(queue, (tail + i) & mask, src);
    }

    atomic_store(&queue->tail, tail + (uint64_t) num);
    return true;
}

bool spsc_queue_pop(spsc_queue_t *queue, void *dst)
{
    uint32_t cap = 1U << queue->cap_log2;
    uint64_t mask = cap - 1;

    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);
    uint32_t used = tail - head;
    if (used == 0) return false;
    
    if (dst) read_item(queue, head & mask, dst);

    // We assume u64s don't overflow
    assert(head < UINT64_MAX);
    atomic_store(&queue->head, head + 1);
    return true;
}

bool spsc_queue_multi_pop(spsc_queue_t *queue, void *arr, int num)
{
    assert(num > 0);

    uint32_t cap = 1U << queue->cap_log2;
    uint64_t mask = cap - 1;

    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);
    uint32_t used = tail - head;
    if (used < num) return false;

    // We assume u64s don't overflow
    assert(head <= UINT64_MAX - num);

    for (int i = 0; i < num; i++) {
        void *dst = (char*) arr + i * queue->item_size;
        read_item(queue, (head + i) & mask, dst);
    }

    atomic_store(&queue->head, head + (uint64_t) num);
    return true;
}

void spsc_queue_peek_0(spsc_queue_t *queue, void **src, int *num)
{
    assert(src && num);

    uint32_t cap  = 1U << queue->cap_log2;
    uint64_t mask = cap - 1;
    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);

    if (head == tail) {
        *src = NULL;
        *num = 0;
    } else {

        uint32_t masked_head = head & mask;
        uint32_t masked_tail = tail & mask;
        
        if (masked_head < masked_tail) {
            *src = item_addr(queue, masked_head);
            *num = masked_tail - masked_head;
        } else {
            *src = item_addr(queue, masked_head);
            *num = cap - masked_head;
        }
    }
}

void spsc_queue_peek_1(spsc_queue_t *queue, void **src, int *num)
{
        uint32_t cap  = 1U << queue->cap_log2;
    uint64_t mask = cap - 1;
    uint64_t head = atomic_load(&queue->head);
    uint64_t tail = atomic_load(&queue->tail);

    if (head == tail) {
        *src = NULL;
        *num = 0;
    } else {

        uint32_t masked_head = head & mask;
        uint32_t masked_tail = tail & mask;
        
        if (masked_head < masked_tail) {
            *src = NULL;
            *num = 0;
        } else {
            *src = item_addr(queue, 0);
            *num = masked_tail;
        }
    }
}