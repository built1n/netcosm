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
#include "verb.h"
#include "world.h"

static void *map = NULL;

struct verb_t *verb_new(const char *class_name)
{
    if(!map)
    {
        map = hash_init(netcosm_verb_classes_sz,
                        hash_djb,
                        compare_strings);

        for(unsigned i = 0; i < netcosm_verb_classes_sz; ++i)
        {
            if(hash_insert(map, netcosm_verb_classes[i].class_name,
                           netcosm_verb_classes + i))
                error("duplicate verb class '%s'", netcosm_verb_classes[i].class_name);
        }
    }

    struct verb_t *new = calloc(1, sizeof(struct verb_t));

    new->class = hash_lookup(map, class_name);
    if(!new->class)
        error("world module attempted to instantiate a verb of unknown class '%s'", class_name);

    return new;
}

void verb_write(int fd, struct verb_t *verb)
{
    write_string(fd, verb->class->class_name);
    write_string(fd, verb->name);
}

struct verb_t *verb_read(int fd)
{
    char *class_name = read_string(fd);
    struct verb_t *ret = verb_new(class_name);
    free(class_name);

    ret->name = read_string(fd);
    return ret;
}

void verb_free(void *ptr)
{
    struct verb_t *verb = ptr;
    free(verb->name);
    free(verb);
}

void verb_shutdown(void)
{
    if(map)
    {
        hash_free(map);
        map = NULL;
    }
}
