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

#include <world_api.h>

/* This is a sample world implemented in NetCosm. */

/* A world is composed of rooms, object classes, and verb classes. For
 * now they are defined as global arrays, but this is subject to change.
 */

/* This is our array of rooms. Each contains multiple callbacks and
   strings pointing to other rooms, see room.h for details */
const struct roomdata_t netcosm_world [] = {
    {
        "uniq_id", // this must be globally unique
        "Room name",
        "Initial description",

        /* these can be replaced with strings identifying other rooms */
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },

        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

const size_t netcosm_world_sz = ARRAYLEN(netcosm_world);
const char *netcosm_world_name = "World Name Here";

/********* OBJECTS *********/

static void generic_ser(int fd, struct object_t *obj)
{
    write_string(fd, obj->userdata);
}

static void generic_deser(int fd, struct object_t *obj)
{
    obj->userdata = read_string(fd);
}

static void generic_destroy(struct object_t *obj)
{
    free(obj->userdata);
}

static const char *generic_desc(struct object_t *obj, user_t *user)
{
    (void) user;
    return obj->userdata;
}

static void *generic_dup(struct object_t *obj)
{
    return strdup(obj->userdata);
}

const struct obj_class_t netcosm_obj_classes[] = {
    {
        "/generic",
        generic_ser,
        generic_deser,
        NULL,
        NULL,
        generic_destroy,
        generic_desc,
        generic_dup,
    },
};

const size_t netcosm_obj_classes_sz = ARRAYLEN(netcosm_obj_classes);

/********* VERBS *********/

const struct verb_class_t netcosm_verb_classes[] = {

};

const size_t netcosm_verb_classes_sz = ARRAYLEN(netcosm_verb_classes);
