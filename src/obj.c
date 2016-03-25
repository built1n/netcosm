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
#include "obj.h"

/* map of class names -> object classes */
static void *obj_class_map = NULL;

static obj_id idcounter = 1;

obj_id obj_get_idcounter(void)
{
    return idcounter;
}

void obj_set_idcounter(obj_id c)
{
    idcounter = c;
}

struct object_t *obj_new(const char *class_name)
{
    if(!obj_class_map)
    {
        extern const struct obj_class_t netcosm_obj_classes[];
        extern const size_t netcosm_obj_classes_sz;
        obj_class_map = hash_init(netcosm_obj_classes_sz / 2 + 1,
                                  hash_djb,
                                  compare_strings);
        for(unsigned i = 0; i < netcosm_obj_classes_sz; ++i)
        {
            if(hash_insert(obj_class_map,
                           netcosm_obj_classes[i].class_name,
                           netcosm_obj_classes + i))
                error("duplicate object class name '%s'", netcosm_obj_classes[i].class_name);
        }
    }

    struct object_t *obj = calloc(1, sizeof(struct object_t));

    obj->class = hash_lookup(obj_class_map, class_name);

    if(!obj->class)
    {
        free(obj);
        error("unknown object class '%s'", class_name);
    }

    obj->id = idcounter++;
    obj->refcount = 1;
    obj->hidden = false;
    obj->default_article = true;

    return obj;
}

void obj_write(int fd, struct object_t *obj)
{
    write_string(fd, obj->class->class_name);

    write_uint64(fd, obj->id);

    write_string(fd, obj->name);
    write_bool(fd, obj->hidden);
    write_bool(fd, obj->default_article);

    struct obj_alias_t *iter = obj->alias_list;
    while(iter)
    {
        write_string(fd, iter->alias);
        iter = iter->next;
    }
    write_string(fd, "");

    if(obj->class->hook_serialize)
        obj->class->hook_serialize(fd, obj);
}

struct object_t *obj_read(int fd)
{
    char *class_name = read_string(fd);
    struct object_t *obj = obj_new(class_name);
    free(class_name);

    obj->id = read_uint64(fd);

    obj->name = read_string(fd);
    obj->hidden = read_bool(fd);
    obj->default_article = read_bool(fd);

    /* aliases */
    struct obj_alias_t *last = NULL;
    while(1)
    {
        char *alias = read_string(fd);
        if(alias[0] == '\0')
        {
            free(alias);
            break;
        }
        struct obj_alias_t *new = calloc(1, sizeof(*new));
        new->alias = alias;
        if(last)
            last->next = new;
        else
            obj->alias_list = new;
        last = new;
    }

    if(obj->class->hook_deserialize)
        obj->class->hook_deserialize(fd, obj);

    return obj;
}

struct object_t *obj_copy(struct object_t *obj)
{
    struct object_t *ret = obj_new(obj->class->class_name);
    ret->name = strdup(obj->name);
    ret->hidden = obj->hidden;
    if(obj->class->hook_dupdata)
        ret->userdata = obj->class->hook_dupdata(obj);
    return ret;
}

struct object_t *obj_dup(struct object_t *obj)
{
    debugf("Adding an object reference to #%lu.\n", obj->id);
    ++obj->refcount;
    return obj;
}

void obj_free(void *ptr)
{
    struct object_t *obj = ptr;
    --obj->refcount;

    debugf("Freeing an object reference for #%lu (%s, %d).\n", obj->id, obj->name, obj->refcount);

    if(!obj->refcount)
    {
        debugf("Freeing object #%lu\n", obj->id);
        if(obj->class->hook_destroy)
            obj->class->hook_destroy(obj);

        struct obj_alias_t *iter = obj->alias_list;
        while(iter)
        {
            struct obj_alias_t *next = iter->next;
            free(iter->alias);
            free(iter);
            iter = next;
        }

        free(obj->name);
        free(obj);
    }
}

void obj_shutdown(void)
{
    hash_free(obj_class_map);
    obj_class_map = NULL;
}

int obj_compare(const void *a, const void *b)
{
    const struct object_t *c = a, *d = b;
    return !(c->id == d->id);
}

size_t obj_count_noalias(const void *a)
{
    size_t ret = 0;
    void *save;
    while(1)
    {
        const struct multimap_list *iter = multimap_iterate(a, &save, NULL);
        a = NULL;
        if(!iter)
            break;
        while(iter)
        {
            struct object_t *obj = iter->val;
            if(!strcmp(iter->key, obj->name))
                ++ret;
            iter = iter->next;
        }
    }
    return ret;
}
