#include "hash.h"

struct hash_node {
    const void *key;
    const void *data;
    struct hash_node *next;
};

struct hash_map {
    unsigned (*hash)(const void *data);
    int (*compare)(const void *a, const void *b);
    struct hash_node **table;
    size_t table_sz;
};

unsigned hash_djb(const char *str)
{
    unsigned hash = 5381;
    char c;
    while((c = *str++))
    {
        hash = ((hash << 5) + hash) ^ c;
    }

    return hash;
}

void *hash_init(size_t sz, unsigned (*hash_fn)(const void*),
                           int (*compare_keys)(const void*, const void*))
{
    struct hash_map *ret = calloc(sizeof(struct hash_map), 1);
    ret->table = calloc(sizeof(struct hash_node), sz);
    ret->table_sz = sz;
    ret->hash = hash_fn;
    ret->compare = compare_keys;

    return ret;
}

void hash_free(void *ptr)
{
    struct hash_map *map = ptr;
    for(unsigned i = 0; i < map->table_sz; ++i)
    {
        struct hash_node *node = map->table[i];
        while(node)
        {
            struct hash_node *next = node->next;
            free(node);
            node = next;
        }
    }
    free(map->table);
    free(map);
}

void *hash_insert(void *ptr, const void *key, const void *data)
{
    struct hash_map *map = ptr;
    unsigned hash = map->hash(key) % map->table_sz;

    struct hash_node *iter = map->table[hash];
    struct hash_node *last = NULL;

    while(iter)
    {
        if(map->compare(key, iter->key) == 0)
            return (void*)(iter->data);
        last = iter;
        iter = iter->next;
    }

    /* insert */
    struct hash_node *new = calloc(sizeof(struct hash_node), 1);
    new->key = key;
    new->data = data;
    new->next = NULL;
    if(!last)
        map->table[hash] = new;
    else
        last->next = new;

    return NULL;
}

void *hash_lookup(void *ptr, const void *key)
{
    struct hash_map *map = ptr;
    unsigned hash = map->hash(key) % map->table_sz;

    struct hash_node *iter = map->table[hash];

    while(iter)
    {
        if(map->compare(key, iter->key) == 0)
            return (void*)(iter->data);
        iter = iter->next;
    }
    return NULL;
}
