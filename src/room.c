/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2015 Franklin Wei
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

#include "netcosm.h"

/* processed world data */
static struct room_t *world;
static size_t world_sz;

struct room_t *room_get(room_id id)
{
    return world + id;
}

bool room_user_add(room_id id, struct child_data *child)
{
    struct room_t *room = room_get(id);
    /* make sure there are no duplicates */

    struct user_t *iter = room->users, *last = NULL;
    while(iter)
    {
        if(iter->data->pid == child->pid)
            return false;
        last = iter;
        iter = iter->next;
    }

    struct user_t *new = calloc(sizeof(struct user_t), 1);

    new->data = child;
    new->next = NULL;

    if(last)
        last->next = new;
    else
        room->users = new;
    return true;
}

bool room_user_del(room_id id, struct child_data *child)
{
    struct room_t *room = room_get(id);

    struct user_t *iter = room->users, *last = NULL;
    while(iter)
    {
        if(iter->data->pid == child->pid)
        {
            if(last)
                last->next = iter->next;
            else
                room->users = iter->next;
            free(iter);
            return true;
        }
        last = iter;
        iter = iter->next;
    }
    return false;
}

void write_roomid(int fd, room_id *id)
{
    write(fd, id, sizeof(*id));
}

void write_string(int fd, const char *str)
{
    size_t len = strlen(str);
    write(fd, &len, sizeof(len));
    write(fd, str, len);
}

room_id read_roomid(int fd)
{
    room_id ret;
    if(read(fd, &ret, sizeof(ret)) < 0)
        return ROOM_NONE;
    return ret;
}

char *read_string(int fd)
{
    size_t sz;
    read(fd, &sz, sizeof(sz));
    char *ret = malloc(sz + 1);
    if(read(fd, ret, sz) < 0)
        return NULL;
    ret[sz] = '\0';
    return ret;
}

void world_save(const char *fname)
{
    int fd = open(fname, O_CREAT | O_WRONLY, 0644);
    uint32_t magic = WORLD_MAGIC;
    write(fd, &magic, sizeof(magic));
    write(fd, &world_sz, sizeof(world_sz));
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
    }
    close(fd);
}

static void room_free(struct room_t *room)
{
    while(room->users)
    {
        struct user_t *old = room->users;
        room->users = room->users->next;
        free(old);
    }
    free(room->data.name);
    free(room->data.desc);
}

void world_free(void)
{
    if(world)
    {
        for(unsigned i = 0; i < world_sz; ++i)
        {
            room_free(world + i);
        }
        free(world);
        world = NULL;
    }
}

bool world_load(const char *fname, const struct roomdata_t *data, size_t data_sz)
{
    int fd = open(fname, O_RDONLY);
    if(fd < 0)
        return false;
    uint32_t magic;
    read(fd, &magic, sizeof(magic));
    if(magic != WORLD_MAGIC)
        return false;
    read(fd, &world_sz, sizeof(world_sz));

    if(world)
        world_free();

    if(world_sz != data_sz)
        return false;

    world = calloc(world_sz, sizeof(struct room_t));

    for(unsigned i = 0; i < world_sz; ++i)
    {
        world[i].id = read_roomid(fd);
        memcpy(&world[i].data, data + i, sizeof(struct roomdata_t));
        world[i].data.name = read_string(fd);
        world[i].data.desc = read_string(fd);
        if(read(fd, world[i].adjacent, sizeof(world[i].adjacent)) < 0)
            return false;
    }

    close(fd);
    return true;
}

static SIMP_HASH(enum direction_t, dir_hash);
static SIMP_EQUAL(enum direction_t, dir_equal);

/* loads room data (supplied by the world module) into our internal format */
void world_init(const struct roomdata_t *data, size_t sz)
{
    printf("Loading world with %lu rooms.\n", sz);
    world = calloc(sz, sizeof(struct room_t));
    world_sz = 0;

    void *map = hash_init(1, hash_djb, compare_strings);

    for(size_t i = 0; i < sz; ++i)
    {
        world[i].id = i;
        memcpy(&world[i].data, &data[i], sizeof(data[i]));

        /* have to strdup these strings so they can be freed later */
        //world[i].uniq_id = strdup(world[i].uniq_id);
        world[i].data.name = strdup(world[i].data.name);
        world[i].data.desc = strdup(world[i].data.desc);
        printf("Loading room '%s'\n", world[i].data.uniq_id);

        if(hash_insert(map, world[i].data.uniq_id, world + i))
            error("Duplicate room ID '%s'\n", world[i].data.uniq_id);

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
                    printf("WARNING: Rooms '%s' and '%s' are one-way\n",
                           world[i].data.uniq_id, adj->data.uniq_id);
            }
        }
    }

    hash_free(dir_map);

    hash_free(map);
}
