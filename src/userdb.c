#include "netcosm.h"

static void *map = NULL;
static char *db_file = NULL;
static size_t entries = 0;

/*
 * the user DB is stored on disk as an ASCII database
 *
 * this is then loaded into variable-sized hash map at init
 * TODO: implement with B-tree
 */
void userdb_init(const char *file)
{
    db_file = strdup(file);
    entries = 0;

    /* FILE* for getline() */
    FILE *f = fopen(file, "r");
    map = hash_init(256, hash_djb, compare_strings);
    hash_setfreedata_cb(map, free);

    char *format;
    asprintf(&format, "%%%d[a-z0-9]:%%%d[A-Z]:%%%ds:%%d\n",
             MAX_NAME_LEN, SALT_LEN, AUTH_HASHLEN * 2);

    if(f)
    {
        while(1)
        {
            struct userdata_t *data = calloc(1, sizeof(*data));

            int ret = fscanf(f, format,
                             &data->username,
                             data->salt,
                             data->passhash,
                             &data->priv);

            if(ret != 4)
                break;

            hash_insert(map, data->username, data);
            ++entries;
        }

        fclose(f);
    }

    free(format);
}

void userdb_write(const char *file)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    void *save, *ptr = map;
    while(1)
    {
        struct userdata_t *user = hash_iterate(ptr, &save, NULL);
        ptr = NULL;
        if(!user)
            break;
        dprintf(fd, "%*s:%*s:%*s:%d\n",
                MAX_NAME_LEN, user->username,
                SALT_LEN, user->salt,
                AUTH_HASHLEN*2, user->passhash,
                user->priv);
    }
    close(fd);
}

struct userdata_t *userdb_lookup(const char *key)
{
    return hash_lookup(map, key);
}

bool userdb_remove(const char *key)
{
    if(hash_remove(map, key))
    {
        --entries;
        userdb_write(db_file);
        return true;
    }
    return false;
}

struct userdata_t *userdb_add(struct userdata_t *data)
{
    struct userdata_t *new = calloc(1, sizeof(*new)); /* only in C! */
    memcpy(new, data, sizeof(*new));
    strncpy(new->username, data->username, sizeof(new->username));

    struct userdata_t *ret;

    if((ret = hash_insert(map, new->username, new))) /* failure */
    {
        hash_remove(map, new->username);
        ret = hash_insert(map, new->username, new);
    }

    userdb_write(db_file);
    if(!ret)
        ++entries;

    return ret;
}

void userdb_shutdown(void)
{
    debugf("shutting down userdb with %d entries\n", entries);
    if(map)
    {
        hash_free(map);
        map = NULL;
    }
    if(db_file)
    {
        free(db_file);
        db_file = NULL;
    }
}

size_t userdb_size(void)
{
    return entries;
}

/* child request wrappers */

/* loop until we can return a user data structure */
struct userdata_t *userdb_request_lookup(const char *name)
{
    send_master(REQ_GETUSERDATA, name, strlen(name) + 1);
    if(reqdata_type == TYPE_USERDATA)
        return &returned_reqdata.userdata;
    return NULL;
}

bool userdb_request_add(struct userdata_t *data)
{
    send_master(REQ_ADDUSERDATA, data, sizeof(*data));
    return returned_reqdata.boolean;
}

bool userdb_request_remove(const char *name)
{
    send_master(REQ_DELUSERDATA, name, strlen(name) + 1);
    return returned_reqdata.boolean;
}
