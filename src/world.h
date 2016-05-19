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
#include "room.h"
#include "verb.h"

/* the world module MUST define all of the following: */

#ifndef _WORLD_MODULE_
/* note that these are now dynamically loaded */

/* verb classes */
extern const struct verb_class_t *netcosm_verb_classes;
extern size_t netcosm_verb_classes_sz;

/* object classes */
extern const struct obj_class_t *netcosm_obj_classes;
extern size_t netcosm_obj_classes_sz;

/* rooms */
extern const struct roomdata_t *netcosm_world;
extern size_t netcosm_world_sz;

/* simulation callback */
extern void (*netcosm_world_simulation_cb)(void);
extern unsigned netcosm_world_simulation_interval;

/* user data callback */
extern void (*netcosm_write_userdata_cb)(int fd, void *ptr);
extern void* (*netcosm_read_userdata_cb)(int fd);

extern const char *netcosm_world_name;
#endif

/* loads the world into RAM for the first time, resets the game
 * state */
void world_init(const struct roomdata_t *data, size_t sz, const char *name);

void world_save(const char *fname);

/* loads the world from disk */
bool world_load(const char *fname, const struct roomdata_t *data, size_t data_sz, const char *world_name);

/** global verbs **/
bool world_verb_add(struct verb_t*);
bool world_verb_del(struct verb_t*);

/* gets the map of global verbs */
void *world_verb_map(void);

void world_free(void);

/* this goes in world_ and not room_ */
struct room_t *room_get(room_id id);

room_id room_get_id(const char *uniq_id);
