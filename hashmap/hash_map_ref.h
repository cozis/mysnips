#include <stdbool.h>

typedef struct {
    int   key;
    void *value;
} entry_t;

typedef struct {
    entry_t *entries;
    int count;
    int capacity;
} hashmapref_t;

void  hashmapref_create(hashmapref_t *map);
void  hashmapref_delete(hashmapref_t *map);
int   hashmapref_count(hashmapref_t *map);
void  hashmapref_insert(hashmapref_t *map, int key, void *value);
bool  hashmapref_remove(hashmapref_t *map, int key);
void *hashmapref_select(hashmapref_t *map, int key);
bool  hashmapref_exists(hashmapref_t *map, int key);
