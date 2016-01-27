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
#include "obj.h"

/* map of class names -> object classes */
static void *obj_class_map = NULL;

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
                error("duplicate object class name");
        }
    }

    struct object_t *obj = calloc(1, sizeof(struct object_t));

    obj->class = hash_lookup(obj_class_map, class_name);
    if(!obj->class)
    {
        free(obj);
        error("unknown object class '%s'", class_name);
    }
    else
        return obj;
}

void obj_write(int fd, struct object_t *obj)
{
    write_string(fd, obj->class->class_name);
    write_string(fd, obj->name);
    write_bool(fd, obj->list);

    if(obj->class->hook_serialize)
        obj->class->hook_serialize(fd, obj);
}

struct object_t *obj_read(int fd)
{
    char *class_name = read_string(fd);
    struct object_t *obj = obj_new(class_name);
    free(class_name);

    obj->name = read_string(fd);
    obj->list = read_bool(fd);
    if(obj->class->hook_deserialize)
        obj->class->hook_deserialize(fd, obj);

    return obj;
}

struct object_t *obj_dup(struct object_t *obj)
{
    struct object_t *new = calloc(1, sizeof(struct object_t));
    memcpy(new, obj, sizeof(*new));
    new->name = strdup(obj->name);
    new->userdata = obj->class->hook_clone(obj->userdata);
    return new;
}

void obj_free(void *ptr)
{
    struct object_t *obj = ptr;
    debugf("Freeing obj %s\n", obj->name);

    if(obj->class->hook_destroy)
        obj->class->hook_destroy(obj);

    free(obj->name);
    free(obj);
}

void obj_shutdown(void)
{
    hash_free(obj_class_map);
    obj_class_map = NULL;
}
