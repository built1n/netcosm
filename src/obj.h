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

#include "room.h"

/* Objects belong to an object class. Objects define their object
 * class through the class name, which is converted to a class ID
 * internally.
 */

struct object_t;

typedef struct child_data user_t; // see server.h for the definition

struct obj_class_t {
    const char *class_name;

    /* write an object's user data to disk */
    void (*hook_serialize)(int fd, struct object_t*);

     /* read an object's user data */
    void (*hook_deserialize)(int fd, struct object_t*);

    /* called when an object is taken;
     * true = can take
     * false = can't take
     * no function (NULL) = can take */
    bool (*hook_take)(struct object_t*, user_t *user);

    /* called when dropping an object;
     * true: can drop
     * false: can't drop
     * NULL: can drop
     */
    bool (*hook_drop)(struct object_t*, user_t *user);
    void (*hook_destroy)(struct object_t*); // free resources
    const char* (*hook_desc)(struct object_t*, user_t*); // get object description

    void *(*hook_dupdata)(struct object_t *obj); // duplicate the userdata pointer
};

typedef uint64_t obj_id;

struct obj_alias_t {
    char *alias;
    struct obj_alias_t *next;
};

/* world modules should not instantiate this directly, use obj_new() instead */
/* also, members marked with 'protected' should not be modified by the world module */
struct object_t {
    obj_id id; // protected

    /* the object name needs to be freeable with free(), and should
     * not be modified by the world module after being added to a
     * room */
    char *name;

    struct obj_class_t *class;

    size_t n_alias;
    struct obj_alias_t *alias_list;

    bool list; /* whether to list in room view */
    bool default_article; /* whether or not to use 'a' or 'an' when describing this */

    void *userdata;

    unsigned refcount; // private
};

/* returns a new object of class 'c' */
struct object_t *obj_new(const char *c);

/* serialize an object */
void obj_write(int fd, struct object_t *obj);

/* deserialize an object */
struct object_t *obj_read(int fd);

/* this adds a reference to an object, DOES NOT COPY */
struct object_t *obj_dup(struct object_t *obj);

/* makes a new object with a new ID, but same data fields as the original */
struct object_t *obj_copy(struct object_t *obj);

/* this only frees the object if its reference count is zero */
void obj_free(void*);

/* shut down the obj_* module */
void obj_shutdown(void);

/* internal use */
obj_id obj_get_idcounter(void);
void obj_set_idcounter(obj_id);

/* compare two objects */
int obj_compare(const void *a, const void *b);

/* count the number of non-alias objects in the given multimap */
size_t obj_count_noalias(const void *multimap);
