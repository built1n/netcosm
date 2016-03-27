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

#include <stdbool.h>
#include <stddef.h>

/* simple, generic chained hash map implementation */
/* no duplicate keys are allowed */
/* O(n) insertion */

/* for telling containers apart */
#define HASH_SENTINEL 0x10
#define MULTIMAP_SENTINEL 0x11

unsigned hash_djb(const void*);
int compare_strings(const void*, const void*);
int compare_strings_nocase(const void*, const void*);

void *hash_init(size_t tabsz, unsigned (*hash_fn)(const void *key),
                int (*compare_key)(const void*, const void*));

void hash_setfreedata_cb(void*, void (*cb)(void *data));
void hash_setfreekey_cb(void*,  void (*cb)(void *key));

/*
 * free all data associated with a map handle
 *
 * if callbacks for free'ing keys or data are installed, they will be
 * called.
 */
void hash_free(void*);

/*
 * insert a pair, returns NULL if NOT already found (a.k.a. success),
 * otherwise returns the existing data pointer without inserting the
 * new pair (a.k.a. failure)
 */
void *hash_insert(void*, const void *key, const void *data);

/* overwrite an existing key/value pair with a new one */
void hash_overwrite(void*, const void *key, const void *data);

/* returns NULL if not found */
void *hash_lookup(void*, const void *key);

bool hash_remove(void *ptr, const void *key);

/*
 * use like you would strtok_r
 *
 * allocates a buffer that's freed once all elements are processed if
 * you must stop iteration without processing every element,
 * free(*saved)
 *
 * if keyptr!=NULL, the key pointer will be saved to *keyptr
 */
void *hash_iterate(void *map, void **saved, void **keyptr);

struct hash_pair {
    void *key;
    unsigned char value[0];
} __attribute__((packed));

/* insert n key->pair members of size pairsize */
void hash_insert_pairs(void*, const struct hash_pair*, size_t pairsize, size_t n);

/* gets the original pointer used to store the tuple */
void *hash_getkeyptr(void*, const void *key);

#define SIMP_HASH(TYPE, NAME)                  \
    unsigned NAME (const void *key)            \
    {                                          \
        return (unsigned)*((const TYPE*)key);  \
    }

#define SIMP_EQUAL(TYPE, NAME)                                          \
    int NAME (const void *a, const void *b)                             \
    {                                                                   \
        return memcmp((a), (b), sizeof(TYPE));                          \
    }

size_t hash_size(void*);

void *hash_dup(void*);

/* sets the callback for when duplicating a data node */
void hash_setdupdata_cb(void*, void *(*cb)(void*));

struct hash_export_node {
    unsigned hash;
    void *last, *node, *next;
};

struct hash_export_node hash_get_internal_node(void *ptr, const void *key);

void hash_del_internal_node(void *ptr, const struct hash_export_node *node);
