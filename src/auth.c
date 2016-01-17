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

    char *salted = gcry_malloc_secure(pass_len + SALT_LEN + 1);
    memcpy(salted, salt, SALT_LEN);
    memcpy(salted + SALT_LEN, pass, pass_len);
    salted[pass_len + SALT_LEN] = '\0';

    unsigned int hash_len = gcry_md_get_algo_dlen(ALGO);
    unsigned char *hash = gcry_malloc_secure(hash_len);

    gcry_md_hash_buffer(ALGO, hash, salted, pass_len + SALT_LEN);

    unsigned char *tmp = gcry_malloc_secure(hash_len);
    /* now hash the hash half a million times to slow things down */
    for(int i = 0; i < HASH_ITERS - 1; ++i)
    {
        memcpy(tmp, hash, hash_len);
        gcry_md_hash_buffer(ALGO, hash, tmp, hash_len);
    }
    gcry_free(tmp);

    memset(salted, 0, pass_len + SALT_LEN + 1);
    gcry_free(salted);

    /* convert to hex */
    char *hex = malloc(hash_len * 2 + 1);
    char *ptr = hex;
    for(unsigned int i = 0; i < hash_len; ++i, ptr += 2)
        snprintf(ptr, 3, "%02x", hash[i]);
    hex[hash_len * 2] = '\0';
    sig_debugf("hash is %s\n", hex);

    gcry_free(hash);

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

    strncpy(userdata.username, name, sizeof(userdata.username));

    memcpy(userdata.passhash, hexhash, sizeof(userdata.passhash));

    free(hexhash);

    userdata.priv = authlevel;

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

    /* add user to end of temp file */
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
        sig_debugf("auth module: user %s found\n", name2);
        char *new_hash_hex = hash_pass_hex(pass, salt);

        memset(pass, 0, strlen(pass));
        free(pass);

        /* hashes are in HEX to avoid the Trucha bug */
        bool success = !memcmp(new_hash_hex, hash, strlen(hash));

        free(new_hash_hex);

        if(success)
            return data;
    }

    sig_debugf("auth failure: username not found\n");

    /* failure */
    sleep(2);
    return NULL;
}

void auth_user_list(void)
{
    FILE *f = fopen(USERFILE, "r");

    flock(fileno(f), LOCK_SH);

    while(1)
    {
        char *line = NULL;
        char *save;
        size_t len = 0;
        if(getline(&line, &len, f) < 0)
        {
            free(line);
            fclose(f);
            return;
        }
        char *user = strdup(strtok_r(line, ":\r\n", &save));
        strtok_r(NULL, ":\r\n", &save);
        strtok_r(NULL, ":\r\n", &save);
        int priv = strtol(strtok_r(NULL, ":\r\n", &save), NULL, 0);
        out("User %s priv %d\n", user, priv);
        free(user);
        free(line);
    }
}
