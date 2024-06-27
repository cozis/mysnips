#include <stdbool.h>
#include "pool.h"
#include "os_alloc.h"

struct page_slot {
    struct page_slot *next;
};

struct page_header {
    struct page_header **prev;
    struct page_header  *next;
    struct page_slot   *slots;
    int allocated_count;
};

static int page_size___ = -1;

void pool_alloc_create(struct pool_alloc *pool, size_t item_size, size_t item_align)
{
    if (page_size___ < 0)
        page_size___ = ospagesize();

    assert(item_size >= sizeof(void*));
    assert(item_size <= page_size___ - sizeof(struct page_header));

    pool->list_partially_full = NULL;
    pool->list_full = NULL;
    pool->item_size = item_size;
    pool->item_align = item_align;
    pool->slots_per_page = 0;

    size_t mask = item_align-1;
    size_t cur = sizeof(struct page_header);
    for (;;) {
        size_t pad = -cur & mask;
        cur += pad;
        if (cur >= page_size___)
            break;
        cur += item_size;
        if (cur >= page_size___)
            break;
        pool->slots_per_page++;
    }

    assert(pool->slots_per_page > 0);
}

void pool_alloc_delete(struct pool_alloc *alloc)
{
    struct page_header *cursor;
    
    cursor = alloc->list_partially_full;
    alloc->list_partially_full = NULL;
    while (cursor) {
        struct page_header *next;
        next = cursor->next;
        osfree(cursor, page_size___);
        cursor = next;
    }

    cursor = alloc->list_full;
    alloc->list_full = NULL;
    while (cursor) {
        struct page_header *next;
        next = cursor->next;
        osfree(cursor, page_size___);
        cursor = next;
    }
}

static bool
ensure_partially_full_page_available(struct pool_alloc *alloc)
{
    if (alloc->list_partially_full == NULL) {

        struct page_header *page;

        page = (typeof(page)) osalloc(page_size___);
        
        int slot_count = (page_size___ - sizeof(struct page_header)) / alloc->item_size;
        struct page_slot *slots = NULL;
        struct page_slot **tail = &slots;

        char *cursor = (char*) (page + 1);
        char *end = (char*) page + page_size___;
        for (int i = 0; i < alloc->slots_per_page; i++) {
            
            uintptr_t mask = alloc->item_align-1;
            cursor += -(uintptr_t) cursor & mask;
            if (cursor >= end) break;
            
            struct page_slot *slot;
            slot = (struct page_slot*) cursor;
            *tail = slot;
            tail = &slot->next;
        }
        *tail = NULL;

        page->allocated_count = 0;
        page->slots = slots;

        page->prev = &alloc->list_partially_full;
        page->next = alloc->list_partially_full;
        alloc->list_partially_full = page;
    }
}

void *pool_alloc_get(struct pool_alloc *alloc)
{
    ensure_partially_full_page_available(alloc);

    struct page_header *page;
    page = alloc->list_partially_full;
    
    struct page_slot *slot;
    slot = page->slots;
    page->slots = slot->next;

    page->allocated_count++;

    // Move the list from the partially full to the full list
    // if this was the last free slot
    if (page->allocated_count == alloc->slots_per_page) {

        // Unlink from the partially full list
        *page->prev = page->next;

        // Link to the full list
        page->prev = &alloc->list_full;
        page->next = alloc->list_full;
        alloc->list_full = page;
    }

    return slot;
}

void pool_alloc_put(struct pool_alloc *alloc, void *ptr)
{
    struct page_header *page;
    page = (struct page_header*) ((uintptr_t) ptr & ~(page_size___-1));

    struct page_slot *slot;
    slot = ptr;

    // Add back to the page's free list
    slot->next = page->slots;
    page->slots = slot;

    assert(page->allocated_count > 0);
    page->allocated_count--;

    if (page->allocated_count == 0) {

        // If the page just became unused, deallocate it
        *page->prev = page->next;
        osfree(page, page_size___);

    } else if (page->allocated_count == alloc->slots_per_page-1) {

        // If the page was full before the deallocation,
        // move it from the full list to the partially
        // full list

        // Remove it from the full list
        *page->prev = page->next;

        // Insert it into the partially full list
        page->prev = &alloc->list_partially_full;
        page->next = alloc->list_partially_full;
        alloc->list_partially_full = page;
    }
}
