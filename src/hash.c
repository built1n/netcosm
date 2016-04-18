/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2016 Franklin Wei
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hash.h"

#include "util.h" // for error()

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file
 * @brief A simple, chained hash table
 */

struct hash_node {
    const void *key;
    const void *data;
    struct hash_node *next;
};

struct hash_map {
    char sentinel; /* for avoiding hash/multihash confusion */
    unsigned (*hash)(const void *data);
    int (*compare)(const void *a, const void *b);
    struct hash_node **table;
    size_t table_sz;
    void (*free_key)(void *key);
    void (*free_data)(void *data);
    void* (*dup_data)(void *data);
    size_t n_entries, used_buckets;
};

#define CHECK_SENTINEL(map) do{if(map && ((struct hash_map*)map)->sentinel!=HASH_SENTINEL)error("hash/multimap mixing");}while(0);

unsigned hash_djb(const void *ptr)
{
    const char *str = ptr;
    unsigned hash = 5381;
    char c;
    while((c = *str++))
    {
        hash = ((hash << 5) + hash) ^ c;
    }

    return hash;
}

/* wrappers to suppress warnings with plain strcmp */
int compare_strings(const void *a, const void *b)
{
    return strcmp(a,b);
}

int compare_strings_nocase(const void *a, const void *b)
{
    return strcasecmp(a,b);
}

void *hash_init(size_t sz, unsigned (*hash_fn)(const void*),
                           int (*compare_keys)(const void*, const void*))
{
    struct hash_map *ret = calloc(1, sizeof(struct hash_map));
    if(!sz)
        sz = 1;
    ret->sentinel = HASH_SENTINEL;
    ret->table = calloc(sz, sizeof(struct hash_node*));
    ret->table_sz = sz;
    ret->hash = hash_fn;
    ret->compare = compare_keys;
    ret->n_entries = 0;
    ret->used_buckets = 0;

    return ret;
}

void hash_setfreekey_cb(void *ptr, void (*cb)(void *key))
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        map->free_key = cb;
    }
}

void hash_setfreedata_cb(void *ptr, void (*cb)(void *data))
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        map->free_data = cb;
    }
}

void hash_free(void *ptr)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        for(unsigned i = 0; i < map->table_sz; ++i)
        {
            struct hash_node *node = map->table[i];
            while(node)
            {
                struct hash_node *next = node->next;

                if(map->free_data)
                    map->free_data((void*)node->data);
                if(map->free_key)
                    map->free_key((void*)node->key);
                free(node);
                node = next;
            }
        }
        free(map->table);
        free(map);
    }
}

void *hash_iterate(void *map, void **saveptr, void **keyptr)
{
    struct iterstate_t {
        struct hash_map *map;
        struct hash_node *node;
        unsigned bucket;
    };

    struct iterstate_t *saved;

    if(map)
    {
        *saveptr = malloc(sizeof(struct iterstate_t));
        saved = *saveptr;

        saved->map = map;
        CHECK_SENTINEL(map);
        saved->bucket = 0;
        saved->node = NULL;
    }
    else
        saved = *saveptr;

    for(;saved->bucket < saved->map->table_sz; ++(saved->bucket))
    {
        do {
            if(!saved->node)
                saved->node = saved->map->table[saved->bucket];
            else
                saved->node = saved->node->next;
            if(saved->node)
            {
                if(keyptr)
                    *keyptr = (void*)saved->node->key;
                return (void*)saved->node->data;
            }
        } while(saved->node);
    }

    free(saved);

    return NULL;
}

/* 75% */
#define RESIZE_THRESHOLD ((1 << 15) + (1 << 14))

static void hash_internal_insert_new(const void *key, const void *data, struct hash_map *map,
                                     unsigned hash, struct hash_node *last)
{
    /* insert */
    struct hash_node *new = calloc(sizeof(struct hash_node), 1);
    new->key = key;
    new->data = data;
    new->next = NULL;
    ++map->n_entries;
    if(!last)
    {
        map->table[hash] = new;
        ++map->used_buckets;

        /* resize if load factor exceeds threshold */
        if((map->used_buckets << 16) / (map->table_sz) >= RESIZE_THRESHOLD)
        {
            hash_resize(map, map->table_sz * 2);
        }
    }
    else
        last->next = new;
}

void hash_overwrite(void *ptr, const void *key, const void *data)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        unsigned hash = map->hash(key) % map->table_sz;

        struct hash_node *iter = map->table[hash];
        struct hash_node *last = NULL;

        while(iter)
        {
            if(map->compare(key, iter->key) == 0)
            {
                if(map->free_data)
                    map->free_data((void*)iter->data);
                if(map->free_key)
                    map->free_key((void*)iter->key);

                iter->key = key;
                iter->data = data;

                return;
            }
            last = iter;
            iter = iter->next;
        }

        /* insert */
        hash_internal_insert_new(key, data, map, hash, last);
    }
}

void *hash_insert(void *ptr, const void *key, const void *data)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
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
        hash_internal_insert_new(key, data, map, hash, last);

        /* fall through */
    }
    return NULL;
}

void *hash_lookup(void *ptr, const void *key)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        unsigned hash = map->hash(key) % map->table_sz;

        struct hash_node *iter = map->table[hash];

        while(iter)
        {
            if(map->compare(key, iter->key) == 0)
                return (void*)(iter->data);
            iter = iter->next;
        }
        /* fall through */
    }

    return NULL;
}

void hash_insert_pairs(void *ptr, const struct hash_pair *pairs,
                       size_t pairsize, size_t n)
{
    CHECK_SENTINEL(ptr);
    const char *iter = (const char*)pairs;
    for(unsigned i = 0; i < n; ++i)
    {
        const struct hash_pair *pair = (const struct hash_pair*)iter;
        hash_insert(ptr, pair->key, pair);
        iter += pairsize;
    }
}

bool hash_remove(void *ptr, const void *key)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        unsigned hash = map->hash(key) % map->table_sz;

        struct hash_node *iter = map->table[hash], *last = NULL;

        while(iter)
        {
            if(map->compare(key, iter->key) == 0)
            {
                if(last)
                    last->next = iter->next;
                else
                {
                    map->table[hash] = iter->next;
                    --map->used_buckets;
                }

                if(map->free_key)
                    map->free_key((void*)iter->key);
                if(map->free_data)
                    map->free_data((void*)iter->data);

                --map->n_entries;

                free(iter);

                return true;
            }
            last = iter;
            iter = iter->next;
        }
        /* fall through */
    }
    return false;
}

/* return an opaque pointer to a particular key/value pair */
struct hash_export_node hash_get_internal_node(void *ptr, const void *key)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        unsigned hash = map->hash(key) % map->table_sz;

        struct hash_node *iter = map->table[hash], *last = NULL;;

        while(iter)
        {
            if(map->compare(key, iter->key) == 0)
            {
                struct hash_export_node ret;
                ret.hash = hash;
                ret.last = last;
                ret.node = iter;
                ret.next = iter->next;
                return ret;
            }
            last = iter;
            iter = iter->next;
        }
        /* fall through */
    }

    struct hash_export_node ret;
    memset(&ret, 0, sizeof(ret));
    ret.node = NULL;
    return ret;
}

void hash_del_internal_node(void *ptr, const struct hash_export_node *node)
{
    if(ptr)
    {
        if(node->node)
        {
            struct hash_map *map = ptr;
            CHECK_SENTINEL(map);

            struct hash_node *node_val = node->node;

            if(map->free_data)
                map->free_data((void*)node_val->data);
            if(map->free_key)
                map->free_key((void*)node_val->key);
            free(node_val);

            if(node->last)
                ((struct hash_node*)node->last)->next = node->next;
            else
            {
                --map->used_buckets;
                map->table[node->hash] = node->next;
            }
        }
    }
}

void *hash_getkeyptr(void *ptr, const void *key)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        unsigned hash = map->hash(key) % map->table_sz;

        struct hash_node *iter = map->table[hash];

        while(iter)
        {
            if(map->compare(key, iter->key) == 0)
            return (void*)(iter->key);
            iter = iter->next;
        }
        /* fall through */
    }
    return NULL;
}

size_t hash_size(void *ptr)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        return map->n_entries;
    }
    else
        return 0;
}

void *hash_dup(void *ptr)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);

        struct hash_map *ret = calloc(1, sizeof(struct hash_map));
        memcpy(ret, map, sizeof(*ret));

        ret->table = calloc(ret->table_sz, sizeof(struct hash_node*));
        ret->used_buckets = 0;
        ret->n_entries = 0;

        void *save = NULL;
        while(1)
        {
            void *key;
            void *data = hash_iterate(ptr, &save, &key);
            if(!data)
                break;
            ptr = NULL;
            if(map->dup_data)
                data = map->dup_data(data);
            hash_insert(ret, key, data);
        }
        return ret;
    }
    else
        return NULL;
}

void hash_setdupdata_cb(void *ptr, void *(*cb)(void*))
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        map->dup_data = cb;
    }
}

/* naive copy */
bool hash_resize(void *ptr, size_t new_sz)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);

        if(new_sz == map->table_sz)
            return false;

        struct hash_node **old = map->table;

        size_t old_entries = map->n_entries, old_sz = map->table_sz;

        map->table = calloc(new_sz, sizeof(struct hash_node*));
        map->table_sz = new_sz;
        map->n_entries = 0;
        map->used_buckets = 0;

        for(unsigned i = 0; i < old_sz; ++i)
        {
            struct hash_node *iter = old[i], *next;
            while(iter)
            {
                next = iter->next;

                /* bare-bones insert */
                unsigned hash = map->hash(iter->key) % map->table_sz;
                iter->next = map->table[hash];
                map->table[hash] = iter;
                if(!iter->next)
                    ++map->used_buckets;
                ++map->n_entries;

                iter = next;
            }
        }

        free(old);

        assert(old_entries == map->n_entries);
        return true;
    }
    else
        return false;
}

/* debug dump */

#if 1

void hash_dump(void *ptr)
{
    if(ptr)
    {
        struct hash_map *map = ptr;
        CHECK_SENTINEL(map);
        size_t used_buckets = map->table_sz, n_entries = 0;
        for(unsigned i = 0; i < map->table_sz; ++i)
        {
            printf("%d:", i);
            struct hash_node *iter = map->table[i];
            if(!iter)
                --used_buckets;
            while(iter)
            {
                printf(" --> `%s'", iter->key);
                ++n_entries;
                iter = iter->next;
            }
            printf("\n");
        }
        printf("Load factor: %f\n", (double)used_buckets / map->table_sz);
        if(n_entries == map->n_entries && used_buckets == map->used_buckets)
            printf("Map is sane.\n");
        else
            printf("Map is NOT SANE!!!\n");
    }
}

#endif
