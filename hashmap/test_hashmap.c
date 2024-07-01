#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "hash_map.h"
#include "hash_map_ref.h"

void check(bool exp, const char *expstr, const char *file, int line)
{
    if (!exp) {
        fprintf(stderr, "Assertion Failed [%s] at %s:%d\n", expstr, file, line);
        abort();
    }
}
#define CHECK(exp) check((exp), #exp, __FILE__, __LINE__);

void checkeqi(int left, int right, const char *file, int line)
{
    if (left != right) {
        fprintf(stderr, "Assertion Failed [%d == %d] at %s:%d\n", left, right, file, line);
        abort();
    }
}

void checkeqp(uintptr_t left, uintptr_t right, const char *file, int line)
{
    if (left != right) {
        fprintf(stderr, "Assertion Failed [%llx == %llx] at %s:%d\n", left, right, file, line);
        abort();
    }
}

#define CHECKEQI(lexp, rexp) checkeqi((lexp), (rexp), __FILE__, __LINE__);
#define CHECKEQP(lexp, rexp) checkeqp((lexp), (rexp), __FILE__, __LINE__);

int main(void)
{
    hashmap_t    map1;
    hashmapref_t map2;
    hashmap_create(&map1, 8);
    hashmapref_create(&map2);

    for (;;) {

        int op = rand() % 3;

        int max_key = 10;
        int min_key = 0;
        int key = min_key + rand() % (max_key - min_key + 1);
        uintptr_t value = (uintptr_t) rand();
        if (value == 0) value = 1;

        CHECKEQI(hashmap_exists(&map1, key), hashmapref_exists(&map2, key));

        switch (op) {

            case 0:
            {
                fprintf(stderr, "INSERT %d\n", key);
                hashmap_insert(&map1, key, value);
                hashmapref_insert(&map2, key, value);
            }
            break;

            case 1:
            {
                fprintf(stderr, "REMOVE %d\n", key);
                bool ok1 = hashmap_remove(&map1, key);
                bool ok2 = hashmapref_remove(&map2, key);
                CHECKEQI(ok1, ok2);
            }
            break;

            case 2:
            {
                fprintf(stderr, "SELECT %d\n", key);
                uintptr_t v1 = hashmap_select(&map1, key);
                uintptr_t v2 = hashmapref_select(&map2, key);
                CHECKEQP(v1, v2);
            }
            break;
        }
#if 0
        fprintf(stderr, "=== MAP 1 ===\n");
        for (int i = 0; i < map1.size; i++) {
            if (map1.pool[i].state == USED) {
                fprintf(stderr, "%lld - %lld\n",
                    map1.pool[i].key,
                    map1.pool[i].value);
            }
        }
        fprintf(stderr, "=============\n\n");

        fprintf(stderr, "=== MAP 2 ===\n");
        for (int i = 0; i < map2.count; i++) {
            fprintf(stderr, "%lld - %lld\n",
                map2.entries[i].key,
                map2.entries[i].value);
        }
        fprintf(stderr, "=============\n");
#endif
        for (int i = 0; i < map2.count; i++) {
            if (!hashmap_exists(&map1, map2.entries[i].key))
                fprintf(stderr, "Missing key %lld\n", map2.entries[i].key);
            CHECK(hashmap_exists(&map1, map2.entries[i].key));
        }

        CHECKEQI(hashmap_count(&map1), hashmapref_count(&map2));
    }

    hashmap_delete(&map1);
    hashmapref_delete(&map2);
    return 0;
}