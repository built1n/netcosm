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

#pragma once

#include "hash.h"

/* a multimap implemented as a hash of linked lists */
/* O(1) insertion and lookup */
/* O(n) deletion by value, O(1) deletion by key */

/* there can be both duplicate keys AND values */

void *multimap_init(size_t tabsz,
                    unsigned (*hash_fn)(const void *key),
                    int (*compare_key)(const void *key_a, const void *key_b),
                    int (*compare_val)(const void *val_a, const void *val_b));

void multimap_free(void*);

struct multimap_list {
    const void *key;
    void *val;
    struct multimap_list *next;
};

/* returns a linked list of values with the same key with the length in n_pairs */
const struct multimap_list *multimap_lookup(void *map, const void *key, size_t *n_pairs);

/* returns true if there was no other pair with the same key */
bool multimap_insert(void *map, const void *key, const void *val);

/* delete the pair(s) with the given key and value, returns # deleted */
size_t multimap_delete(void *map, const void *key, const void *val);

/* returns # deleted */
size_t multimap_delete_all(void *map, const void *key);

/* returns a linked list, NOT individual items of a linked list */
/* set map to NULL after the initial call */
const struct multimap_list *multimap_iterate(void *map, void **save, size_t *n_pairs);

size_t multimap_size(void *map);

void multimap_setfreedata_cb(void *map, void (*)(void*));

void multimap_free(void *ptr);
void *multimap_dup(void *ptr);
void multimap_setdupdata_cb(void *ptr, void *(*cb)(void *ptr));
void *multimap_copy(void *ptr);
