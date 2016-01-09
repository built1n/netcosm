#include "netcosm.h"
#include "auth.h"

/*
 * on-disk database for storing user data
 *
 * child processes MUST go through the master to use this
 */

struct userdata_t {
    char username[MAX_NAME_LEN + 1];

    char salt[SALT_LEN + 1];

    /* in hex + NULL terminator */
    char passhash[AUTH_HASHLEN * 2 + 1];

    int priv;
};

/* call before using anything else */
void userdb_init(const char *dbfile);

/* looks up a username in the DB, returns NULL upon failure */
struct userdata_t *userdb_lookup(const char *username);

bool userdb_remove(const char *username);

/* is it empty? */
size_t userdb_size(void);

/*
 * adds an entry to the DB
 * if it already exists, OVERWRITE
 * returns a pointer to the added entry, NULL on failure
 *
 * a DUPLICATE of the entry will be inserted
 */
struct userdata_t *userdb_add(struct userdata_t*);

void userdb_shutdown(void);

/* save the DB to disk */
void userdb_write(const char*);
