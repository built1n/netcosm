#include <world_api.h>

/* implements dunnet in NetCosm */

/************ ROOM DEFINITIONS ************/

static void deadend_init(room_id id)
{
    struct object_t *new = obj_new("/generic");
    new->name = strdup("shovel");
    new->userdata = strdup("It is a normal shovel with a price tag attached that says $19.99.");

    room_obj_add(id, new);

    new = obj_new("/generic/notake");
    new->name = strdup("trees");
    new->userdata = strdup("They are palm trees with a bountiful supply of coconuts in them.");
    new->hidden = true;

    room_obj_add(id, new);
    room_obj_add_alias(id, new, "tree");
    room_obj_add_alias(id, new, "palm");
    room_obj_add_alias(id, new, "palm tree");

    /* add global verbs */
    struct verb_t *verb = verb_new("dig");
    verb->name = strdup("dig");
    world_verb_add(verb);

    verb = verb_new("put");
    verb->name = strdup("put");
    world_verb_add(verb);

    verb = verb_new("eat");
    verb->name = strdup("eat");
    world_verb_add(verb);

    verb = verb_new("shake");
    verb->name = strdup("shake");
    world_verb_add(verb);

    verb = verb_new("type");
    verb->name = strdup("type");
    world_verb_add(verb);
}

static void ew_road_init(room_id id)
{
    struct object_t *new = obj_new("/generic/notake");
    new->name = strdup("large boulder");
    new->userdata = strdup("It is just a boulder.  It cannot be moved.");
    room_obj_add(id, new);
    room_obj_add_alias(id, new, "boulder");
    room_obj_add_alias(id, new, "rock");
}

static void fork_init(room_id id)
{
    room_get(id)->userdata = calloc(1, sizeof(bool));
    /* flag for whether the user has already dug */
    bool *b = room_get(id)->userdata;
    *b = false;
}

static void bool_ser(room_id id, int fd)
{
    bool *b = room_get(id)->userdata;
    write_bool(fd, *b);
}

static void bool_deser(room_id id, int fd)
{
    bool *b = calloc(1, sizeof(bool));
    *b = read_bool(fd);
    room_get(id)->userdata = b;
}

static void bool_destroy(room_id id)
{
    free(room_get(id)->userdata);
}

static void senw_init(room_id id)
{
    struct object_t *new = obj_new("/generic/dunnet/food");
    new->name = strdup("some food");
    new->userdata = strdup("It looks like some kind of meat.  Smells pretty bad.");
    new->default_article = false;
    room_obj_add(id, new);
    room_obj_add_alias(id, new, "food");
    room_obj_add_alias(id, new, "meat");
}

static void hangout_init(room_id id)
{
    struct object_t *new = obj_new("/generic/notake");
    new->name = strdup("ferocious bear");
    new->userdata = strdup("It looks like a grizzly to me.");
    room_obj_add(id, new);
    room_obj_add_alias(id, new, "bear");
}

static void hidden_init(room_id id)
{
    struct object_t *new = obj_new("/generic");
    new->name = strdup("emerald bracelet");
    new->userdata = strdup("I see nothing special about that.");
    room_obj_add(id, new);
    room_obj_add_alias(id, new, "bracelet");
}

static bool building_enter(room_id id, user_t *user)
{
    (void) id;
    if(multimap_lookup(userdb_lookup(user->user)->objects, "shiny brass key", NULL))
        return true;
    else
    {
        send_msg(user, "You don't have a key that can open this door.\n");
        return false;
    }
}

static void mailroom_init(room_id id)
{
    struct object_t *new = obj_new("/generic/notake");
    new->name = strdup("bins");
    new->hidden = true;

    /* insert IAC NOP to prevent the extra whitespace from being dropped */
    new->userdata = strdup("All of the bins are empty.  Looking closely you can see that there are names written at the bottom of each bin, but most of them are faded away so that you cannot read them.  You can only make out three names:\n\377\361                   Jeffrey Collier\n\377\361                   Robert Toukmond\n\377\361                   Thomas Stock\n");
    room_obj_add(id, new);
    room_obj_add_alias(id, new, "mail bins");
}

static void computer_room_init(room_id id)
{
    struct object_t *new = obj_new("/generic/notake");
    new->name = strdup("computer");
    new->userdata = strdup("I see nothing special about that.");
    new->hidden = true;

    room_obj_add(id, new);
    room_obj_add_alias(id, new, "vax");
    room_obj_add_alias(id, new, "pokey");

    /* flag for whether computer is active */
    room_get(id)->userdata = malloc(sizeof(bool));
    bool *b = room_get(id)->userdata;
    *b = false;
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
        bool_ser,
        bool_deser,
        bool_destroy,
    },

    {
        "senw_road",
        "SE/NW road",
        "You are on a southeast/northwest road.",
        { NONE_N, NONE_NE, NONE_E, "bear_hangout", NONE_S, NONE_SW, NONE_W, "fork", NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        senw_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "bear_hangout",
        "Bear Hangout",
        "You are standing at the end of a road. A passage leads back to the northwest.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, "hidden_area", NONE_W, "senw_road", NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        hangout_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "hidden_area",
        "Hidden Area",
        "You are in a well-hidden area off to the side of a road.  Back to the northeast through the brush you can see the bear hangout.",
        { NONE_N, "bear_hangout", NONE_E, NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        hidden_init,
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
        { NONE_N, "building_front", NONE_E, NONE_SE, NONE_S, "fork", NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "building_front",
        "Building Front",
        "You are at the end of the road.  There is a building in front of you to the northeast, and the road leads back to the southwest.",
        { NONE_N, "building_hallway", NONE_E, NONE_SE, NONE_S, "nesw_road", NONE_W, NONE_NW, NONE_UP, NONE_DN, "building_hallway", NONE_OT },
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "building_hallway",
        "Old Building hallway",
        "You are in the hallway of an old building.  There are rooms to the east and west, and doors leading out to the north and south.",
        { NONE_N, NONE_NE, "mailroom", NONE_SE, "building_front", NONE_SW, "computer_room", NONE_NW, NONE_UP, NONE_DN, NONE_IN, "building_front" },
        NULL,
        building_enter,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "mailroom",
        "Mailroom",
        "You are in a mailroom.  There are many bins where the mail is usually kept.  The exit is to the west.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, "building_hallway", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        mailroom_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },

    {
        "computer_room",
        "Computer room",
        "You are in a computer room.  It seems like most of the equipment has been removed.  There is a VAX 11/780 in front of you, however, with one of the cabinets wide open.  A sign on the front of the machine says: This VAX is named 'pokey'.  To type on the console, use the 'type' command.  The exit is to the east.\nThe panel lights are steady and motionless.",
        { NONE_N, NONE_NE, "building_hallway", NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        computer_room_init,
        NULL,
        NULL,
        bool_ser,
        bool_deser,
        bool_destroy,
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

static bool food_drop(struct object_t *obj, user_t *user)
{
    if(room_obj_get(user->room, "bear"))
    {
        send_msg(user, "The bear takes the food and runs away with it. He left something behind.\n");

        room_obj_del(user->room, "ferocious bear");
        room_obj_del_by_ptr(user->room, obj);

        struct object_t *new = obj_new("/generic");
        new->hidden = false;

        new->name = strdup("shiny brass key");
        new->userdata = strdup("I see nothing special about that.");

        room_obj_add(user->room, new);
        room_obj_add_alias(user->room, new, "key");
        room_obj_add_alias(user->room, new, "shiny key");
        room_obj_add_alias(user->room, new, "brass key");
    }

    return true;
}

const struct obj_class_t netcosm_obj_classes[] = {
    /* a generic, takeable object class with userdata pointing to its description */
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

    /* a generic, non-takeable object class, inherits /generic */
    {
        "/generic/notake",
        generic_ser,
        generic_deser,
        no_take,
        NULL,
        generic_destroy,
        generic_desc,
        generic_dup,
    },

    /* a specialized "food" object for dunnet, inherits /generic */
    {
        "/generic/dunnet/food",
        generic_ser,
        generic_deser,
        NULL,
        food_drop,
        generic_destroy,
        generic_desc,
        generic_dup,
    },
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
            room_obj_add(user->room, new);
            room_obj_add_alias(user->room, new, "cpu");
            room_obj_add_alias(user->room, new, "chip");
            room_obj_add_alias(user->room, new, "card");
            send_msg(user, "I think you found something.\n");
        }
        else
        {
            goto nothing;
        }
    }
    else
        send_msg(user, "Digging here reveals nothing.\n");

    return;

nothing:
    send_msg(user, "Digging here reveals nothing.\n");
}

static void put_exec(struct verb_t *verb, char *args, user_t *user)
{
    (void) verb;
    char *save;
    const char *obj_name = strtok_r(args, WSPACE, &save);

    if(!obj_name)
    {
        send_msg(user, "You must supply an object\n");
        return;
    }

    args = NULL;
    const struct multimap_list *list = multimap_lookup(userdb_lookup(user->user)->objects,
                                                       obj_name, NULL);
    if(!list)
    {
        send_msg(user, "You don't have that.\n");
        return;
    }

    struct object_t *obj = list->val;

    /* original dunnet ignores the preposition */
    const char *prep = strtok_r(args, WSPACE, &save);
    (void) prep;

    const char *ind_obj_name = strtok_r(args, WSPACE, &save);

    if(!ind_obj_name)
    {
        send_msg(user, "You must supply an indirect object.\n");
        return;
    }

    list = room_obj_get(user->room, ind_obj_name);

    if(!list)
    {
        send_msg(user, "I don't know what that indirect object is.\n");
        return;
    }

    struct object_t *ind_obj = list->val;

    /* now execute the verb */
    if(!strcmp(obj->name, "CPU card") && !strcmp(ind_obj->name, "computer") && user->room == room_get_id("computer_room"))
    {
        userdb_del_obj_by_ptr(user->user, obj);
        send_msg(user, "As you put the CPU board in the computer, it immediately springs to life.  The lights start flashing, and the fans seem to startup.\n");
        bool *b = room_get(user->room)->userdata;
        *b = true;

        free(room_get(user->room)->data.desc);
        room_get(user->room)->data.desc = strdup("You are in a computer room.  It seems like most of the equipment has been removed.  There is a VAX 11/780 in front of you, however, with one of the cabinets wide open.  A sign on the front of the machine says: This VAX is named 'pokey'.  To type on the console, use the 'type' command.  The exit is to the east.\nThe panel lights are flashing in a seemingly organized pattern.");
    }
    else
    {
        send_msg(user, "I don't know how to combine those objects.  Perhaps you should just try dropping it.\n");
    }
}

static void eat_exec(struct verb_t *verb, char *args, user_t *user)
{
    (void) verb;
    char *save;
    char *obj_name = strtok_r(args, WSPACE, &save);
    if(!obj_name)
    {
        send_msg(user, "You must supply an object.\n");
        return;
    }

    size_t n_objs;
    const struct multimap_list *list = multimap_lookup(userdb_lookup(user->user)->objects, obj_name, &n_objs);

    if(!list)
    {
        if(!room_obj_get(user->room, obj_name))
            send_msg(user, "I don't know what that is.\n");
        else
            send_msg(user, "You don't have that.\n");
        return;
    }

    struct object_t *obj = list->val;

    if(!strcmp(obj->name, "some food"))
    {
        send_msg(user, "That tasted horrible.\n");
    }
    else
    {
        char buf[MSG_MAX];
        send_msg(user, "You forcibly shove %s down your throat, and start choking.\n",
                 format_noun(buf, sizeof(buf), obj->name, n_objs, obj->default_article, false));

        /* TODO: kill player */
    }

    userdb_del_obj(user->user, obj_name);
}

static void shake_exec(struct verb_t *verb, char *args, user_t *user)
{
    (void) verb;
    char *save;
    char *obj_name = strtok_r(args, WSPACE, &save);

    if(!obj_name)
    {
        send_msg(user, "You must supply an object.\n");
        return;
    }

    size_t n_objs_room, n_objs_inv;
    const struct multimap_list *list_room = room_obj_get_size(user->room, obj_name, &n_objs_room);

    const struct multimap_list *list_inv = multimap_lookup(userdb_lookup(user->user)->objects, obj_name, &n_objs_inv);

    if(!list_room && !list_inv)
    {
        send_msg(user, "I don't know what that is.\n");
        return;
    }

    if(list_room)
    {
        struct object_t *obj = list_room->val;
        if(!strcmp(obj->name, "trees"))
            send_msg(user, "You begin to shake a tree, and notice a coconut begin to fall from the air.  As you try to get your hand up to block it, you feel the impact as it lands on your head.\n");
        else
            send_msg(user, "You don't have that.\n");
    }
    else if(list_inv)
    {
        struct object_t *obj = list_inv->val;
        char buf[MSG_MAX];
        send_msg(user, "Shaking %s seems to have no effect.\n",
                 format_noun(buf, sizeof(buf), obj->name,
                             n_objs_inv, obj->default_article,
                             false));
    }
}

static void type_exec(struct verb_t *verb, char *args, user_t *user)
{
    (void) verb;
    (void) args;

    struct room_t *room = room_get(user->room);
    if(strcmp(room->data.uniq_id, "computer_room"))
        send_msg(user, "There is nothing here on which you could type.\n");
    else
    {
        bool *b = room->userdata;

        /* computer is not active */
        if(!*b)
        {
            send_msg(user, "You type on the keyboard, but your characters do not even echo.\n");
            return;
        }
        else
        {
            send_msg(user, "FIXME\n");
        }
    }
}

/* verb classes */

const struct verb_class_t netcosm_verb_classes[] = {
    { "dig",
      dig_exec },
    { "put",
      put_exec },
    { "eat",
      eat_exec },
    { "shake",
      shake_exec },
    { "type",
      type_exec },
    /*
    { "climb",
      climb_exec },
      { "feed",
      feed_exec },
    */
};

const size_t netcosm_verb_classes_sz = ARRAYLEN(netcosm_verb_classes);

/* simulation callback */
void netcosm_world_simulation_cb(void)
{
    /* do nothing */
    //printf("callback\n");
    return;
}

/* 100 ms */
unsigned netcosm_world_simulation_interval = 100;
