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

#include "auth.h"
#include "client.h"
#include "userdb.h"

static bool valid_login_name(const char *name);

/* returns a pointer to a malloc-allocated buffer containing the salted hex hash of pass */
/* salt should point to a buffer containing SALT_LEN random characters */
/* pass must be null-terminated */
static char *hash_pass_hex(const char *pass, const char *salt)
{
    size_t pass_len = strlen(pass);

    unsigned char *salted = malloc(pass_len + SALT_LEN + 1);
    memcpy(salted, salt, SALT_LEN);
    memcpy(salted + SALT_LEN, pass, pass_len);
    salted[pass_len + SALT_LEN] = '\0';

    unsigned char hash[AUTH_HASHLEN];

    SHA512(salted, pass_len + SALT_LEN, hash);

    unsigned char tmp[AUTH_HASHLEN];
    /* now hash the hash a million times to slow things down */
    for(int i = 0; i < (HASH_ITERS / 2) - 1; ++i)
    {
        AUTH_HASHFUNC(hash, AUTH_HASHLEN, tmp);
        AUTH_HASHFUNC(tmp, AUTH_HASHLEN, hash);
    }
    memset(tmp, 0, sizeof(tmp));

    memset(salted, 0, pass_len + SALT_LEN + 1);
    free(salted);

    /* convert to hex */
    char *hex = malloc(AUTH_HASHLEN * 2 + 1);
    char *ptr = hex;
    for(unsigned int i = 0; i < AUTH_HASHLEN; ++i, ptr += 2)
        snprintf(ptr, 3, "%02x", hash[i]);
    hex[AUTH_HASHLEN * 2] = '\0';

    return hex;
}

static void add_user_internal(const char *name, const char *pass, int authlevel)
{
    char salt[SALT_LEN + 1];
    for(int i = 0; i < SALT_LEN; ++i)
    {
        salt[i] = 'A' + rand()%26;
    }
    salt[SALT_LEN] = '\0';

    char *hexhash = hash_pass_hex(pass, salt);

    /* doesn't need to be malloc'd */
    struct userdata_t userdata;
    memset(&userdata, 0, sizeof(userdata));

    strncpy(userdata.username, name, sizeof(userdata.username));
    memcpy(userdata.passhash, hexhash, sizeof(userdata.passhash));

    free(hexhash);

    userdata.priv = authlevel;
    userdata.last_login = time(0);
    memcpy(userdata.salt, salt, sizeof(salt));

    userdb_request_add(&userdata);
}

bool auth_user_del(const char *user2)
{
    char *user = strdup(user2);
    remove_cruft(user);
    if(valid_login_name(user))
        return userdb_request_remove(user);
    else
    {
        free(user);
        return false;
    }
}

bool auth_user_add(const char *user2, const char *pass2, int level)
{
    char *user = strdup(user2);
    remove_cruft(user);
    char *pass = strdup(pass2);
    remove_cruft(pass);

    debugf("Add user '%s'\n", user);

    if(!valid_login_name(user))
    {
        free(user);
        free(pass);
        return false;
    }

    add_user_internal(user, pass, level);

    free(user);
    memset(pass, 0, strlen(pass));
    free(pass);

    return true;
}

static bool valid_login_name(const char *name)
{
    size_t len = 0;
    while(*name)
    {
        char c = *name++;
        ++len;
        if(len > MAX_NAME_LEN)
            return false;
        if(!(isalpha(c) || isdigit(c)))
            return false;
    }
    return true;
}

void first_run_setup(void)
{
    debugf("Welcome to NetCosm!\n");
    debugf("Please set up the administrator account now.\n");

    char *admin_name;
    size_t len = 0;
    do {
        admin_name = NULL;
        debugf("Admin account name: ");
        fflush(stdout);
        getline(&admin_name, &len, stdin);
        remove_cruft(admin_name);
    } while(!valid_login_name(admin_name));

    debugf("Admin password (_DO_NOT_ USE A VALUABLE PASSWORD): ");
    fflush(stdout);
    char *admin_pass = NULL;
    len = 0;
    getline(&admin_pass, &len, stdin);
    remove_cruft(admin_pass);

    if(!auth_user_add(admin_name, admin_pass, PRIV_ADMIN))
        error("Unknown error");

    /* zero the memory */
    memset(admin_name, 0, strlen(admin_name));
    memset(admin_pass, 0, strlen(admin_pass));
    free(admin_name);
    free(admin_pass);
}

struct userdata_t *auth_check(const char *name2, const char *pass2)
{
    /* get our own copy to remove newlines */
    char *name = strdup(name2);
    char *pass = strdup(pass2);
    remove_cruft(name);
    remove_cruft(pass);

    /* find it in the user list */
    struct userdata_t *data = userdb_request_lookup(name);

    free(name);

    char *salt = data->salt, *hash = data->passhash;

    if(data)
    {
        debugf("auth module: user %s found\n", name2);

        /* hashes are in HEX to avoid the Trucha bug */
        char *new_hash_hex = hash_pass_hex(pass, salt);

        bool success = true;
        /* constant-time comparison to a timing attack */
        for(int i = 0; i < AUTH_HASHLEN; ++i)
        {
            if(new_hash_hex[i] != hash[i])
                success = false;
        }

        free(new_hash_hex);

        if(success)
        {
            memset(pass, 0, strlen(pass));
            free(pass);

            return data;
        }
    }

    debugf("auth failure for user %s\n", name2);

    memset(pass, 0, strlen(pass));
    free(pass);

    /* failure */
    sleep(2);
    return NULL;
}

void auth_user_list(void)
{
    /* FIXME: todo */
}
