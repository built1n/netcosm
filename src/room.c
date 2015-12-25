#include "netcosm.h"

static struct room_t *world;
static size_t world_sz;

static struct room_t *room_linear_search(const char *id)
{
    printf("Looking for room with id '%s'\n", id);
    for(size_t i = 0; i < world_sz; ++i)
        if(!strcmp(world[i].data.uniq_id, id))
            return world + i;
        else
            printf("Iterating over room '%s'\n", world[i].data.uniq_id);
    return NULL;
}

struct room_t *room_get(room_id id)
{
    return world + id;
}

void world_free(void)
{
    free(world);
}

/* loads room data (supplied by the world module) into our internal format */
void world_init(const struct roomdata_t *data, size_t sz)
{
    printf("Loading world with %lu rooms.\n", sz);
    world = calloc(sz, sizeof(struct room_t));
    world_sz = 0;

    void *map = hash_init(1, hash_djb, strcmp);

    for(size_t i = 0; i < sz; ++i)
    {
        world[i].id = i;
        memcpy(&world[i].data, &data[i], sizeof(data[i]));
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
                struct room_t *room = room_linear_search(adjacent_room);
                if(room)
                    world[i].adjacent[dir] = room->id;
                else
                    error("unknown room '%s' referenced from '%s'", adjacent_room, world[i].data.uniq_id);
            }
            else
                world[i].adjacent[dir] = ROOM_NONE;
        }
    }

    /* third pass to call all the init handlers */
    for(size_t i = 0; i < world_sz; ++i)
        if(world[i].data.hook_init)
            world[i].data.hook_init(world[i].id);

    hash_free(map);
}
