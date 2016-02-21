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

#include "globals.h"

#include "multimap.h"

#define CHECK_SENTINEL(map) do{if(map && ((struct multimap_t*)map)->sentinel!=MULTIMAP_SENTINEL)error("hash/multimap mixing");}while(0);

/* contains all the pairs using a key */
struct multimap_node {
    struct multimap_list *list;
    size_t n_pairs;
    struct multimap_t *map;
};

struct multimap_t {
    char sentinel;
    size_t total_pairs;
    unsigned refcount;

    /* map of key->multimap nodes */
    void *hash_tab;
    int (*compare_val)(const void *val_a, const void *val_b);

    void (*free_data)(void*);
    void (*free_key)(void*);
    void *(*dup_data)(void*);
};

static void free_node(void *ptr)
{
    struct multimap_node *node = ptr;
    /* free the list */
    struct multimap_list *iter = node->list, *next;
    while(iter)
    {
        next = iter->next;
        if(node->map->free_data)
            node->map->free_data(iter->val);
        if(node->map->free_key)
            node->map->free_key((void*)iter->key);
        free(iter);
        iter = next;
    }
    free(node);
}

static void *dup_node(void *ptr)
{
    struct multimap_node *node = ptr;
    struct multimap_node *ret  = calloc(1, sizeof(*ret));
    memcpy(ret, node, sizeof(*ret));

    struct multimap_list *iter = node->list, *last = NULL;
    while(iter)
    {
        struct multimap_list *newpair = calloc(1, sizeof(*newpair));
        memcpy(newpair, iter, sizeof(*newpair));
        if(!last)
            ret->list = iter;
        else
            last->next = iter;
        last = newpair;

        iter = iter->next;
    }

    return ret;
}

void *multimap_init(size_t tabsz,
                    unsigned (*hash_fn)(const void *key),
                    int (*compare_key)(const void *key_a, const void *key_b),
                    int (*compare_val)(const void *val_a, const void *val_b))
{
    struct multimap_t *ret = calloc(1, sizeof(struct multimap_t));
    ret->hash_tab = hash_init(tabsz, hash_fn, compare_key);
    hash_setfreedata_cb(ret->hash_tab, free_node);
    hash_setdupdata_cb(ret->hash_tab, dup_node);
    ret->compare_val = compare_val;
    ret->sentinel = MULTIMAP_SENTINEL;
    ret->refcount = 1;
    return ret;
}

void multimap_free(void *ptr)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);
        if(!(--map->refcount))
        {
            hash_free(map->hash_tab);
            free(map);
        }
    }
}

const struct multimap_list *multimap_lookup(void *ptr, const void *key, size_t *n_pairs)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);
        struct multimap_node *node = hash_lookup(map->hash_tab, key);

        if(!node)
        {
            if(n_pairs)
                *n_pairs = 0;
            return NULL;
        }

        if(n_pairs)
            *n_pairs = node->n_pairs;

        return node->list;
    }
    else
        return NULL;
}

bool multimap_insert(void *ptr, const void *key, const void *val)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;

        CHECK_SENTINEL(map);

        struct multimap_node *node = hash_lookup(map->hash_tab, key);
        if(!node)
        {
            node = calloc(1, sizeof(struct multimap_node));

            node->map = map;

            node->list = calloc(1, sizeof(struct multimap_list));
            node->list->key = key;
            node->list->val = (void*)val;
            node->list->next = NULL;

            node->n_pairs = 1;
            ++map->total_pairs;

            hash_insert(map->hash_tab, key, node);

            return true;
        }
        else
        {
            struct multimap_list *new = calloc(1, sizeof(struct multimap_list));
            new->key = key;
            new->val = (void*)val;
            new->next = node->list;

            ++node->n_pairs;
            ++map->total_pairs;

            node->list = new;

            return false;
        }
    }
    else
        return false;
}

size_t multimap_delete(void *ptr, const void *key, const void *val)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);

        struct multimap_node *node = hash_lookup(map->hash_tab, key);

        if(!node)
            return false;
        /* iterate over the node's pairs and delete */
        size_t deleted = 0;

        struct multimap_list *last = NULL, *iter = node->list, *next;;
        while(iter)
        {
            next = iter->next;
            if(!map->compare_val(val, iter->val))
            {
                if(map->free_data)
                    map->free_data(iter->val);
                if(map->free_key)
                    map->free_key((void*)iter->key);

                if(last)
                    last->next = iter->next;
                else
                    node->list = iter->next;
                free(iter);

                ++deleted;
                --node->n_pairs;
                --map->total_pairs;
            }
            else
                last = iter;
            iter = next;
        }

        if(!node->n_pairs)
        {
            hash_remove(map->hash_tab, key);
        }

        return deleted;
    }
    else
        return 0;
}

size_t multimap_delete_all(void *ptr, const void *key)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);

        struct multimap_node *node = hash_lookup(map->hash_tab, key);
        if(node)
        {
            size_t ret = node->n_pairs;
            map->total_pairs -= ret;

            hash_remove(map->hash_tab, key);

            return ret;
        }
        /* fall through */
    }
    return 0;
}

const struct multimap_list *multimap_iterate(const void *ptr, void **save, size_t *n_pairs)
{
    const struct multimap_t *map = ptr;
    CHECK_SENTINEL(map);

    struct multimap_node *node;
    if(map)
        node = hash_iterate(map->hash_tab, save, NULL);
    else
        node = hash_iterate(NULL, save, NULL);

    if(node)
    {
        if(n_pairs)
            *n_pairs = node->n_pairs;

        return node->list;
    }
    else
        return NULL;
}

size_t multimap_size(void *ptr)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);
        return map->total_pairs;
    }
    else
        return 0;
}

void multimap_setfreedata_cb(void *ptr, void (*cb)(void *ptr))
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);

        map->free_data = cb;
    }
}

void multimap_setdupdata_cb(void *ptr, void *(*cb)(void *ptr))
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);

        map->dup_data = cb;
    }
}

void *multimap_dup(void *ptr)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);
        ++map->refcount;

        return map;
    }
    else
        return NULL;
}

void *multimap_copy(void *ptr)
{
    if(ptr)
    {
        struct multimap_t *map = ptr;
        CHECK_SENTINEL(map);

        struct multimap_t *ret = calloc(1, sizeof(struct multimap_t));
        memcpy(ret, map, sizeof(*ret));

        ret->hash_tab = hash_dup(map->hash_tab);
        ret->refcount = 1;

        /* iterate and replace each node's *map pointer */
        void *map_ptr = ret->hash_tab, *save;
        while(1)
        {
            struct multimap_node *node = hash_iterate(map_ptr, &save, NULL);
            if(!node)
                break;
            map_ptr = NULL;
            node->map = ret;
        }

        return ret;
    }
    else
        return NULL;
}
