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

#include "telnet.h"

#define USERFILE "users.dat"
#define MAX_FAILURES 3
#define NETCOSM_VERSION "v0.1"

#define PRIV_NONE -1
#define PRIV_USER 0
#define PRIV_ADMIN 1337

struct authinfo_t {
    bool success;
    const char *user;
    int authlevel;
};

void client_main(int sock, struct sockaddr_in *addr, int, int to_parent, int from_parent);
void __attribute__((noreturn)) error(const char *fmt, ...);
void first_run_setup(void);
struct authinfo_t auth_check(const char*, const char*);

/* add or change a user */
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

/* child->master commands */
#define REQ_INVALID     0
#define REQ_BCASTMSG    1
#define REQ_LISTCLIENTS 2
#define REQ_CHANGESTATE 3
#define REQ_CHANGEUSER  4

#define STATE_INIT      0
#define STATE_AUTH      1
#define STATE_CHECKING  2
#define STATE_LOGGEDIN  3
#define STATE_ADMIN     4
#define STATE_FAILED    5

void auth_list_users(void);
