#include <world_api.h>

const struct roomdata_t netcosm_world[] = {
    {
        "starting_room",
        "Starting Room",
        "You are in the starting room.\nThere are exits to the west and the east.",
        { NONE_N, NONE_NE, "east_room", NONE_SE, NONE_S, NONE_SW, "west_room", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
        NULL
    },

    {
        "west_room",
        "West Room",
        "You are in the west room.\nThere is an exit to the east.",
        { NONE_N, NONE_NE, "starting_room", NONE_SE, NONE_S, NONE_SW, NONE_W, NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
        NULL
    },

    {
        "east_room",
        "East Room",
        "You are in the east room.\nThere is an exit to the west.",
        { NONE_N, NONE_NE, NONE_E, NONE_SE, NONE_S, NONE_SW, "starting_room", NONE_NW, NONE_UP, NONE_DN, NONE_IN, NONE_OT },
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

const size_t netcosm_world_sz = ARRAYLEN(netcosm_world);
const char *netcosm_world_name = "Test World 1.1";
