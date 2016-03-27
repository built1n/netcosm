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
#include "multimap.h"
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
        bool ret = !hash_insert(room->users, child->user, child);
        if(room->data.hook_enter)
            room->data.hook_enter(id, child);
        return ret;
    }
    else
        return false;
}

bool room_user_del(room_id id, struct child_data *child)
{
    struct room_t *room = room_get(id);

    if(child->user)
    {
        bool ret = hash_remove(room->users, child->user);
        if(room->data.hook_leave)
            room->data.hook_leave(id, child);
        return ret;
    }
    else
        return false;
}

void room_free(struct room_t *room)
{
    if(room->data.hook_destroy)
        room->data.hook_destroy(room->id);

    hash_free(room->users);
    room->users = NULL;

    multimap_free(room->objects);
    room->objects = NULL;

    hash_free(room->verbs);
    room->verbs = NULL;

    free(room->data.name);
    free(room->data.desc);
}

bool room_obj_add(room_id room, struct object_t *obj)
{
    debugf("ADDING OBJECT %s to ROOM %d\n", obj->name, room);

    bool status = true;
    if(!multimap_insert(room_get(room)->objects, obj->name, obj))
    {
        debugf("FAILED TO ADD OBJECT.\n");
        status = false;
    }

    struct obj_alias_t *iter = obj->alias_list;
    while(iter)
    {
        if(!multimap_insert(room_get(room)->objects, iter->alias, obj_dup(obj)))
        {
            status = false;

            debugf("Status is false\n");
        }
        iter = iter->next;
    }

    debugf("ROOM %d now has %d objects\n", room, multimap_size(room_get(room)->objects));

    return status;
}

bool room_obj_add_alias(room_id room, struct object_t *obj, char *alias)
{
    /* the primary name must be added */
    if(!room_obj_get(room, obj->name))
        room_obj_add(room, obj);

    /* don't add an alias with the same name as the object */
    if(!strcmp(alias, obj->name))
        return false;

    /* make sure this alias isn't already added */
    struct obj_alias_t *iter = obj->alias_list;
    while(iter)
    {
        if(!strcasecmp(iter->alias, alias))
            return false;
        iter = iter->next;
    }

    debugf("adding alias '%s' for object '%s'\n", alias, obj->name);

    debugf("ROOM %d now has %d total object names\n", room, multimap_size(room_get(room)->objects));

    struct obj_alias_t *new = calloc(1, sizeof(struct obj_alias_t));

    new->alias = strdup(alias);

    new->next = obj->alias_list;
    obj->alias_list = new;

    ++obj->n_alias;

    bool status = multimap_insert(room_get(room)->objects, alias, obj_dup(obj));

    return status;
}

const struct multimap_list *room_obj_iterate(room_id room, void **save, size_t *n_pairs)
{
    if(room != ROOM_NONE)
        return multimap_iterate(room_get(room)->objects, save, n_pairs);
    else
        return multimap_iterate(NULL, save, n_pairs);
}

const struct multimap_list *room_obj_get(room_id room, const char *name)
{
    return multimap_lookup(room_get(room)->objects, name, NULL);
}

const struct multimap_list *room_obj_get_size(room_id room, const char *name, size_t *n_objs)
{
    return multimap_lookup(room_get(room)->objects, name, n_objs);
}

size_t room_obj_count(room_id room)
{
    return multimap_size(room_get(room)->objects);
}

size_t room_obj_count_noalias(room_id id)
{
    return obj_count_noalias(room_get(id)->objects);
}

/* delete a specific object and its aliases */

bool room_obj_del_by_ptr(room_id room, struct object_t *obj)
{
    struct obj_alias_t *iter = obj->alias_list;

    struct object_t tmp;
    tmp.id = obj->id;

    while(iter)
    {
        multimap_delete(room_get(room)->objects, iter->alias, &tmp);
        iter = iter->next;
    }

    return multimap_delete(room_get(room)->objects, obj->name, &tmp);
}

/* delete all the objects with a matching name, and all their aliases
 * from a room */

bool room_obj_del(room_id room, const char *name)
{
    const struct multimap_list *iter = multimap_lookup(room_get(room)->objects, name, NULL);
    if(!iter)
        return false;
    while(iter)
    {
        struct multimap_list *next = iter->next;
        struct object_t *obj = iter->val;
        room_obj_del_by_ptr(room, obj);
        iter = next;
    }
    return true;
}

#define OBJMAP_SIZE 8
#define VERBMAP_SZ 8

/* initialize the room's hash tables */
void room_init_maps(struct room_t *room)
{
    room->users = hash_init((userdb_size() / 2) + 1, hash_djb, compare_strings);

    room->objects = multimap_init(OBJMAP_SIZE, hash_djb, compare_strings_nocase, obj_compare);
    multimap_setfreedata_cb(room->objects, obj_free);

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
