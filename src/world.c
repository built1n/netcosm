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
#include "room.h"
#include "world.h"

/* verb classes */
const struct verb_class_t *netcosm_verb_classes;
size_t netcosm_verb_classes_sz;

/* object classes */
const struct obj_class_t *netcosm_obj_classes;
size_t netcosm_obj_classes_sz;

/* rooms */
const struct roomdata_t *netcosm_world;
size_t netcosm_world_sz;

/* simulation callback */
void (*netcosm_world_simulation_cb)(void) = NULL;
unsigned netcosm_world_simulation_interval = 0;

const char *netcosm_world_name;

/* processed world data */

static struct room_t *world;
static size_t world_sz;
static char *world_name;

/* map of room names -> rooms */
static void *world_map = NULL;

struct room_t *room_get(room_id id)
{
    return world + id;
}

void world_save(const char *fname)
{
    int fd = open(fname, O_CREAT | O_WRONLY, 0644);

    write_uint32(fd, WORLD_MAGIC);

    write(fd, &world_sz, sizeof(world_sz));
    write_string(fd, world_name);

    /* write all the global verbs */
    void *global_verbs = world_verb_map();
    size_t n_global_verbs = hash_size(global_verbs);
    write(fd, &n_global_verbs, sizeof(n_global_verbs));

    if(n_global_verbs)
    {
        void *save;

        while(1)
        {
            struct verb_t *verb = hash_iterate(global_verbs, &save, NULL);
            if(!verb)
                break;
            global_verbs = NULL;
            verb_write(fd, verb);
        }
    }

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

        size_t n_objects = room_obj_count_noalias(i);
        write(fd, &n_objects, sizeof(n_objects));

        room_id id = i;
        void *save;
        while(1)
        {
            const struct multimap_list *iter = room_obj_iterate(id, &save, NULL);
            if(!iter)
                break;
            id = ROOM_NONE;
            while(iter)
            {
                struct object_t *obj = iter->val;
                if(!obj)
                    break;
                const char *name = iter->key;
                if(!strcmp(name, obj->name))
                {
                    obj_write(fd, obj);
                }
                iter = iter->next;
            }
        }

        /* and now all the verbs... */

        void *verb_map = room_verb_map(i);
        size_t n_verbs = hash_size(verb_map);
        write_size(fd, n_verbs);
        while(1)
        {
            struct verb_t *verb = hash_iterate(verb_map, &save, NULL);
            if(!verb)
                break;
            verb_map = NULL;
            verb_write(fd, verb);
        }

        /* and now user data... */
        if(world[i].data.hook_serialize)
            world[i].data.hook_serialize(i, fd);
    }

    /* write the object counter so future objects will have sequential ids */
    write_uint64(fd, obj_get_idcounter());

    /* now write the map of room names to ids */
    void *ptr = world_map, *save;
    while(1)
    {
        void *key;
        struct room_t *room = hash_iterate(ptr, &save, &key);
        if(!room)
            break;
        ptr = NULL;
        write_string(fd, key);
        write_roomid(fd, &room->id);
    }

    close(fd);
}

static ev_timer *sim_timer = NULL;

static void sim_cb(EV_P_ ev_timer *w, int revents)
{
    (void) EV_A;
    (void) w;
    (void) revents;
    netcosm_world_simulation_cb();
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

        hash_free(world_verb_map());
        hash_free(world_map);

        free(world);
        world = NULL;
    }
    if(sim_timer)
        free(sim_timer);
}

static void start_sim_callback(void)
{
    /* start callback */
    if(netcosm_world_simulation_cb && netcosm_world_simulation_interval)
    {
        sim_timer = calloc(1, sizeof(ev_timer));
        ev_timer_init(sim_timer, sim_cb, netcosm_world_simulation_interval/1000.0,
                      netcosm_world_simulation_interval/1000.0);
        ev_timer_start(EV_DEFAULT_ sim_timer);
    }
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

    if(read_uint32(fd) != WORLD_MAGIC)
        error("unknown world file format");

    if(world)
        world_free();

    read(fd, &world_sz, sizeof(world_sz));

    if(world_sz != data_sz)
    {
        debugf("Incompatible world state.\n");
        return false;
    }

    world = calloc(world_sz, sizeof(struct room_t));

    world_name = read_string(fd);
    if(strcmp(name, world_name))
    {
        debugf("Incompatible world state (%s %s).\n", name, world_name);
        free(world_name);
        return false;
    }

    debugf("Loading world `%s'.\n", world_name);

    obj_set_idcounter(1);

    size_t n_global_verbs = read_size(fd);
    for(unsigned i = 0; i < n_global_verbs; ++i)
    {
        struct verb_t *verb = verb_read(fd);
        if(!world_verb_add(verb))
            error("read duplicate global verb '%s'", verb->name);
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

            room_obj_add(i, obj);
        }

        /* read room-local verbs */
        size_t n_verbs = read_size(fd);
        for(unsigned j = 0; j < n_verbs; ++j)
        {
            struct verb_t *verb = verb_read(fd);
            if(!room_verb_add(i, verb))
                error("duplicate verb '%s' in room '%s'", verb->name,
                      world[i].data.name);
        }

        /* user data, if any */
        if(world[i].data.hook_deserialize)
            world[i].data.hook_deserialize(i, fd);
    }

    obj_set_idcounter(read_uint64(fd));

    /* read in the room name -> room map */

    world_map = hash_init(world_sz * 2, hash_djb, compare_strings);
    hash_setfreekey_cb(world_map, free);

    for(unsigned int i = 0; i < world_sz; ++i)
    {
        const char *key = read_string(fd);
        room_id id  = read_roomid(fd);
        hash_insert(world_map, key, world + id);
    }

    close(fd);

    start_sim_callback();

    return true;
}

static SIMP_HASH(enum direction_t, dir_hash);
static SIMP_EQUAL(enum direction_t, dir_equal);

/* loads room data (supplied by the world module) into our internal format */
void world_init(const struct roomdata_t *data, size_t sz, const char *name)
{
    debugf("Loading world with %zu rooms.\n", sz);
    world = calloc(sz, sizeof(struct room_t));
    world_sz = 0;
    world_name = strdup(name);

    world_map = hash_init(sz * 2, hash_djb, compare_strings);

    for(size_t i = 0; i < sz; ++i)
    {
        world[i].id = i;
        memcpy(&world[i].data, &data[i], sizeof(data[i]));

        /* have to strdup these strings so they can be freed later */
        //world[i].uniq_id = strdup(world[i].uniq_id);
        world[i].data.name = strdup(world[i].data.name);
        world[i].data.desc = strdup(world[i].data.desc);
        //debugf("Loading room '%s'\n", world[i].data.uniq_id);

        if(hash_insert(world_map, world[i].data.uniq_id, world + i))
            error("Duplicate room ID '%s'", world[i].data.uniq_id);

        for(int dir = 0; dir < NUM_DIRECTIONS; ++dir)
        {
            const char *adjacent_room = world[i].data.adjacent[dir];
            if(adjacent_room)
            {
                struct room_t *room = hash_lookup(world_map, adjacent_room);
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
                struct room_t *room = hash_lookup(world_map, adjacent_room);
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

    start_sim_callback();
}

static void *verb_map = NULL;

#define VERBMAP_SZ 32

static void init_map(void)
{
    if(!verb_map)
    {
        verb_map = hash_init(VERBMAP_SZ, hash_djb, compare_strings);
        hash_setfreedata_cb(verb_map, verb_free);
    }
}

bool world_verb_add(struct verb_t *verb)
{
    init_map();
    //debugf("Added global verb %s\n", verb->name);
    return !hash_insert(verb_map, verb->name, verb);
}

void *world_verb_map(void)
{
    init_map();
    return verb_map;
}

room_id room_get_id(const char *uniq_id)
{
    struct room_t *room = hash_lookup(world_map, uniq_id);
    if(!room)
        return ROOM_NONE;
    else
        return room->id;
}
