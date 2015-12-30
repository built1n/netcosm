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

#ifndef _NC_H_
#define _NC_H_
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
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

#include "auth.h"
#include "hash.h"
#include "telnet.h"

#define USERFILE "users.dat"
#define WORLDFILE "world.dat"
#define WORLD_MAGIC 0xff467777
#define MAX_FAILURES 3
#define NETCOSM_VERSION "v0.1"

#define PRIV_NONE -1
#define PRIV_USER 0
#define PRIV_ADMIN 1337

/* child<->master commands */
/* children might not implement all of these */
/* meanings might be different for the server and child, see comments */
#define REQ_NOP               0 /* server, child: do nothing */
#define REQ_BCASTMSG          1 /* server: broadcast text; child: print following text */
#define REQ_LISTCLIENTS       2 /* server: list childs */
#define REQ_CHANGESTATE       3 /* server: change child state flag */
#define REQ_CHANGEUSER        4 /* server: change child login name */
#define REQ_HANG              5 /* <UNIMP> server: loop forever */
#define REQ_KICK              6 /* server: kick PID with message; child: print message, quit */
#define REQ_WAIT              7 /* server: sleep 10s */
#define REQ_GETROOMDESC       8 /* server: send child room description */
#define REQ_SETROOM           9 /* server: set child room */
#define REQ_MOVE              10 /* server: move child based on direction; child: success or failure */
#define REQ_GETROOMNAME       11 /* server: send child's room name */
#define REQ_LISTROOMCLIENTS   12 /* server: list clients in child's room */

/* child states, sent as an int to the master */
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
#define NONE_IN NULL
#define NONE_OT NULL

#define MSG_MAX 512

#define ROOM_NONE -1

#define ARRAYLEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX(a,b) ((a>b)?(a):(b))
#define MIN(a,b) ((a<b)?(a):(b))

#ifndef NDEBUG
#define debugf(fmt,...) debugf_real(fmt, ##__VA_ARGS__)
#define sig_debugf debugf
#else
#define debugf(fmt,...)
#define sig_debugf debugf
#endif

typedef int room_id;

/* used by the room module to keep track of users in rooms */
struct user_t {
    struct child_data *data;
    struct user_t *next;
};

enum direction_t { DIR_N = 0, DIR_NE, DIR_E, DIR_SE, DIR_S, DIR_SW, DIR_W, DIR_NW, DIR_UP, DIR_DN, DIR_IN, DIR_OT, NUM_DIRECTIONS };

struct object_t {
    const char *class;

};

struct verb_t {
    const char *name;

    /* toks is strtok_r's pointer */
    void (*execute)(const char *toks);
};

struct child_data {
    pid_t pid;
    int readpipe[2];
    int outpipe[2];

    int state;
    room_id room;
    char *user;

    struct in_addr addr;
};

/* the data we get from a world module */
struct roomdata_t {
    /* the non-const pointers can be modified by the world module */
    const char * const uniq_id;

    /* mutable properties */
    char *name;
    char *desc;

    const char * const adjacent[NUM_DIRECTIONS];

    void (* const hook_init)(room_id id);
    void (* const hook_enter)(room_id room, pid_t player);
    void (* const hook_say)(room_id room, pid_t player, const char *msg);
    void (* const hook_leave)(room_id room, pid_t player);
};

struct room_t {
    room_id id;
    struct roomdata_t data;

    room_id adjacent[NUM_DIRECTIONS];

    /* arrays instead of linked lists because insertion should be rare for these */
    size_t objects_sz;
    struct object_t *objects;

    size_t verbs_sz;
    struct verb_t *verbs;

    /* linked list for users, random access is rare */
    struct user_t *users;
    int num_users;
};

extern const struct roomdata_t netcosm_world[];
extern const size_t netcosm_world_sz;
extern const char *netcosm_world_name;

/* called for every client */
void client_main(int sock, struct sockaddr_in *addr, int, int to_parent, int from_parent);

void out(const char *fmt, ...) __attribute__((format(printf,1,2)));
void out_raw(const unsigned char*, size_t);

void telnet_init(void);
void telnet_handle_command(const unsigned char*);
void telnet_echo_on(void);
void telnet_echo_off(void);

void world_init(const struct roomdata_t *data, size_t sz, const char *name);
bool world_load(const char *fname, const struct roomdata_t *data, size_t data_sz, const char *world_name);
void world_save(const char *fname);

struct room_t *room_get(room_id id);
bool room_user_add(room_id id, struct child_data *child);
bool room_user_del(room_id id, struct child_data *child);

void world_free(void);

/* utility functions */
void __attribute__((noreturn,format(printf,1,2))) error(const char *fmt, ...);
void debugf_real(const char *fmt, ...);
void remove_cruft(char*);
#endif
