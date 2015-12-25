#include "hash.h"
#include <string.h>
#include <stdio.h>

int main()
{
    void *map = hash_init(10000, hash_djb, compare_strings);
    hash_insert(map, "a",1);
    hash_insert(map, "b",2);
    void *ptr = map;
    void *data = NULL;
    void *save;
    do {
        char *key;
        data = hash_iterate(ptr, &save, &key);
        ptr = NULL;
        printf("%d %s\n", data, key);
    } while(data);
}
