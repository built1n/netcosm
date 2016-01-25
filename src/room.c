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

/* processed world data */

static struct room_t *world;
static size_t world_sz;
static char *world_name;

struct room_t *room_get(room_id id)
{
    return world + id;
}

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

void world_save(const char *fname)
{
    int fd = open(fname, O_CREAT | O_WRONLY, 0644);
    uint32_t magic = WORLD_MAGIC;
    write(fd, &magic, sizeof(magic));
    write(fd, &world_sz, sizeof(world_sz));
    write_string(fd, world_name);
    for(unsigned i = 0; i < world_sz; ++i)
    {
        write_roomid(fd, &world[i].id);
        /* unique ID never changes */
        //write_string(fd, world[i].data.uniq_id);
        write_string(fd, world[i].data.name);
        write_string(fd, world[i].data.desc);
        /* adjacency strings not serialized, only adjacent IDs */

        /* callbacks are static, so are not serialized */

        write(fd, world[i].adjacent, sizeof(world[i].adjacent));

        /* now we serialize all the objects in this room */

        size_t n_objects = room_obj_count(i);
        write(fd, &n_objects, sizeof(n_objects));

        room_id id = i;
        void *save;
        while(1)
        {
            struct object_t *obj = room_obj_iterate(id, &save);
            if(!obj)
                break;
            id = ROOM_NONE;

        }
    }
    close(fd);
}

static void room_free(struct room_t *room)
{
    hash_free(room->users);
    room->users = NULL;
    hash_free(room->objects);
    room->objects = NULL;
    free(room->data.name);
    free(room->data.desc);
}

void world_free(void)
{
    if(world)
    {
        if(world_name)
            free(world_name);
        for(unsigned i = 0; i < world_sz; ++i)
        {
            room_free(world + i);
        }
        free(world);
        world = NULL;
    }
}

bool room_obj_add(room_id room, struct object_t *obj)
{
    return !hash_insert(room_get(room)->objects, obj->name, obj);
}

#define OBJMAP_SIZE 8

static void free_obj(void *ptr)
{
    struct object_t *obj = ptr;
    if(obj->class->hook_destroy)
        obj->class->hook_destroy(obj);
    free(obj);
}

/* initialize the room's hash tables */
static void room_init_maps(struct room_t *room)
{
    room->users = hash_init((userdb_size() / 2) + 1, hash_djb, compare_strings);

    room->objects = hash_init(OBJMAP_SIZE, hash_djb, compare_strings);
    hash_setfreedata_cb(room->objects, free_obj);
}

/**
 * Loads a world using data on disk and in memory.
 *
 * @param fname world file
 * @param data world module data
 * @param data_sz number of rooms in world module
 */
bool world_load(const char *fname, const struct roomdata_t *data, size_t data_sz, const char *name)
{
    int fd = open(fname, O_RDONLY);
    if(fd < 0)
        return false;
    uint32_t magic;
    read(fd, &magic, sizeof(magic));
    if(magic != WORLD_MAGIC)
        return false;

    if(world)
        world_free();

    read(fd, &world_sz, sizeof(world_sz));

    if(world_sz != data_sz)
        return false;

    world = calloc(world_sz, sizeof(struct room_t));

    world_name = read_string(fd);
    if(strcmp(name, world_name))
    {
        free(world_name);
        debugf("Incompatible world state.\n");
        return false;
    }

    for(unsigned i = 0; i < world_sz; ++i)
    {
        room_init_maps(world + i);

        world[i].id = read_roomid(fd);
        memcpy(&world[i].data, data + i, sizeof(struct roomdata_t));
        world[i].data.name = read_string(fd);
        world[i].data.desc = read_string(fd);
        if(read(fd, world[i].adjacent, sizeof(world[i].adjacent)) < 0)
            return false;

        size_t n_objects;
        if(read(fd, &n_objects, sizeof(n_objects)) != sizeof(n_objects))
            error("world file corrupt");

        for(unsigned j = 0; j < n_objects; ++j)
        {
            struct object_t *obj = obj_read(fd);

            if(!room_obj_add(i, obj))
                error("duplicate object name in room '%s'", world[i].data.name);
        }
    }

    close(fd);
    return true;
}

static SIMP_HASH(enum direction_t, dir_hash);
static SIMP_EQUAL(enum direction_t, dir_equal);

/* loads room data (supplied by the world module) into our internal format */
void world_init(const struct roomdata_t *data, size_t sz, const char *name)
{
    debugf("Loading world with %lu rooms.\n", sz);
    world = calloc(sz, sizeof(struct room_t));
    world_sz = 0;
    world_name = strdup(name);

    void *map = hash_init(sz / 2 + 1, hash_djb, compare_strings);

    for(size_t i = 0; i < sz; ++i)
    {
        world[i].id = i;
        memcpy(&world[i].data, &data[i], sizeof(data[i]));

        /* have to strdup these strings so they can be freed later */
        //world[i].uniq_id = strdup(world[i].uniq_id);
        world[i].data.name = strdup(world[i].data.name);
        world[i].data.desc = strdup(world[i].data.desc);
        debugf("Loading room '%s'\n", world[i].data.uniq_id);

        if(hash_insert(map, world[i].data.uniq_id, world + i))
            error("Duplicate room ID '%s'", world[i].data.uniq_id);

        for(int dir = 0; dir < NUM_DIRECTIONS; ++dir)
        {
            const char *adjacent_room = world[i].data.adjacent[dir];
            if(adjacent_room)
            {
                struct room_t *room = hash_lookup(map, adjacent_room);
                if(room)
                    world[i].adjacent[dir] = room->id;
            }
        }
        room_init_maps(world + i);

        world_sz = i + 1;
    }

    /* second pass to fill in missing references */
    for(size_t i = 0; i < sz; ++i)
    {
        for(int dir = 0; dir < NUM_DIRECTIONS; ++dir)
        {
            const char *adjacent_room = world[i].data.adjacent[dir];
            if(adjacent_room)
            {
                struct room_t *room = hash_lookup(map, adjacent_room);
                if(room)
                    world[i].adjacent[dir] = room->id;
                else
                    error("unknown room '%s' referenced from '%s'",
                          adjacent_room, world[i].data.uniq_id);
            }
            else
                world[i].adjacent[dir] = ROOM_NONE;
        }
    }

    struct direction_pair {
        enum direction_t dir, opp;
    } pairs[] = {
        { DIR_N,  DIR_S  },
        { DIR_NE, DIR_SW },
        { DIR_E,  DIR_W  },
        { DIR_SE, DIR_NW },
        { DIR_UP, DIR_DN },
        { DIR_IN, DIR_OT },
    };

    void *dir_map = hash_init(ARRAYLEN(pairs) * 2, dir_hash, dir_equal);
    for(int n = 0; n < 2; ++n)
    {
        for(unsigned i = 0; i < ARRAYLEN(pairs); ++i)
        {
            if(!n)
                hash_insert(dir_map, &pairs[i].dir, &pairs[i].opp);
            else
                hash_insert(dir_map, &pairs[i].opp, &pairs[i].dir);
        }
    }

    /* third pass to call all the init handlers and check accessibility */
    for(room_id i = 0; i < (int)world_sz; ++i)
    {
        if(world[i].data.hook_init)
            world[i].data.hook_init(world[i].id);
        /* check that all rooms are accessible */
        for(enum direction_t j = 0; j < NUM_DIRECTIONS; ++j)
        {
            if(world[i].adjacent[j] != ROOM_NONE)
            {
                enum direction_t *opp = hash_lookup(dir_map, &j);
                struct room_t *adj = room_get(world[i].adjacent[j]);
                if(adj->adjacent[*opp] != i)
                    debugf("WARNING: Rooms '%s' and '%s' are one-way\n",
                           world[i].data.uniq_id, adj->data.uniq_id);
            }
        }
    }

    hash_free(dir_map);

    hash_free(map);
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
