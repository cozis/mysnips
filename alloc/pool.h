#include <stddef.h>

struct page_header;

struct pool_alloc {
    struct page_header *list_partially_full;
    struct page_header *list_full;
    size_t item_size;
    size_t item_align;
    size_t slots_per_page;
};

void  pool_alloc_create(struct pool_alloc *pool, size_t item_size, size_t item_align);
void  pool_alloc_delete(struct pool_alloc *alloc);
void *pool_alloc_get(struct pool_alloc *alloc);
void  pool_alloc_put(struct pool_alloc *alloc, void *ptr);