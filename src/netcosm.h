#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hash.h"
#include "telnet.h"

#define USERFILE "users.dat"
#define MAX_FAILURES 3
#define NETCOSM_VERSION "v0.1"

#define PRIV_NONE -1
#define PRIV_USER 0
#define PRIV_ADMIN 1337

/* child<->master commands */
/* children might not implement all of these */
/* meanings might be different for the server and child, see comments */

#define REQ_NOP         0 /* server, child: do nothing */
#define REQ_BCASTMSG    1 /* server: broadcast text; child: print following text */
#define REQ_LISTCLIENTS 2 /* server: list childs */
#define REQ_CHANGESTATE 3 /* server: change child state flag */
#define REQ_CHANGEUSER  4 /* server: change child login name */
#define REQ_HANG        5 /* <UNIMP> server: loop forever */
#define REQ_KICK        6 /* server: kick PID with message; child: print message, quit */
#define REQ_WAIT        7 /* server: sleep 10s */
#define REQ_GETROOMDESC 8 /* server: send child room description */
#define REQ_SETROOM     9 /* server: set child room */
#define REQ_MOVE        10 /* server: move child based on direction; child: success or failure */
#define REQ_GETROOMNAME 11 /* server: send child's room name */

/* child states */
#define STATE_INIT      0 /* initial state */
#define STATE_AUTH      1 /* at login screen */
#define STATE_CHECKING  2 /* checking password */
#define STATE_LOGGEDIN  3 /* logged in as user */
#define STATE_ADMIN     4 /* logged in w/ admin privs */
#define STATE_FAILED    5 /* failed a password attempt */

/* for convenience when writing world specs */
#define NONE_N  NULL
#define NONE_NE NULL
#define NONE_E  NULL
#define NONE_SE NULL
#define NONE_S  NULL
#define NONE_SW NULL
#define NONE_W  NULL
#define NONE_NW NULL
#define NONE_UP NULL
#define NONE_DN NULL

struct authinfo_t {
    bool success;
    const char *user;
    int authlevel;
};

/* logged in users are identified by the PID of the process serving them */
struct user_t {
    pid_t pid;
};

enum direction_t { DIR_N = 0, DIR_NE, DIR_E, DIR_SE, DIR_S, DIR_SW, DIR_W, DIR_NW, DIR_UP, DIR_DOWN, NUM_DIRECTIONS };

struct item_t {
    const char *class;

};

struct verb_t {
    const char *name;

    /* toks is strtok_r's pointer */
    void (*execute)(const char *toks);
};

typedef int room_id;

#define ROOM_NONE -1

/* the data we get from a world module */
struct roomdata_t {
    const char *uniq_id;
    const char *name;
    const char *desc;

    const char *adjacent[NUM_DIRECTIONS];

    void (*hook_init)(room_id id);
    void (*hook_enter)(room_id room, pid_t player);
    void (*hook_say)(room_id room, pid_t player, const char *msg);
    void (*hook_leave)(room_id room, pid_t player);
};

struct room_t {
    room_id id;
    struct roomdata_t data;

    room_id adjacent[NUM_DIRECTIONS];

    /* arrays instead of linked lists because insertion should be rare for these */
    struct item_t *items;
    size_t items_sz;

    struct verb_t *verbs;
    size_t verbs_sz;
};

extern const struct roomdata_t netcosm_world[];
extern const size_t netcosm_world_sz;

void client_main(int sock, struct sockaddr_in *addr, int, int to_parent, int from_parent);
void __attribute__((noreturn)) error(const char *fmt, ...);
void first_run_setup(void);
struct authinfo_t auth_check(const char*, const char*);

/* add or change a user, NOT reentrant */
bool add_change_user(const char *user2, const char *pass2, int level);
bool auth_remove(const char*);

void telnet_handle_command(const unsigned char*);

#define ARRAYLEN(x) (sizeof(x)/sizeof(x[0]))

void out(const char *fmt, ...) __attribute__((format(printf,1,2)));
void out_raw(const unsigned char*, size_t);
void telnet_init(void);
void telnet_echo_on(void);
void telnet_echo_off(void);
#define MSG_MAX 512

void remove_cruft(char*);

void auth_list_users(void);
void world_init(const struct roomdata_t *data, size_t sz);
void sig_printf(const char *fmt, ...);
void world_free(void);

/* only the master calls this */
struct room_t *room_get(room_id id);
