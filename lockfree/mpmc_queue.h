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

#include <stdint.h>
#include <stdbool.h>

/*
 * +--------------+-----------------+----------------+-------------------+-------------+
 * | unused       | Pop in progress |     Items      | Push in progress  | Unused      |
 * +--------------+-----------------+----------------+-------------------+-------------+
 *                ^                 ^                ^                   ^
 *                temp_head         head             tail                temp_tail
 */

struct mpmc_queue {

    _Atomic uint64_t head;
    _Atomic uint64_t tail;
    _Atomic uint64_t temp_head;
    _Atomic uint64_t temp_tail;

    // The following aren't written to by
    // push and pop operations, therefore
    // don't need to be atomic.
    uint64_t size; // Capacity of the queue (item count)
    uint64_t cell; // Size of a single item (in bytes)
    char *base; // Pointer to the first item
};

void mpmc_queue_init(struct mpmc_queue *q, void *arrptr, int arrlen, int item_size);
void mpmc_queue_push(struct mpmc_queue *q, void *src);
void mpmc_queue_pop(struct mpmc_queue *q, void *dst);
bool mpmc_queue_try_push(struct mpmc_queue *q, void *src);
bool mpmc_queue_try_pop(struct mpmc_queue *q, void *dst);

#define mpmc_queue_INIT(q, arr) mpmc_queue_init(q, arr, sizeof(arr), sizeof((arr)[0]))