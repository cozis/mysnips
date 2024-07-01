#include <stddef.h>
#include "hash_map_ref.h"
#include "../alloc/os_alloc.h"

void hashmapref_create(hashmapref_t *ref)
{
    ref->entries = NULL;
    ref->count = 0;
    ref->capacity = 0;
}

void hashmapref_delete(hashmapref_t *ref)
{
    os_free(ref->entries, ref->capacity * sizeof(entry_t));
}

int hashmapref_count(hashmapref_t *ref)
{
    return ref->count;
}

void hashmapref_insert(hashmapref_t *ref, uintptr_t key, uintptr_t value)
{
    for (int i = 0; i < ref->count; i++) {
        if (ref->entries[i].key == key) {
            ref->entries[i].value = value;
            return;
        }
    }

    if (ref->entries == NULL) {
        int init_cap = 8;
        ref->entries = os_alloc(sizeof(entry_t) * init_cap);
        ref->count = 0;
        ref->capacity = init_cap;
    } else if (ref->count == ref->capacity) {
        int new_capacity = 2 * ref->capacity;
        entry_t *new_entries = os_alloc(sizeof(entry_t) * new_capacity);
        for (int i = 0; i < ref->count; i++)
            new_entries[i] = ref->entries[i];
        os_free(ref->entries, ref->capacity * sizeof(entry_t));
        ref->entries = new_entries;
        ref->capacity = new_capacity;
    }

    ref->entries[ref->count].key = key;
    ref->entries[ref->count].value = value;
    ref->count++;
}

bool hashmapref_remove(hashmapref_t *ref, uintptr_t key)
{
    int i = 0;
    while (i < ref->count && ref->entries[i].key != key)
        i++;
    if (i == ref->count)
        return false;

    ref->entries[i] = ref->entries[ref->count-1];
    ref->count--;
    return true;
}

uintptr_t hashmapref_select(hashmapref_t *ref, uintptr_t key)
{
    int i = 0;
    while (i < ref->count && ref->entries[i].key != key)
        i++;
    if (i == ref->count)
        return 0;
    return ref->entries[i].value;
}

bool hashmapref_exists(hashmapref_t *ref, uintptr_t key)
{
    return hashmapref_select(ref, key) != 0;
}
