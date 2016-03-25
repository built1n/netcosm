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
#include "multimap.h"
#include "server.h"
#include "userdb.h"

static void *map = NULL;
static char *db_file = NULL;

static void free_userdata(void *ptr)
{
    struct userdata_t *data = ptr;

    if(data->objects)
    {
        multimap_free(data->objects);
        data->objects = NULL;
    }
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
            error("unknown user file format");

        size_t n_users = read_size(fd);
        for(size_t u = 0; u < n_users; ++u)
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
                error("unexpected EOF");
            }

            data->objects = multimap_init(MIN(8, n_objects),
                                          hash_djb,
                                          compare_strings_nocase,
                                          obj_compare);

            multimap_setfreedata_cb(data->objects, obj_free);
            multimap_setdupdata_cb(data->objects, (void*(*)(void*))obj_dup);

            for(unsigned i = 0; i < n_objects; ++i)
            {
                struct object_t *obj = obj_read(fd);
                multimap_insert(data->objects, obj->name, obj);

                struct obj_alias_t *iter = obj->alias_list;
                while(iter)
                {
                    multimap_insert(data->objects, iter->alias, obj_dup(obj));
                    iter = iter->next;
                }
            }

            hash_insert(map, data->username, data);
        }

        close(fd);
    }
}

bool userdb_write(const char *file)
{
    debugf("Writing userdb...\n");

    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if(fd < 0)
        return false;
    write_uint32(fd, USERDB_MAGIC);

    write_size(fd, hash_size(map));
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
            n_objects = obj_count_noalias(user->objects);
        else
            n_objects = 0;

        write(fd, &n_objects, sizeof(n_objects));

        /* write objects */

        if(n_objects)
        {
            void *objptr = user->objects, *objsave;
            while(1)
            {
                const struct multimap_list *iter = multimap_iterate(objptr, &objsave, NULL);

                if(!iter)
                    break;
                objptr = NULL;

                while(iter)
                {
                    struct object_t *obj = iter->val;
                    if(!strcmp(iter->key, obj->name))
                    {
                        debugf("Writing an object to disk...\n");
                        obj_write(fd, iter->val);
                    }
                    iter = iter->next;
                }
            }
        }
    }
    close(fd);

    debugf("Done writing userdb.\n");

    return true;
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

bool userdb_add(struct userdata_t *data)
{
    struct userdata_t *new = calloc(1, sizeof(*new)); /* only in C! */
    memcpy(new, data, sizeof(*new));

    /* don't overwrite their inventory */
    struct userdata_t *old = userdb_lookup(new->username);

    if(old && old->objects)
    {
        new->objects = multimap_dup(old->objects);
    }
    else
    {
        new->objects = multimap_init(8, hash_djb, compare_strings_nocase, obj_compare);

        multimap_setdupdata_cb(new->objects, (void*(*)(void*))obj_dup);
        multimap_setfreedata_cb(new->objects, obj_free);
    }

    hash_overwrite(map, new->username, new);

    return userdb_write(db_file);
}

void userdb_dump(void)
{
    debugf("*** User Inventories Dump ***\n");
    void *userptr = map, *usersave = NULL;
    while(1)
    {
        struct userdata_t *user = hash_iterate(userptr, &usersave, NULL);
        if(!user)
            break;
        userptr = NULL;
        void *objptr = user->objects, *objsave;
        debugf("User %s:\n", user->username);
        while(1)
        {
            const struct multimap_list *iter = multimap_iterate(objptr, &objsave, NULL);

            if(!iter)
                break;
            objptr = NULL;

            while(iter)
            {
                struct object_t *obj = iter->val;
                debugf(" - Obj #%lu class %s: name %s\n", obj->id, obj->class->class_name, obj->name);
                iter = iter->next;
            }
        }
    }
}

void userdb_shutdown(void)
{
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

struct userdata_t *userdb_iterate(void **save)
{
    if(*save)
        return hash_iterate(NULL, save, NULL);
    else
        return hash_iterate(map, save, NULL);
}

bool userdb_add_obj(const char *name, struct object_t *obj)
{
    struct userdata_t *user = userdb_lookup(name);

    /* add aliases */
    struct obj_alias_t *alias = obj->alias_list;
    while(alias)
    {
        debugf("userdb adding object alias %s\n", alias->alias);
        multimap_insert(user->objects, alias->alias, obj_dup(obj));
        alias = alias->next;
    }

    return multimap_insert(user->objects, obj->name, obj_dup(obj));
}

bool userdb_del_obj_by_ptr(const char *username, struct object_t *obj)
{
    struct userdata_t *user = userdb_lookup(username);

    struct obj_alias_t *iter = obj->alias_list;

    struct object_t tmp;
    tmp.id = obj->id;

    while(iter)
    {
        multimap_delete(user->objects, iter->alias, &tmp);
        iter = iter->next;
    }

    return multimap_delete(user->objects, obj->name, &tmp);
}

bool userdb_del_obj(const char *username, const char *obj_name)
{
    struct userdata_t *user = userdb_lookup(username);
    const struct multimap_list *iter = multimap_lookup(user->objects, obj_name, NULL);
    while(iter)
    {
        const struct multimap_list *next = iter->next;
        struct object_t *obj = iter->val;
        userdb_del_obj_by_ptr(username, obj);
        iter = next;
    }

    return true;
}

/*** child request wrappers ***/
/* NOTE: these also work from the master, but it's better to use the
 * userdb_* funcs instead */

struct userdata_t *userdb_request_lookup(const char *name)
{
    if(are_child)
    {
        send_master(REQ_GETUSERDATA, name, strlen(name) + 1);
        if(reqdata_type == TYPE_USERDATA)
        {
            returned_reqdata.userdata.objects = NULL;
            return &returned_reqdata.userdata;
        }
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
