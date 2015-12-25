#include <stddef.h>
#include <stdlib.h>

/* simple, generic hash map implementation */

unsigned hash_djb(const char*);

void *hash_init(size_t tabsz, unsigned (*hash_fn)(const void*),
                int (*compare_key)(const void*, const void*));

void hash_free(void*);

/* insert a pair, returns null if not already found, otherwise
   return the existing data pointer */
void *hash_insert(void*, const void *key, const void *data);

/* returns NULL if not found */
void *hash_lookup(void*, const void *key);
