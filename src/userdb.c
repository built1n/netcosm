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

#include "client.h"
#include "hash.h"
#include "server.h"
#include "userdb.h"

static void *map = NULL;
static char *db_file = NULL;

/*
 * the user DB is stored on disk as an ASCII database
 *
 * this is then loaded into fixed-sized hash map at init
 * TODO: implement with B-tree
 */
void userdb_init(const char *file)
{
    db_file = strdup(file);

    /* FILE* for getline() */
    FILE *f = fopen(file, "r");
    map = hash_init(256, hash_djb, compare_strings);
    hash_setfreedata_cb(map, free);

    char *format;
    asprintf(&format, "%%%d[a-z0-9 ]:%%%d[A-Z]:%%%ds:%%d:%%ld\n",
             MAX_NAME_LEN, SALT_LEN, AUTH_HASHLEN * 2);

    if(f)
    {
        while(1)
        {
            struct userdata_t *data = calloc(1, sizeof(*data));

            int ret = fscanf(f, format,
                             data->username,
                             data->salt,
                             data->passhash,
                             &data->priv,
                             &data->last_login);

            if(ret != 5)
            {
                free(data);
                break;
            }

            hash_insert(map, data->username, data);
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
        dprintf(fd, "%s:%*s:%*s:%d:%ld\n",
                user->username,
                SALT_LEN, user->salt,
                AUTH_HASHLEN*2, user->passhash,
                user->priv,
                user->last_login);
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
        userdb_write(db_file);
        return true;
    }
    return false;
}

/* should never fail, but returns NULL if something weird happens */
struct userdata_t *userdb_add(struct userdata_t *data)
{
    struct userdata_t *new = calloc(1, sizeof(*new)); /* only in C! */
    memcpy(new, data, sizeof(*new));
    strncpy(new->username, data->username, sizeof(new->username));

    struct userdata_t *ret;

    if((ret = hash_insert(map, new->username, new))) /* already exists */
    {
        hash_remove(map, new->username);
        ret = hash_insert(map, new->username, new);
    }

    userdb_write(db_file);

    return ret;
}

void userdb_shutdown(void)
{
    if(map && db_file)
        userdb_write(db_file);
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
    return hash_size(map);
}

/* child request wrappers */
/* NOTE: also works from the master, but it's better to use the userdb_* funcs instead */

struct userdata_t *userdb_request_lookup(const char *name)
{
    if(are_child)
    {
        send_master(REQ_GETUSERDATA, name, strlen(name) + 1);
        sig_debugf("returned reqdata is of type %d\n", reqdata_type);
        if(reqdata_type == TYPE_USERDATA)
            return &returned_reqdata.userdata;
        return NULL;
    }
    else
        return userdb_lookup(name);
}

bool userdb_request_add(struct userdata_t *data)
{
    if(are_child)
    {
        send_master(REQ_ADDUSERDATA, data, sizeof(*data));
        return returned_reqdata.boolean;
    }
    else
        return userdb_add(data);
}

bool userdb_request_remove(const char *name)
{
    if(are_child)
    {
        send_master(REQ_DELUSERDATA, name, strlen(name) + 1);
        return returned_reqdata.boolean;
    }
    else
        return userdb_remove(name);
}
