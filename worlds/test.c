#include <world_api.h>

/************ ROOM DEFINITIONS ************/

static void deadend_init(room_id id)
{
    struct object_t *new = obj_new("/generic");
    new->name = strdup("shovel");
    new->userdata = strdup("It is a normal shovel with a price tag attached that says $19.99.");
    new->list = true;
    room_obj_add(id, new);
    new = obj_copy(new);
    room_obj_add(id, new);
}

static void ew_road_init(room_id id)
{
    struct object_t *new = obj_new("/generic/notake");
    new->name = strdup("large boulder");
    new->userdata = strdup("It is just a boulder.  It cannot be moved.");
    new->list = true;
    room_obj_add(id, new);
}

static void fork_init(room_id id)
{
    struct verb_t *new = verb_new("dig");
    new->name = strdup("dig");
    room_verb_add(id, new);

    room_get(id)->userdata = calloc(1, sizeof(bool));
    /* flag for whether the user has already dug */
    bool *b = room_get(id)->userdata;
    *b = false;
}

static void fork_ser(room_id id, int fd)
{
    bool *b = room_get(id)->userdata;
    write_bool(fd, *b);
}

static void fork_deser(room_id id, int fd)
{
    bool *b = calloc(1, sizeof(bool));
    *b = read_bool(fd);
    room_get(id)->userdata = b;
}

static void fork_destroy(room_id id)
{
    free(room_get(id)->userdata);
}

static void senw_init(room_id id)
{
    struct object_t *new = obj_new("/generic");
    new->name = strdup("some food");
    new->userdata = strdup("It looks like some kind of meat.  Smells pretty bad.");
    new->default_article = false;
    room_obj_add(id, new);
    room_obj_add_alias(id, new, strdup("food"));
}

const struct roomdata_t netcosm_world[] = {
    {
        "dead_end",
        "Dead End",
        "You are at a dead end of a dirt road.  The road goes to the east. In the distance you can see that it will eventually fork off. The trees here are very tall royal palms, and they are spaced equidistant from each other.",
        { NONE_N, NONE_NE, "ew_road", NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        deadend_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "ew_road",
        "E/W Dirt road",
        "You are on the continuation of a dirt road. There are more trees on both sides of you. The road continues to the east and west.",
        { NONE_N, NONE_NE, "fork", NONE_SE, NONE_S, NONE_SW, "dead_end", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        ew_road_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "fork",
        "Fork",
        "You are at a fork of two passages, one to the northeast, and one to the southeast. The ground here seems very soft. You can also go back west.",
        { NONE_N, "nesw_road", NONE_E, "senw_road", NONE_S, NONE_SW, "ew_road", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        fork_init,
        NULL,
        NULL,
        fork_ser,
        fork_deser,
        fork_destroy,
    },

    {
        "senw_road",
        "SE/NW road",
        "You are on a southeast/northwest road.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, NONE_W, "fork", NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        senw_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "nesw_road",
        "NE/SW road",
        "You are on a northeast/southwest road.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, "fork", NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

const size_t netcosm_world_sz = ARRAYLEN(netcosm_world);
const char *netcosm_world_name = "Dunnet 0.1";

/************ OBJECT DEFINITIONS ************/

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

static bool no_take(struct object_t *obj, user_t *user)
{
    (void) obj; (void) user;
    return false;
}

const struct obj_class_t netcosm_obj_classes[] = {
    /* a generic, takable object class with userdata as its description */
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

    /* a generic, non-takable object class */
    {
        "/generic/notake",
        generic_ser,
        generic_deser,
        no_take,
        NULL,
        generic_destroy,
        generic_desc,
        generic_dup,
    }
};

const size_t netcosm_obj_classes_sz = ARRAYLEN(netcosm_obj_classes);

/**************** VERB DEFINITIONS ****************/

static void dig_exec(struct verb_t *verb, char *args, user_t *user)
{
    (void) verb;
    (void) args;
    if(!multimap_lookup(userdb_lookup(user->user)->objects, "shovel", NULL))
    {
        send_msg(user, "You have nothing with which to dig.\n");
        return;
    }

    if(!strcmp(room_get(user->room)->data.name, "Fork"))
    {
        bool *b = room_get(user->room)->userdata;
        if(!*b)
        {
            *b = true;
            struct object_t *new = obj_new("/generic");
            new->name = strdup("CPU card");
            new->userdata = strdup("The CPU board has a VAX chip on it.  It seems to have 2 Megabytes of RAM onboard.");
            new->list = true;
            room_obj_add(user->room, new);
            send_msg(user, "I think you found something.\n");
        }
        else
        {
            goto nothing;
        }
    }

    return;

nothing:
    send_msg(user, "Digging here reveals nothing.\n");
}

const struct verb_class_t netcosm_verb_classes[] = {
    { "dig",
      dig_exec },
    /*
    { "shake",
      shake_exec },
    { "climb",
      climb_exec },
    { "put",
      put_exec },
    { "eat",
      eat_exec },
    { "feed",
      feed_exec },
    */
};

const size_t netcosm_verb_classes_sz = ARRAYLEN(netcosm_verb_classes);
