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

#include <string.h>
#include <assert.h>
#include <stdatomic.h>
#include "mpmc_queue.h"

void
mpmc_queue_init(struct mpmc_queue *q, 
                void *arrptr, int arrlen,
                int cell)
{
    assert(arrlen % cell == 0);

    q->head = 0;
    q->tail = 0;
    q->temp_head = 0;
    q->temp_tail = 0;
    q->size = arrlen / cell;
    q->base = arrptr;
    q->cell = cell;
}

static void*
item_addr(struct mpmc_queue *q, int index)
{
    return q->base + q->cell * (index % q->size);
}

static uint64_t
acquire_push_location(struct mpmc_queue *q)
{
    uint64_t old_temp_tail;
    uint64_t new_temp_tail;

    do {

        uint64_t old_temp_head;

        old_temp_tail = q->temp_tail;
        old_temp_head = q->temp_head;

        if (old_temp_head + q->size == old_temp_tail)
            return UINT64_MAX; // Queue is full

        assert(old_temp_head + q->size > old_temp_tail);

        new_temp_tail = old_temp_tail + 1;
    } while (!atomic_compare_exchange_weak(&q->temp_tail, &old_temp_tail, new_temp_tail));

    return old_temp_tail;
}

static void
release_push_location(struct mpmc_queue *q, uint64_t index)
{
    // The current thread already inserted an
    // item at position "index", which comes
    // after the queue's tail.
    //
    // It may be possible that other threads
    // inserted items before this one, but still
    // after the tail
    //
    // Before being able to move the tail over
    // this element, we need to wait for other
    // threads to do it.

    while (q->tail != index);

    q->tail++;
}

bool
mpmc_queue_try_push(struct mpmc_queue *q, void *src)
{
    uint64_t index = acquire_push_location(q);
    if (index == UINT64_MAX) return false;

    void *dst = item_addr(q, index);
    memcpy(dst, src, q->cell);

    release_push_location(q, index);
    return true;
}

void
mpmc_queue_push(struct mpmc_queue *q, void *src)
{
    while (!mpmc_queue_try_push(q, src));
}

static uint64_t
acquire_pop_location(struct mpmc_queue *q)
{
    uint64_t old_head;
    uint64_t new_head;
    do {
        uint64_t old_tail;

        // It's important to get head before tail
        // or head may be incremented over tail before
        // querying it.
        old_head = q->head;
        old_tail = q->tail;

        if (old_head == old_tail)
            return UINT64_MAX;

        assert(old_tail > old_head);

        new_head = old_head + 1;
    } while (!atomic_compare_exchange_weak(&q->head, &old_head, new_head));
    return old_head;
}

static void
release_pop_location(struct mpmc_queue *q, uint64_t index)
{
    while (q->temp_head != index);

    q->temp_head++;
}

bool
mpmc_queue_try_pop(struct mpmc_queue *q, void *dst)
{
    uint64_t index = acquire_pop_location(q);
    if (index == UINT64_MAX)
        return false;

    void *src = item_addr(q, index);
    memcpy(dst, src, q->cell);

    release_pop_location(q, index);
    return true;
}

void
mpmc_queue_pop(struct mpmc_queue *q, void *dst)
{
    while (!mpmc_queue_try_pop(q, dst));
}
