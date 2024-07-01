#include <stdbool.h>

typedef struct {
    uintptr_t key;
    uintptr_t value;
} entry_t;

typedef struct {
    entry_t *entries;
    int count;
    int capacity;
} hashmapref_t;

void      hashmapref_create(hashmapref_t *map);
void      hashmapref_delete(hashmapref_t *map);
int       hashmapref_count (hashmapref_t *map);
void      hashmapref_insert(hashmapref_t *map, uintptr_t key, uintptr_t value);
bool      hashmapref_remove(hashmapref_t *map, uintptr_t key);
uintptr_t hashmapref_select(hashmapref_t *map, uintptr_t key);
bool      hashmapref_exists(hashmapref_t *map, uintptr_t key);
