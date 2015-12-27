/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2015 Franklin Wei
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

#include <stdbool.h>
#include <stddef.h>

/* simple, generic chained hash map implementation */

unsigned hash_djb(const void*);
int compare_strings(const void*, const void*);

void *hash_init(size_t tabsz, unsigned (*hash_fn)(const void*),
                int (*compare_key)(const void*, const void*));

void hash_setfreedata_cb(void*, void (*cb)(void *data));
void hash_setfreekey_cb(void*,  void (*cb)(void *key));

void hash_free(void*);

/* insert a pair, returns null if not already found, otherwise
   return the existing data pointer */
void *hash_insert(void*, const void *key, const void *data);

/* returns NULL if not found */
void *hash_lookup(void*, const void *key);

bool hash_remove(void *ptr, const void *key);

/* use like you would strtok_r */
/* allocates a buffer that's freed once all elements are processed */
void *hash_iterate(void *map, void **saved, void **keyptr);

struct hash_pair {
    void *key;
    unsigned char value[0];
};

/* insert n key->pair members of size pairsize */
void hash_insert_pairs(void*, const struct hash_pair*, size_t pairsize, size_t n);

/* gets the original pointer used to store the tuple */
void *hash_getkeyptr(void*, const void *key);

#define SIMP_HASH(TYPE, NAME)       \
    unsigned NAME (const void *key) \
    {                               \
        return *((TYPE*)key);        \
    }

#define SIMP_EQUAL(TYPE, NAME)                                          \
    int NAME (const void *a, const void *b)                             \
    {                                                                   \
        return !(*((TYPE*)a) == *((TYPE*)b));                           \
    }
