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
