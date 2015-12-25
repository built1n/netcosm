#include "hash.h"
#include <string.h>

int main()
{
    struct hash_map *map = hash_init(10, hash_djb, strcmp);
    hash_insert(map, "a", 42);
    hash_insert(map, "b", 32);
    printf("weird. %d\n", hash_lookup(map, "a"));
}
