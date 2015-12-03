#include "netcosm.h"

/* add a user to the on-disk database */

/*
 * format:
 *  [login]:[hash]\n
*/

void add_user(const char *name, const char *pass)
{
    int fd = open(USERFILE, O_WRONLY | O_CREAT | O_APPEND, 0600);

    int hash_len = gcry_md_get_algo_dlen(GCRY_MD_SHA512);
    unsigned char *hash = malloc(hash_len);
    gcry_md_hash_buffer(GCRY_MD_SHA512, hash, pass, strlen(pass));
    char *hex = malloc(hash_len * 2 + 1);
    char *ptr = hex;
    for(int i = 0; i < hash_len; ++i, ++ptr)
        snprintf(ptr, 3, "%02x", hash[i]);

    flock(fd, LOCK_EX);

    dprintf(fd, "%s:%s\n", name, hex);

    flock(fd, LOCK_UN);
}

bool valid_login_name(const char *name)
{
    while(*name)
    {
        char c = *name++;
        switch(c)
        {
        case ':':
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
    } while(!valid_login_name(admin_name));

    printf("Admin password (DO NOT USE A VALUABLE PASSWORD): ");
    fflush(stdout);
    char *admin_pass = NULL;
    len = 0;
    getline(&admin_pass, &len, stdin);

    /* hash and store */

    add_user(admin_name, admin_pass);
}
