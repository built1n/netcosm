#include "netcosm.h"

#define SALT_LEN 8

#define ALGO GCRY_MD_SHA1

/* add a user to the on-disk database */

/*
 * format:
 *  { [username]:[salt]:[hash]:[authlevel]\n } [N]
*/

static bool valid_login_name(const char *name);

static void add_user_append(int fd, const char *name, const char *pass, int authlevel)
{
    if(errno < 0)
        perror("unknown");
    size_t pass_len = strlen(pass);

    /* salt */
    char *salted = malloc(pass_len + SALT_LEN + 1);
    char salt[SALT_LEN + 1];
    for(int i = 0; i < SALT_LEN; ++i)
    {
        salted[i] = salt[i] = 'A' + rand()%26;
    }
    salt[SALT_LEN] = '\0';

    memcpy(salted + SALT_LEN, pass, pass_len);
    salted[pass_len + SALT_LEN] = '\0';

    unsigned int hash_len = gcry_md_get_algo_dlen(ALGO);
    unsigned char *hash = malloc(hash_len);
    gcry_md_hash_buffer(ALGO, hash, salted, pass_len + SALT_LEN);
    free(salted);

    /* convert to hex */
    char *hex = malloc(hash_len * 2 + 1);
    char *ptr = hex;
    for(unsigned int i = 0; i < hash_len; ++i, ptr += 2)
        snprintf(ptr, 3, "%02x", hash[i]);

    free(hash);

    /* write */
    flock(fd, LOCK_EX);
    if(dprintf(fd, "%s:%s:%s:%d\n", name, salt, hex, authlevel) < 0)
        perror("dprintf");
    printf("writing %s:%s:%s:%d\n", name, salt, hex, authlevel);
    flock(fd, LOCK_UN);

    close(fd);
    free(hex);
    perror("add_user_append");
}

/* writes the contents of USERFILE to a temp file, and return its path, which is statically allocated */
static char *remove_user_internal(const char *user, int *found)
{
    FILE *in_fd = fopen(USERFILE, "a+");
    static char tmp[] = "userlist_tmp.XXXXXX";
    int out_fd = mkstemp(tmp);
    if(found)
        *found = 0;

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

    close(out_fd);
    fclose(in_fd);

    return tmp;
}

bool auth_remove(const char *user2)
{
    char *user = strdup(user2);
    strtok(user, "\r\n");
    if(valid_login_name(user))
    {
        int found = 0;
        char *tmp = remove_user_internal(user, &found);
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

bool add_change_user(const char *user2, const char *pass2, int level)
{
    char *user = strdup(user2);
    strtok(user, "\r\n");
    char *pass = strdup(pass2);
    strtok(pass, "\r\n");

    printf("Add user '%s'\n", user);

    if(!valid_login_name(user))
    {
        free(user);
        free(pass);
        return false;
    }

    /* remove any instances of the user in the file, write to temp file */
    char *tmp = remove_user_internal(user, NULL);

    printf("point 0\n");

    /* add user to end of temp file */
    int out_fd = open(tmp, O_WRONLY | O_APPEND);
    add_user_append(out_fd, user, pass, level);
    printf("point 1\n");
    close(out_fd);
    printf("point 2\n");

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
    printf("Welcome to NetCosm!\n");
    printf("Please set up the administrator account now.\n");

    char *admin_name;
    size_t len = 0;
    do {
        admin_name = NULL;
        printf("Admin account name: ");
        fflush(stdout);
        getline(&admin_name, &len, stdin);
        strtok(admin_name, "\r\n");
    } while(!valid_login_name(admin_name));

    printf("Admin password (_DO_NOT_ USE A VALUABLE PASSWORD): ");
    fflush(stdout);
    char *admin_pass = NULL;
    len = 0;
    getline(&admin_pass, &len, stdin);
    strtok(admin_pass, "\r\n");

    if(!add_change_user(admin_name, admin_pass, PRIV_ADMIN))
        error("Unknown error");

    /* zero the memory */
    memset(admin_name, 0, strlen(admin_name));
    memset(admin_pass, 0, strlen(admin_pass));
    free(admin_name);
    free(admin_pass);
}

struct authinfo_t auth_check(const char *name2, const char *pass2)
{
    sleep(1);

    /* get our own copy to remove newlines */
    char *name = strdup(name2);
    char *pass = strdup(pass2);
    strtok(name, "\r\n");
    strtok(pass, "\r\n");

    /* find it in the user list */

    FILE *f = fopen(USERFILE, "r");

    flock(fileno(f), LOCK_SH);

    struct authinfo_t ret;
    ret.success = false;
    ret.authlevel = PRIV_NONE;

    while(1)
    {
        char *line = NULL;
        size_t len = 0;
        if(getline(&line, &len, f) < 0)
        {
            free(line);
            goto bad;
        }
        if(!strcmp(strtok(line, ":\r\n"), name))
        {
            free(name);
            size_t pass_len = strlen(pass);

            char *salt = strdup(strtok(NULL, ":\r\n"));
            char *hash = strdup(strtok(NULL, ":\r\n"));

            ret.authlevel = strtol(strtok(NULL, ":\r\n"), NULL, 10);

            free(line);

            unsigned int hash_len = gcry_md_get_algo_dlen(ALGO);

            if(strlen(hash) != hash_len * 2)
                error("hash corrupt %d %d", strlen(hash), hash_len * 2);
            if(strlen(salt) != SALT_LEN)
                error("salt corrupt");

            char *buf = malloc(pass_len + SALT_LEN + 1);
            memcpy(buf, salt, strlen(salt));
            memcpy(buf + SALT_LEN, pass, pass_len);
            buf[pass_len + SALT_LEN] = '\0';

            free(salt);
            free(pass);

            unsigned char *newhash = malloc(hash_len);

            gcry_md_hash_buffer(ALGO, newhash, buf, pass_len + SALT_LEN);
            free(buf);
            char *hex = malloc(hash_len * 2 + 1);

            char *ptr = hex;
            for(unsigned int i = 0; i < hash_len; ++i, ptr += 2)
                snprintf(ptr, 3, "%02x", newhash[i]);

            free(newhash);

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
    }
good:
    printf("Successful authentication.\n");
    return ret;
bad:
    sleep(2);
    printf("Failed authentication.\n");
    return ret;
}
