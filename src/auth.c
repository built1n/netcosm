/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2015 Franklin Wei
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

#include "netcosm.h"

#define SALT_LEN 12
#define ALGO GCRY_MD_SHA512
//#define HASH_ITERS 500000
#define HASH_ITERS 1

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

    gcry_free(hash);

    return hex;
}

static void add_user_append(int fd, const char *name, const char *pass, int authlevel)
{
    char salt[SALT_LEN + 1];
    for(int i = 0; i < SALT_LEN; ++i)
    {
        salt[i] = 'A' + rand()%26;
    }
    salt[SALT_LEN] = '\0';

    char *hex = hash_pass_hex(pass, salt);

    /* write */
    flock(fd, LOCK_EX);
    if(dprintf(fd, "%s:%s:%s:%d\n", name, salt, hex, authlevel) < 0)
        perror("dprintf");
    flock(fd, LOCK_UN);

    close(fd);
    free(hex);
}

/* writes the contents of USERFILE to a temp file, and return its path, which is statically allocated */
static int remove_user_internal(const char *user, int *found, char **filename)
{
    FILE *in_fd = fopen(USERFILE, "a+");
    static char tmp[32];
    const char *template = "userlist_tmp.XXXXXX";
    strncpy(tmp, template, sizeof(tmp));

    int out_fd = mkstemp(tmp);

    if(found)
        *found = 0;
    if(filename)
        *filename = tmp;

    while(1)
    {
        char *line = NULL;
        char *junk;
        size_t buflen = 0;
        ssize_t len = getline(&line, &buflen, in_fd);


        /* getline's return value is the actual length of the line read */
        /* it's second argument in fact stores the length of the /buffer/, not the line */
        if(len < 0)
        {
            free(line);
            break;
        }

        char *old = strdup(line);

        char *user_on_line = strtok_r(line, ":\r\n", &junk);

        if(strcmp(user_on_line, user) != 0)
        {
            write(out_fd, old, len);
        }
        else
            if(found)
                (*found)++;
        free(line);
        free(old);
    }

    fclose(in_fd);

    return out_fd;
}

bool auth_user_del(const char *user2)
{
    char *user = strdup(user2);
    remove_cruft(user);
    if(valid_login_name(user))
    {
        int found = 0;
        char *tmp;
        remove_user_internal(user, &found, &tmp);
        free(user);
        if(found)
        {
            rename(tmp, USERFILE);
            return true;
        }
        else
        {
            remove(tmp);
            return false;
        }
    }
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

    /* remove any instances of the user in the file, write to temp file */
    char *tmp;
    int out_fd = remove_user_internal(user, NULL, &tmp);

    /* add user to end of temp file */
    add_user_append(out_fd, user, pass, level);
    close(out_fd);
    free(user);
    memset(pass, 0, strlen(pass));
    free(pass);

    /* rename temp file -> user list */
    rename(tmp, USERFILE);

    return true;
}

static bool valid_login_name(const char *name)
{
    while(*name)
    {
        char c = *name++;
        switch(c)
        {
        case ' ':
        case ':':
        case '\n':
        case '\r':
        case '\t':
            return false;
        default:
            break;
        }
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

struct authinfo_t auth_check(const char *name2, const char *pass2)
{
    /* get our own copy to remove newlines */
    char *name = strdup(name2);
    char *pass = strdup(pass2);
    remove_cruft(name);
    remove_cruft(pass);

    /* find it in the user list */

    FILE *f = fopen(USERFILE, "r");

    flock(fileno(f), LOCK_SH);

    struct authinfo_t ret;
    ret.success = false;
    ret.authlevel = PRIV_NONE;
    ret.user = NULL;

    while(1)
    {
        char *line = NULL;
        char *save;
        size_t len = 0;
        if(getline(&line, &len, f) < 0)
        {
            free(line);
            free(name);
            memset(pass, 0, strlen(pass));
            free(pass);
            goto bad;
        }
        if(!strcmp(strtok_r(line, ":\r\n", &save), name))
        {
            free(name);

            char *salt = strdup(strtok_r(NULL, ":\r\n", &save));
            char *hash = strdup(strtok_r(NULL, ":\r\n", &save));

            ret.authlevel = strtol(strtok_r(NULL, ":\r\n", &save), NULL, 10);

            free(line);

            unsigned int hash_len = gcry_md_get_algo_dlen(ALGO);

            if(strlen(hash) != hash_len * 2)
                error("hash corrupt (wrong length)");
            if(strlen(salt) != SALT_LEN)
                error("salt corrupt (wrong length)");

            char *hex = hash_pass_hex(pass, salt);

            memset(pass, 0, strlen(pass));
            free(pass);
            free(salt);

            if(!memcmp(hex, hash, hash_len * 2))
            {
                free(hex);
                free(hash);
                ret.success = true;
                goto good;
            }
            else
            {
                free(hex);
                free(hash);
                ret.success = false;
                goto bad;
            }
        }
        else
            free(line);
    }
good:
    debugf("Successful authentication.\n");
    fclose(f);
    return ret;
bad:
    sleep(2);
    fclose(f);
    debugf("Failed authentication.\n");
    return ret;
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
