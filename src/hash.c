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

#include "netcosm.h"

#include <stdlib.h>
#include <string.h>

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
    void (*free_key)(void *key);
    void (*free_data)(void *data);
    size_t n_entries;
};

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

/* wrapper to supress warnings */
int compare_strings(const void *a, const void *b)
{
    return strcmp(a,b);
}

void *hash_init(size_t sz, unsigned (*hash_fn)(const void*),
                           int (*compare_keys)(const void*, const void*))
{
    struct hash_map *ret = calloc(sizeof(struct hash_map), 1);
    ret->table = calloc(sizeof(struct hash_node), sz);
    ret->table_sz = sz;
    ret->hash = hash_fn;
    ret->compare = compare_keys;
    ret->n_entries = 0;

    return ret;
}

void hash_setfreekey_cb(void *ptr, void (*cb)(void *key))
{
    struct hash_map *map = ptr;
    map->free_key = cb;
}

void hash_setfreedata_cb(void *ptr, void (*cb)(void *data))
{
    struct hash_map *map = ptr;
    map->free_data = cb;
}

void hash_free(void *ptr)
{
    sig_debugf("freeing map\n");
    if(ptr)
    {
        struct hash_map *map = ptr;
        for(unsigned i = 0; i < map->table_sz; ++i)
        {
            struct hash_node *node = map->table[i];
            while(node)
            {
                struct hash_node *next = node->next;

                if(map->free_data)
                {
                    debugf("freeing data\n");
                    map->free_data((void*)node->data);
                }
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
    ++map->n_entries;

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

void hash_insert_pairs(void *ptr, const struct hash_pair *pairs,
                       size_t pairsize, size_t n)
{
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
    struct hash_map *map = ptr;
    unsigned hash = map->hash(key) % map->table_sz;

    struct hash_node *iter = map->table[hash], *last = NULL;

    while(iter)
    {
        if(map->compare(key, iter->key) == 0)
        {
            if(last)
                last->next = iter->next;
            else
                map->table[hash] = iter->next;
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

    return false;
}

void *hash_getkeyptr(void *ptr, const void *key)
{
    struct hash_map *map = ptr;
    unsigned hash = map->hash(key) % map->table_sz;

    struct hash_node *iter = map->table[hash];

    while(iter)
    {
        if(map->compare(key, iter->key) == 0)
            return (void*)(iter->key);
        iter = iter->next;
    }
    return NULL;
}

size_t hash_size(void *ptr)
{
    struct hash_map *map = ptr;
    return map->n_entries;
}
