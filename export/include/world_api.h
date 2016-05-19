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

#include "globals.h"

#include "server.h"
#include "server_reqs.h"
#include "hash.h"
#include "multimap.h"
#include "userdb.h"
#include "world.h"

/* world modules should only call the functions provided in this
 * structure, and those in the standard library */

struct world_api {
    /* object */
    struct object_t *(*obj_new)(const char *class);
    struct object_t *(*obj_dup)(struct object_t *obj); // inc. ref. count
    struct object_t *(*obj_copy)(struct object_t *obj); // full copy
    void (*obj_free)(void*);

    /* room */
    void (*room_user_teleport)(user_t *child, room_id id);
    bool (*room_obj_add)(room_id room, struct object_t*);
    bool (*room_obj_add_alias)(room_id room, struct object_t*, const char *alias);
    bool (*room_obj_del)(room_id room, const char *name);
    bool (*room_obj_del_by_ptr)(room_id room, struct object_t *obj);
    const struct multimap_list *(*room_obj_get)(room_id room, const char *obj);
    const struct multimap_list *(*room_obj_get_size)(room_id room, const char *name, size_t *n_objs);
    size_t (*room_obj_count)(room_id room);
    size_t (*room_obj_count_noalias)(room_id id); // doesn't count aliases
    bool   (*room_verb_add)(room_id room, struct verb_t*);
    bool   (*room_verb_del)(room_id room, const char *verbname);
    void   *(*room_verb_map)(room_id room); // hash map of local verbs
    struct room_t *(*room_get)(room_id id);
    room_id (*room_get_id)(const char *name);

    /* world */
    bool  (*world_verb_add)(struct verb_t*);
    bool  (*world_verb_del)(struct verb_t*);
    void *(*world_verb_map)(void); // gets hash map of global verbs

    /* verb */
    struct verb_t *(*verb_new)(const char *class);
    void (*verb_free)(void *verb);

    /* hash map */
    unsigned (*hash_djb)(const void*);
    int (*compare_strings)(const void*, const void*);
    int (*compare_strings_nocase)(const void*, const void*);

    void *(*hash_init)(size_t tabsz, unsigned (*hash_fn)(const void *key),
                       int (*compare_key)(const void*, const void*));
    void (*hash_setfreedata_cb)(void*, void (*cb)(void *data));
    void (*hash_setfreekey_cb)(void*,  void (*cb)(void *key));
    void (*hash_free)(void*);
    void *(*hash_insert)(void*, const void *key, const void *data);
    void (*hash_overwrite)(void*, const void *key, const void *data);
    void *(*hash_lookup)(void*, const void *key);
    bool (*hash_remove)(void *ptr, const void *key);
    void *(*hash_iterate)(void *map, void **saved, void **keyptr);
    void (*hash_insert_pairs)(void*, const struct hash_pair*, size_t pairsize, size_t n);
    void *(*hash_getkeyptr)(void*, const void *key);
    void *(*hash_dup)(void*);
    void (*hash_setdupdata_cb)(void*, void *(*cb)(void*));

    /* multimap */
    void *(*multimap_init)(size_t tabsz,
                           unsigned (*hash_fn)(const void *key),
                           int (*compare_key)(const void *key_a, const void *key_b),
                           int (*compare_val)(const void *val_a, const void *val_b));
    void (*multimap_free)(void*);
    const struct multimap_list *(*multimap_lookup)(void *map, const void *key, size_t *n_pairs);
    bool (*multimap_insert)(void *map, const void *key, const void *val);
    size_t (*multimap_delete)(void *map, const void *key, const void *val);
    size_t (*multimap_delete_all)(void *map, const void *key);

    /* returns a linked list, NOT individual items of a linked list */
    const struct multimap_list *(*multimap_iterate)(const void *map, void **save, size_t *n_pairs);

    size_t (*multimap_size)(void *map);
    void (*multimap_setfreedata_cb)(void *map, void (*)(void*));
    void *(*multimap_dup)(void *ptr);
    void (*multimap_setdupdata_cb)(void *ptr, void *(*cb)(void *ptr));
    void *(*multimap_copy)(void *ptr);

    /* server */
    void (*send_msg)(user_t *child, const char *fmt, ...) __attribute__((format(printf,2,3)));
    void (*child_toggle_rawmode)(user_t *child, void (*cb)(user_t*, char *data, size_t len));

    /* userdb */
    struct userdata_t *(*userdb_lookup)(const char *username);
    bool (*userdb_remove)(const char *username);
    size_t (*userdb_size)(void);
    bool (*userdb_add)(struct userdata_t*);
    /* (*save) should be set to NULL on the first run */
    struct userdata_t *(*userdb_iterate)(void **save);
    bool (*userdb_add_obj)(const char *username, struct object_t *obj);
    bool (*userdb_del_obj)(const char *username, const char *obj_name);
    bool (*userdb_del_obj_by_ptr)(const char *username, struct object_t *obj);

    /* util */
    void     (*error)(const char *fmt, ...) __attribute__((noreturn,format(printf,1,2)));
    void     (*all_upper)(char*);
    void     (*all_lower)(char*);

    void     (*write_string)(int fd, const char *str);
    char*    (*read_string)(int fd);
    void     (*write_roomid)(int fd, room_id *id);
    room_id  (*read_roomid)(int fd);
    void     (*write_bool)(int fd, bool b);
    bool     (*read_bool)(int fd);
    void     (*write_uint32)(int fd, uint32_t i);
    uint32_t (*read_uint32)(int fd);
    void     (*write_uint64)(int fd, uint64_t i);
    uint64_t (*read_uint64)(int fd);
    void     (*write_size)(int fd, size_t);
    size_t   (*read_size)(int fd);
    void     (*write_int)(int fd, int i);
    int      (*read_int)(int fd);

    bool (*is_vowel)(char c);
    size_t (*strlcat)(char *dst, const char *src, size_t siz);
    char *(*format_noun)(char *buf, size_t len, const char *name,
                         size_t count, bool default_article, bool capitalize);


};

/* defined in src/world_api.c  */
extern const struct world_api *nc;
