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

static void free_userdata_and_objs(void *ptr)
{
    struct userdata_t *data = ptr;

    if(data->objects)
    {
        hash_setfreedata_cb(data->objects, obj_free);
        hash_free(data->objects);
        data->objects = NULL;
    }
    free(data);
}

static void free_userdata(void *ptr)
{
    struct userdata_t *data = ptr;

    hash_free(data->objects);
    free(data);
}

/*
 * the user DB is stored on disk as an binary flat file
 *
 * this is then loaded into fixed-sized hash map at init
 * TODO: re-implement with B-tree
 */
void userdb_init(const char *file)
{
    db_file = strdup(file);

    int fd = open(file, O_RDONLY);
    map = hash_init(256, hash_djb, compare_strings);
    hash_setfreedata_cb(map, free_userdata);

    /* 0 is a valid fd */
    if(fd >= 0)
    {
        if(read_uint32(fd) != USERDB_MAGIC)
            error("bad userdb magic value");
        while(1)
        {
            struct userdata_t *data = calloc(1, sizeof(*data));

            if(read(fd, data, sizeof(*data)) != sizeof(*data))
            {
                free(data);
                break;
            }

            size_t n_objects;
            if(read(fd, &n_objects, sizeof(n_objects)) != sizeof(n_objects))
            {
                free(data);
                break;
            }

            data->objects = hash_init(MIN(8, n_objects),
                                      hash_djb,
                                      compare_strings);

            for(unsigned i = 0; i < n_objects; ++i)
            {
                struct object_t *obj = obj_read(fd);
                hash_insert(data->objects, obj->name, obj);
            }

            hash_insert(map, data->username, data);
        }

        close(fd);
    }
}

void userdb_write(const char *file)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    write_uint32(fd, USERDB_MAGIC);
    void *save, *ptr = map;
    while(1)
    {
        struct userdata_t *user = hash_iterate(ptr, &save, NULL);
        ptr = NULL;
        if(!user)
            break;

        write(fd, user, sizeof(*user));

        size_t n_objects;
        if(user->objects)
            n_objects = hash_size(user->objects);
        else
            n_objects = 0;

        write(fd, &n_objects, sizeof(n_objects));

        /* write objects */

        if(n_objects)
        {
            void *objptr = user->objects, *objsave;
            while(1)
            {
                struct object_t *obj = hash_iterate(objptr, &objsave, NULL);
                if(!obj)
                    break;
                objptr = NULL;
                obj_write(fd, obj);
            }
        }
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

/* returns NULL on success */
struct userdata_t *userdb_add(struct userdata_t *data)
{
    struct userdata_t *new = calloc(1, sizeof(*new)); /* only in C! */
    memcpy(new, data, sizeof(*new));

    /* don't overwrite their inventory */
    struct userdata_t *old = userdb_lookup(new->username);
    if(old && old->objects)
        new->objects = hash_dup(old->objects);
    else
        new->objects = hash_init(8, hash_djb, compare_strings);

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
    if(map && db_file && !are_child)
        userdb_write(db_file);

    if(map)
    {
        hash_setfreedata_cb(map, free_userdata_and_objs);
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
