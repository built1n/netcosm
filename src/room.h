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

typedef enum room_id { ROOM_NONE = -1 } room_id;

typedef enum obj_id { OBJ_NONE = -1 } obj_id;

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
    void (* const hook_enter)(room_id room, pid_t player);
    void (* const hook_say)(room_id room, pid_t player, const char *msg);
    void (* const hook_leave)(room_id room, pid_t player);
};

struct user_t {
    struct child_data *data;
    struct user_t *next;
};

struct object_t {
    const char *class;
    obj_id id;
};

struct verb_t {
    const char *name;

    /* toks is strtok_r's pointer */
    void (*execute)(const char *toks);
};

struct room_t {
    room_id id;
    struct roomdata_t data;

    room_id adjacent[NUM_DIRECTIONS];

    /* arrays instead of linked lists because insertion should be rare for these */
    size_t objects_sz;
    struct object_t *objects;

    size_t verbs_sz;
    struct verb_t *verbs;

    /* linked list for users, random access is rare */
    struct user_t *users;
    int num_users;
};

/* room/world */
void world_init(const struct roomdata_t *data, size_t sz, const char *name);
bool world_load(const char *fname, const struct roomdata_t *data, size_t data_sz, const char *world_name);
void world_save(const char *fname);

struct room_t *room_get(room_id id);
bool room_user_add(room_id id, struct child_data *child);
bool room_user_del(room_id id, struct child_data *child);

void world_free(void);