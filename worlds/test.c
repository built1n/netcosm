#include <world_api.h>

const char *sword_desc(struct object_t *obj, user_t *user)
{
    return "It is very shiny.";
}

static void portal_init(room_id id)
{
    debugf("portal room init.\n");
    struct object_t *new = obj_new("weapon");
    new->name = "sword";
    obj_add(id, new);
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
        "You are in the Beride town square. All around you there are people hurrying to get along. To the north stands a statue of the late King Ajax IV. There are exits in all four directions.",
        { "beride_square_n_statue", NONE_NE, "beride_square_e", NONE_SE, "beride_square_s", NONE_SW, "beride_square_w", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
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
        "beride_square_e",
        "Bottomless Pit",
        "You take a step onto what seems to be solid rock, but your foot unexplicably slips through it, leading you to lose your balance and slip into the bottomless abyss...",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
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
        return "It's VERRRYYY SHIIINNNNYYYY!!!!";
    else
        return "It's kinda shiny.";
}

const struct obj_class_t netcosm_obj_classes[] = {
    { "weapon",
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      shiny },
};

const size_t netcosm_obj_classes_sz = ARRAYLEN(netcosm_obj_classes);
