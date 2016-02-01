#include <world_api.h>

/************ ROOM DEFINITIONS ************/

static void portal_init(room_id id)
{
    debugf("portal room init.\n");
    struct object_t *new = obj_new("weapon");
    new->name = strdup("sword");
    new->list = true;
    new->userdata = malloc(sizeof(double));
    double p = 3.14159265358979323846L;
    memcpy(new->userdata, &p, sizeof(p));
    room_obj_add(id, new);
}

static bool no_take(struct object_t *obj, user_t *user)
{
    (void)obj; (void)user;
    return false;
}

static void road_sign_init(room_id id)
{
    struct object_t *new = obj_new("sign");
    new->name = strdup("sign");
    new->list = true;
    new->userdata = strdup("The sign reads,\n\nThe Great Road\n==============\nAlagart -- 35km");

    room_obj_add(id, new);

    struct verb_t *verb = verb_new("read");
    verb->name = strdup("read");
    room_verb_add(id, verb);
}

const struct roomdata_t netcosm_world[] = {
    {
        "portal_room",
        "Portal Room",
        "You stand in the middle of a stone room. In to the east lies a portal to the world. Above it, there is a sign that reads `Alacron, 238 A.B.A.`.",
        { NONE_N, NONE_NE, "world_start", NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, "world_start", NONE_OT },
        portal_init,
        NULL,
        NULL,
    },

    {
        "world_start",
        "Beride Town Square",
        "You are in the Beride town square. All around you there are people hurrying to get along. To the north stands a statue of the late King Ajax IV, and to the east is the Great Road. There are exits in all four directions.",
        { "beride_square_n_statue", NONE_NE, "great_road_1", NONE_SE, "beride_square_s", NONE_SW, "beride_square_w", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
    },

    {
        "beride_square_n_statue",
        "King Ajax IV Statue",
        "Your path is blocked by an enormous bronze statue. A plaque on the pedestal reads,\n\nKing Ajax IV\n\n182 - 238 A.B.A.\n\nTo the south is the Beride Town Square.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, "world_start", NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
    },

    {
        "great_road_1",
        "Great Road",
        "You are at the start of a long, winding east-west road through the plains. Directly to your west is Beride.",
        { NONE_N, NONE_NE, "great_road_2", NONE_SE, NONE_S, NONE_SW, "world_start", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        road_sign_init,
        NULL,
        NULL,
    },


    {
        "great_road_2",
        "Great Road",
        "You are on a long, winding east-west road through the plains. To your west you can make out a town.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, "great_road_1", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
    },

    {
        "beride_square_w",
        "Bottomless Pit",
        "You take a step onto what seems to be solid rock, but your foot unexplicably slips through it, leading you to lose your balance and slip into the bottomless abyss...",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
    },

    {
        "beride_square_s",
        "Bottomless Pit",
        "You take a step onto what seems to be solid rock, but your foot unexplicably slips through it, leading you to lose your balance and slip into the bottomless abyss...",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
    },
};

const size_t netcosm_world_sz = ARRAYLEN(netcosm_world);
const char *netcosm_world_name = "Alacron 0.1";

/************ OBJECT DEFINITIONS ************/

const char *shiny(struct object_t *obj, user_t *user)
{
    if(user->state == STATE_ADMIN)
    {
        static char buf[128];
        double *d = obj->userdata;
        snprintf(buf, sizeof(buf), "It has %f written on it.", *d);
        return buf;
    }
    else
        return "It's kinda shiny.";
}

static void weap_serialize(int fd, struct object_t *obj)
{
    if(obj->userdata)
        write(fd, obj->userdata, sizeof(double));
}

static void weap_deserialize(int fd, struct object_t *obj)
{
    obj->userdata = malloc(sizeof(double));
    read(fd, obj->userdata, sizeof(double));
}

static void weap_destroy(struct object_t *obj)
{
    free(obj->userdata);
    obj->userdata = NULL;
}

static void sign_write(int fd, struct object_t *obj)
{
    write_string(fd, obj->userdata);
}

static void sign_read(int fd, struct object_t *obj)
{
    obj->userdata = read_string(fd);
}

static void sign_free(struct object_t *obj)
{
    free(obj->userdata);
}

static const char *sign_desc(struct object_t *obj, user_t *user)
{
   (void) user;
    return obj->userdata;
}

const struct obj_class_t netcosm_obj_classes[] = {
    {
        "weapon",
        weap_serialize,
        weap_deserialize,
        NULL,
        NULL,
        weap_destroy,
        shiny
    },

    {
        "sign",
        sign_write, // serialize
        sign_read,  // deserialize
        no_take,    // take
        NULL,       // drop
        sign_free,  // destroy
        sign_desc
    },
};

const size_t netcosm_obj_classes_sz = ARRAYLEN(netcosm_obj_classes);

/**************** VERB DEFINITIONS ****************/

static void read_exec(struct verb_t *verb, char *args, user_t *user)
{
    (void) verb;
    char *save;
    if(!args)
    {
        send_msg(user, "Read what?\n");
        return;
    }
    char *what = strtok_r(args, " ", &save);
    struct object_t *obj = room_obj_get(user->room, what);
    if(obj)
    {
        if(!strcmp(obj->class->class_name, "sign"))
            send_msg(user, "%s\n", obj->class->hook_desc(obj, user));
        else
            send_msg(user, "You can't read that.\n");
    }
    else
        send_msg(user, "I don't know what that is.\n");
}

const struct verb_class_t netcosm_verb_classes[] = {
    { "read",
      read_exec },
};

const size_t netcosm_verb_classes_sz = ARRAYLEN(netcosm_verb_classes);
