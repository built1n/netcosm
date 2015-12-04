#include "netcosm.h"

#define SALT_LEN 8

#define ALGO GCRY_MD_SHA1

/* add a user to the on-disk database */

/*
 * format:
 *  [login]:[hash]\n
*/

static void add_user_append(int fd, const char *name, const char *pass, int authlevel)
{
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

    printf("hashing %s\n", salted);

    int hash_len = gcry_md_get_algo_dlen(ALGO);
    unsigned char *hash = malloc(hash_len);
    gcry_md_hash_buffer(ALGO, hash, salted, pass_len + SALT_LEN);
    free(salted);

    /* convert to hex */
    char *hex = malloc(hash_len * 2 + 1);
    char *ptr = hex;
    for(int i = 0; i < hash_len; ++i, ptr += 2)
        snprintf(ptr, 3, "%02x", hash[i]);

    free(hash);

    /* write */

    flock(fd, LOCK_EX);

    dprintf(fd, "%s:%s:%s:%d\n", name, salt, hex, authlevel);

    free(hex);

    flock(fd, LOCK_UN);
    close(fd);
}

void add_user(const char *name2, const char *pass2, int level)
{
    char *name = strdup(name2);
    strtok(name, "\r\n");
    char *pass = strdup(pass2);
    strtok(pass, "\r\n");

    /* remove any instances of the user in the file, write to temp file */

    FILE *in_fd = fopen(USERFILE, "w+");
    flock(fileno(in_fd), LOCK_SH);
    char *tmp = tmpnam(NULL);
    int out_fd = open(tmp, O_CREAT | O_WRONLY, 0600);
    while(1)
    {
        char *line = NULL;
        size_t len = 0;
        if(getline(&line, &len, in_fd) < 0)
            break;
        if(strcmp(strtok(line, ":\r\n"), name) != 0)
            write(out_fd, line, len);
    }
    flock(fileno(in_fd), LOCK_UN);
    fclose(in_fd);

    /* add user to end of temp file */

    add_user_append(out_fd, name, pass, level);
    close(out_fd);

    /* rename temp file -> user list */
    int fd = open(tmp, O_RDONLY);
    int userfile = open(USERFILE, O_WRONLY | O_TRUNC | O_CREAT, 0600);

    ssize_t nread;
    char buf[1024];
    while (nread = read(fd, buf, sizeof buf), nread > 0)
    {
        printf("writing %d bytes\n", nread);
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(userfile, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else
                break;
        } while (nread > 0);
    }

    close(userfile);
    remove(tmp);
}

bool valid_login_name(const char *name)
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
    add_user(admin_name, admin_pass, 0);

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
    strtok(name, "\r\n");
    strtok(pass, "\r\n");

    /* find it in the user list */

    FILE *f = fopen(USERFILE, "r");

    flock(fileno(f), LOCK_SH);

    struct authinfo_t ret;

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

            int hash_len = gcry_md_get_algo_dlen(ALGO);

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
            for(int i = 0; i < hash_len; ++i, ptr += 2)
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
    printf("Failed authentication.\n");
    sleep(1);
    return ret;
}
