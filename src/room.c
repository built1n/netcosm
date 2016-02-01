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

#include "globals.h"

#include "hash.h"
#include "server.h"
#include "room.h"
#include "userdb.h"
#include "world.h"

bool room_user_add(room_id id, struct child_data *child)
{
    struct room_t *room = room_get(id);
    /* make sure there are no duplicates */

    if(!room)
        error("unknown room %d", id);

    if(child->user)
    {
        /* hash_insert returns NULL on success */
        return !hash_insert(room->users, child->user, child);
    }
    else
        return false;
}

bool room_user_del(room_id id, struct child_data *child)
{
    struct room_t *room = room_get(id);

    if(child->user)
        return hash_remove(room->users, child->user);
    else
        return false;
}

void room_free(struct room_t *room)
{
    hash_free(room->users);
    room->users = NULL;

    hash_free(room->objects);
    room->objects = NULL;

    hash_free(room->verbs);
    room->verbs = NULL;

    free(room->data.name);
    free(room->data.desc);
}

bool room_obj_add(room_id room, struct object_t *obj)
{
    return !hash_insert(room_get(room)->objects, obj->name, obj);
}

struct object_t *room_obj_iterate(room_id room, void **save)
{
    if(room != ROOM_NONE)
        return hash_iterate(room_get(room)->objects, save, NULL);
    else
        return hash_iterate(NULL, save, NULL);
}

struct object_t *room_obj_get(room_id room, const char *name)
{
    return hash_lookup(room_get(room)->objects, name);
}

size_t room_obj_count(room_id room)
{
    return hash_size(room_get(room)->objects);
}

bool room_obj_del(room_id room, const char *name)
{
    return hash_remove(room_get(room)->objects, name);
}

#define OBJMAP_SIZE 8
#define VERBMAP_SZ 8

/* initialize the room's hash tables */
void room_init_maps(struct room_t *room)
{
    room->users = hash_init((userdb_size() / 2) + 1, hash_djb, compare_strings);

    room->objects = hash_init(OBJMAP_SIZE, hash_djb, compare_strings);
    hash_setfreedata_cb(room->objects, obj_free);

    room->verbs = hash_init(VERBMAP_SZ,
                            hash_djb,
                            compare_strings);

    hash_setfreedata_cb(room->verbs, verb_free);
}

void *room_verb_map(room_id id)
{
    return room_get(id)->verbs;
}

bool room_verb_add(room_id id, struct verb_t *verb)
{
    return !hash_insert(room_get(id)->verbs, verb->name, verb);
}

bool room_verb_del(room_id id, const char *name)
{
    return hash_remove(room_get(id), name);
}
