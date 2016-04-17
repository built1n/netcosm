/* generates a 3d world with no content */

#include <globals.h>
#include <hash.h>
#include <room.h>

/* x and y are horizontal and forward-back axes, respectively */
/* z is the vertical axis */

struct direction_info_t {
    enum direction_t dir;
    int off_x, off_y, off_z;
} dirs[] = {
    { DIR_N,  0,  1,   0  },
    { DIR_NE, 1,  1,   0  },
    { DIR_E,  1,  0,   0  },
    { DIR_SE, 1,  -1,  0  },
    { DIR_S,  0,  -1,  0  },
    { DIR_SW, -1, -1,  0  },
    { DIR_W,  -1,  0,  0  },
    { DIR_NW, -1,  1,  0  },
    { DIR_UP, 0,   0,  1  },
    { DIR_DN, 0,   0,  -1 },
};

int main()
{
    printf("#include <world_api.h>\n");
    printf("const struct roomdata_t netcosm_world[] = {\n");

    for(int x = 0; x < WORLD_DIM; ++x)
        for(int y = 0; y < WORLD_DIM; ++y)
            for(int z = 0; z < WORLD_DIM; ++z)
            {
                printf("{\n");
                printf("\"room_%d_%d_%d\",\n", x, y, z);
                printf("\"Room (%d,%d,%d)\",\n", x, y, z);
                printf("\"You are in a room...\",");

                char *adj[ARRAYLEN(dirs)];
                for(int i = 0; i < ARRAYLEN(dirs); ++i)
                {
                    int new_x = x + dirs[i].off_x,
                        new_y = y + dirs[i].off_y,
                        new_z = z + dirs[i].off_z;

                    if(new_x < 0 || new_x >= WORLD_DIM ||
                       new_y < 0 || new_y >= WORLD_DIM ||
                       new_z < 0 || new_z >= WORLD_DIM)
                        asprintf(adj + i, "NULL");
                    else
                        asprintf(adj + i, "\"room_%d_%d_%d\"",
                                 new_x,
                                 new_y,
                                 new_z);
                }

                printf("{ ");
                for(int i = 0; i < ARRAYLEN(dirs); ++i)
                {
                    printf("%s, ", adj[i]);
                    free(adj[i]);
                }
                printf("NONE_IN, NONE_OT },\n");

                for(int i = 0; i < 6; ++i)
                    printf("NULL,\n");

                printf("},\n");
            }

    printf("};\n");
    printf("const size_t netcosm_world_sz = ARRAYLEN(netcosm_world);\n");
    printf("const char *netcosm_world_name = \"World Name Here\";\n");

    printf("static void generic_ser(int fd, struct object_t *obj)\n");
    printf("{\n");
    printf("    write_string(fd, obj->userdata);\n");
    printf("}\n");

    printf("static void generic_deser(int fd, struct object_t *obj)\n");
    printf("{\n");
    printf("    obj->userdata = read_string(fd);\n");
    printf("}\n");

    printf("static void generic_destroy(struct object_t *obj)\n");
    printf("{\n");
    printf("    free(obj->userdata);\n");
    printf("}\n");

    printf("static const char *generic_desc(struct object_t *obj, user_t *user)\n");
    printf("{\n");
    printf("    (void) user;\n");
    printf("    return obj->userdata;\n");
    printf("}\n");

    printf("static void *generic_dup(struct object_t *obj)\n");
    printf("{\n");
    printf("    return strdup(obj->userdata);\n");
    printf("}\n");

    printf("const struct obj_class_t netcosm_obj_classes[] = {\n");
    printf("    {\n");
    printf("        \"/generic\",\n");
    printf("        generic_ser,\n");
    printf("        generic_deser,\n");
    printf("        NULL,\n");
    printf("        NULL,\n");
    printf("        generic_destroy,\n");
    printf("        generic_desc,\n");
    printf("        generic_dup,\n");
    printf("    },\n");
    printf("};\n");
    printf("const size_t netcosm_obj_classes_sz = ARRAYLEN(netcosm_obj_classes);\n");

    printf("const struct verb_class_t netcosm_verb_classes[] = {\n");
    printf("\n");
    printf("};\n");
    printf("\n");
    printf("const size_t netcosm_verb_classes_sz = ARRAYLEN(netcosm_verb_classes);\n");

};
