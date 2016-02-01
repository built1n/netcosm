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

#include "obj.h"
#include "verb.h"

/* Our world is an array of rooms, each having a list of objects in
   them, as well as actions that can be performed in the room. Objects
   are added by hooks in rooms, which are provided by the world
   module. */

typedef enum room_id { ROOM_NONE = -1 } room_id;

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
bool room_user_add(room_id id, struct child_data *child);
bool room_user_del(room_id id, struct child_data *child);

/* on the first call, room should be a valid room id, and *save should
 * point to a void pointer. On subsequent calls, room should be
 * ROOM_NONE, and *save should remain unchanged from the previous
 * call */
struct object_t *room_obj_iterate(room_id room, void **save);

/* new should point to a new object allocated with obj_new(), with
 * 'name' properly set */
bool room_obj_add(room_id room, struct object_t *obj);

/* obj should be all lowercase */
struct object_t *room_obj_get(room_id room, const char *obj);

size_t room_obj_count(room_id room);

bool room_obj_del(room_id room, const char *name);

/* local verbs override global verbs */
bool room_verb_add(room_id room, struct verb_t*);
bool room_verb_del(room_id room, const char *verbname);

/* get the local map of verbs */
void *room_verb_map(room_id room);

/* free a room and its resources */
void room_free(struct room_t *room);

/* semi-protected, should only be called from world_ */
void room_init_maps(struct room_t *room);
