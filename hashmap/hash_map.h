#include <stdbool.h>

typedef struct {
    void *value;
    int   state;
    int   key;
} item_t;

typedef struct {
    item_t *pool; // Pool of 2^n items
    int     size; // Capacity of the pool
    int     used; // Number of pool slots occupied
} hashmap_t;

void  hashmap_create(hashmap_t *map, int init_size);
void  hashmap_delete(hashmap_t *map);
int   hashmap_count(hashmap_t *map);
void  hashmap_insert(hashmap_t *map, int key, void *value);
bool  hashmap_remove(hashmap_t *map, int key);
void *hashmap_select(hashmap_t *map, int key);
bool  hashmap_exists(hashmap_t *map, int key);
