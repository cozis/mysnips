#include <stdint.h>
#include <stdbool.h>
#include "../thread/sync.h"

enum {
    UNUSED,
    USED,
    DELETED,
};

typedef struct {
    int       state;
    uintptr_t value;
    uintptr_t key;
} item_t;

typedef struct {
    item_t *pool; // Pool of 2^n items
    int     size; // Capacity of the pool
    int     used; // Number of pool slots occupied
    os_mutex_t mutex;
} hashmap_t;

void      hashmap_create(hashmap_t *map, int init_size);
void      hashmap_delete(hashmap_t *map);
int       hashmap_count (hashmap_t *map);
void      hashmap_insert(hashmap_t *map, uintptr_t key, uintptr_t value);
bool      hashmap_remove(hashmap_t *map, uintptr_t key);
uintptr_t hashmap_select(hashmap_t *map, uintptr_t key);
bool      hashmap_exists(hashmap_t *map, uintptr_t key);
