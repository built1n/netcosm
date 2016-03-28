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
#include "server.h"

/* the verb API is modeled after that of obj_*, this allows for
 * dynamic creation/deletion of verbs, but is also easily
 * serializable.
 *
 * so, all verbs are part of a verb class, which has all of its
 * callbacks.
 */

struct verb_t;
struct verb_class_t {
    const char *class_name;

    void (*hook_exec)(struct verb_t*, char *args, user_t *user);
};

struct verb_t {
    char *name;

    struct verb_class_t *class;
};

struct verb_t *verb_new(const char *class);

void verb_write(int fd, struct verb_t*);

struct verb_t *verb_read(int fd);

void verb_free(void *verb);

/* free the verb_ module's internal data structures */
void verb_shutdown(void);
