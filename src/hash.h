#include <stdbool.h>
#include <stddef.h>

/* simple, generic chained hash map implementation */

unsigned hash_djb(const void*);
int compare_strings(const void*, const void*);

void *hash_init(size_t tabsz, unsigned (*hash_fn)(const void*),
                int (*compare_key)(const void*, const void*));

void hash_free(void*);

/* insert a pair, returns null if not already found, otherwise
   return the existing data pointer */
void *hash_insert(void*, const void *key, const void *data);

/* returns NULL if not found */
void *hash_lookup(void*, const void *key);

bool hash_remove(void *ptr, void *key);

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
