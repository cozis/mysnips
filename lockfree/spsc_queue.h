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
#include <stdatomic.h>

typedef struct {
    void *items;
    uint32_t item_size;
    uint32_t cap_log2;
    _Atomic uint64_t head;
    _Atomic uint64_t tail;
} spsc_queue_t;

void spsc_queue_init(spsc_queue_t *queue, void *items, uint32_t num_items_log2, uint32_t item_size);
bool spsc_queue_push(spsc_queue_t *queue, void *src);
bool spsc_queue_pop (spsc_queue_t *queue, void *dst);
bool spsc_queue_multi_pop (spsc_queue_t *queue, void *arr, int num);
bool spsc_queue_multi_push(spsc_queue_t *queue, void *arr, int num);
int  spsc_queue_size(spsc_queue_t *queue);
int  spsc_queue_capacity(spsc_queue_t *queue);
void spsc_queue_peek_0(spsc_queue_t *queue, void **src, int *num);
void spsc_queue_peek_1(spsc_queue_t *queue, void **src, int *num);
