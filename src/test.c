#include "hash.h"
#include <string.h>
#include <stdio.h>

int main()
{
    void *map = hash_init(10000, hash_djb, compare_strings);
    hash_insert(map, "a",1);
    hash_insert(map, "b",2);
    hash_resize(map, 2);
    void *ptr = map, *save, *key;
    while(1)
    {
        void *data = hash_iterate(ptr, &save, &key);
        ptr = NULL;
        if(data)
            printf("%s %d\n", key, data);
        else
            break;
    }
}
