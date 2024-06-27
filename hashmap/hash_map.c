#include <stdint.h>
#include <assert.h>
#include "hash_map.h"
#include "../alloc/os_alloc.h"

// TEMP
#include <stdio.h>

enum {
    UNUSED,
    USED,
    DELETED,
};

static int next_pow2(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static bool is_pow2(int val)
{
    return (val & (val - 1)) == 0;
}

void hashmap_create(hashmap_t *map, int init_size)
{
    if (init_size < 8)
        init_size = 8;

    init_size = next_pow2(init_size);

    map->pool = os_alloc(sizeof(item_t) * init_size);

    for (int i = 0; i < init_size; i++)
        map->pool[i].state = UNUSED;

    map->size = init_size;
    map->used = 0;
}

void hashmap_delete(hashmap_t *map)
{
    os_free(map->pool, sizeof(item_t) * map->size);
}

int hashmap_count(hashmap_t *map)
{
    return map->used;
}

static bool should_resize(hashmap_t *map)
{
    // Resize if the pool is about 70% full.
    return map->used * 10 >= map->size * 7;
}

static void resize(hashmap_t *map, int new_size)
{
    // If the target size is smaller than the 
    // item count, change it.
    if (new_size < map->used)
        new_size = map->used;

    if (new_size < 8)
        new_size = 8;

    // Move the target size to the next power
    // of 2.
    new_size = next_pow2(new_size);

    item_t *new_pool = os_alloc(sizeof(item_t) * new_size);
    assert(new_pool);

    for (int i = 0; i < new_size; i++)
        new_pool[i].state = UNUSED;

    // Get the current pool and save it in a
    // temporary variable, then put the new one
    // in its place and call "Insert" again for
    // each item.
    item_t *old_pool = map->pool;
    int     old_used = map->used;
    int     old_size = map->size;

    map->pool = new_pool;
    map->size = new_size;
    map->used = 0;

    for (int i = 0; i < old_size; i++)
        if (old_pool[i].state == USED)
            hashmap_insert(map, old_pool[i].key, old_pool[i].value);

    // Resize was succesful
    os_free(old_pool, sizeof(item_t) * old_size);
}

static int hashfunc(int key)
{
    return key;
}

void hashmap_insert(hashmap_t *map, int key, void *value)
{
    assert(value);

    if (should_resize(map))
        resize(map, 2 * map->size);

    assert(map->used < map->size);
    assert(is_pow2(map->size));

    int hash = hashfunc(key);
    int pert = hash;
    int mask = map->size - 1;

    int i = hash & mask;
    for (;;) {
        
        /*
         * Look for a slot that's either marked as 
         * UNUSED or DELETED.
         * 
         * If the current slot is USED, then it may
         * be a value previously associated to this
         * key. If it is, don't create a new entry
         * but update the current one. If the USED
         * slot has a different key, consider a new
         * slot.
         */

        item_t *item = &map->pool[i];
        if (item->state == UNUSED || item->state == DELETED) {
            
            /*
             * Insert the item
             */

            item->value = value;
            item->state = USED;
            item->key = key;
            map->used++;
            break;
        }

        assert(item->state == USED);

        if (item->key == key) {
            item->value = value;
            break;
        }

        /*
         * Advance to the next slot.
         */
        pert >>= 5;
        i = (i*5 + pert + 1) & mask;
    }
}

bool hashmap_remove(hashmap_t *map, int key)
{
    if (map->size == 0)
        return false;

    assert(is_pow2(map->size));

    uint32_t hash = hashfunc(key);
    uint32_t pert = hash;
    uint32_t mask = map->size - 1;

    uint32_t i = hash & mask;
    for (;;) {

        /*
         * We're looking for a USED slot with the
         * expected key. If an UNUSED slot is found
         * while searching, it means the key was
         * never inserted. If a slot is DELETED, then
         * just ignore it.
         */

        item_t *item = &map->pool[i];

        if (item->state == UNUSED)
            return false;

        if (item->state == USED && item->key == key) {
            
            /*
             * This slot matches the key. Erase the key and value
             * objects and change its state to DELETED.
             */
            
            item->state = DELETED;
            item->value = NULL;
            item->key = -1;

            map->used--;
            break;
        }

        assert(item->state == DELETED || item->state == USED);

        /*
         * Advance to the next slot.
         */
        pert >>= 5;
        i = (i*5 + pert + 1) & mask;
    }
    return true;
}

void *hashmap_select(hashmap_t *map, int key)
{
    if (map->size == 0)
        return NULL;

    assert(is_pow2(map->size));

    int hash = hashfunc(key);
    int pert = hash;
    int mask = map->size - 1;

    int start_i =  hash & mask;
    int i = start_i;
    do {

        /*
         * This loop is pretty much the same as Delete,
         * except we return the value instead of deleting
         * the item.
         */

        item_t *item = &map->pool[i];

        if (item->state == UNUSED) break;

        if (item->state == USED && item->key == key)
            return item->value;

        assert(item->state == DELETED || item->state == USED);

        /*
         * Advance to the next slot.
         */
        pert >>= 5;
        i = (i*5 + pert + 1) & mask;

    } while (i != start_i);

    return NULL;
}

bool hashmap_exists(hashmap_t *map, int key)
{
    return hashmap_select(map, key) != NULL;
}
