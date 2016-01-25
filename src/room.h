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

/* Our world is an array of rooms, each having a list of objects in
   them, as well as actions that can be performed in the room. Objects
   are added by hooks in rooms, which are provided by the world
   module. */

typedef enum room_id { ROOM_NONE = -1 } room_id;

typedef struct int128 {
    uint64_t halves[2];
} int128;

typedef int128 obj_id;

typedef struct child_data user_t;

enum direction_t { DIR_N = 0, DIR_NE, DIR_E, DIR_SE, DIR_S, DIR_SW, DIR_W, DIR_NW, DIR_UP, DIR_DN, DIR_IN, DIR_OT, NUM_DIRECTIONS };

/* the data we get from a world module */
struct roomdata_t {
    /* the non-const pointers can be modified by the world module */
    const char * const uniq_id;

    /* mutable properties */
    char *name;
    char *desc;

    const char * const adjacent[NUM_DIRECTIONS];

    void (* const hook_init)(room_id id);
    void (* const hook_enter)(room_id room, user_t *user);
    void (* const hook_leave)(room_id room, user_t *user);
};

struct object_t {
    obj_id id; // don't modify

    const char *name; /* no articles: "a", "an", "the" */

    void *userdata;

    bool can_take;
    bool list;

    void (*hook_serialize)(int fd, struct object_t*);
    bool (*hook_take)(struct object_t*, user_t *user);
    void (*hook_drop)(struct object_t*, user_t *user);
    void (*hook_use)(struct object_t*, user_t *user);
    void (*hook_destroy)(struct object_t*);
    const char* (*hook_desc)(struct object_t*, user_t*);
};

struct verb_t {
    const char *name;

    /* toks is strtok_r's pointer */
    void (*execute)(const char *toks, user_t *user);
};

struct room_t {
    room_id id;
    struct roomdata_t data;

    room_id adjacent[NUM_DIRECTIONS];

    /* hash maps */
    void *objects; /* object name -> object */
    void *verbs;
    void *users; /* username -> child_data */
};

/* room/world */
void world_init(const struct roomdata_t *data, size_t sz, const char *name);
bool world_load(const char *fname, const struct roomdata_t *data, size_t data_sz, const char *world_name);
void world_save(const char *fname);

struct room_t *room_get(room_id id);
bool room_user_add(room_id id, struct child_data *child);
bool room_user_del(room_id id, struct child_data *child);

/* returns a new object with a unique id */
struct object_t *obj_new(void);

/* new should point to a new object allocated with obj_new(), with
 * 'name' properly set */
bool obj_add(room_id room, struct object_t *obj);

/* on the first call, room should be a valid room id, and *save should
 * point to a void pointer. On subsequent calls, room should be
 * ROOM_NONE, and *save should remain unchanged from the previous
 * call */
struct object_t *room_obj_iterate(room_id room, void **save);

/* obj should be all lowercase */
struct object_t *room_obj_get(room_id room, const char *obj);

void world_free(void);
