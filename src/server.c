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

#include "client.h"
#include "hash.h"
#include "server.h"
#include "userdb.h"
#include "util.h"

#define PORT 1234
#define BACKLOG 512

/* global data */
bool are_child = false;
void *child_map = NULL;

/* assume int is atomic */
volatile int num_clients = 0;

/* local data */
static uint16_t port;

static int server_socket;

void __attribute__((noreturn)) error(const char *fmt, ...)
{
    char buf[128];
    memset(buf, 0, sizeof(buf));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    perror(buf);
    exit(EXIT_FAILURE);
}

static void free_child_data(void *ptr)
{
    struct child_data *child = ptr;
    if(child->user)
    {
        free(child->user);
        child->user = NULL;
    }
    if(child->io_watcher)
    {
        ev_io_stop(EV_DEFAULT_ child->io_watcher);

        free(child->io_watcher);
        child->io_watcher = NULL;
    }
    free(ptr);
}

static void handle_disconnects(void)
{
    int saved_errno = errno;

    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        struct child_data *child = hash_lookup(child_map, &pid);

        debugf("Client disconnect.\n");

        room_user_del(child->room, child);

        --num_clients;

        hash_remove(child_map, &pid);
    }

    errno = saved_errno;
}

volatile sig_atomic_t reap_children = 0;

static void sigchld_handler(int sig)
{
    (void) sig;
    reap_children = 1;
}

static void handle_client(int fd, struct sockaddr_in *addr,
                          int nclients, int to, int from)
{
    client_main(fd, addr, nclients, to, from);
}

static void __attribute__((noreturn)) serv_cleanup(void)
{
    if(!are_child)
        debugf("Shutdown server.\n");
    else
        debugf("Shutdown worker.\n");

    if(shutdown(server_socket, SHUT_RDWR) > 0)
        error("shutdown");

    close(server_socket);

    world_free();
    reqmap_free();
    hash_free(child_map);
    child_map = NULL;

    extern void *dir_map;
    hash_free(dir_map);
    dir_map = NULL;

    userdb_shutdown();

    extern char *current_user;
    if(current_user)
        free(current_user);

    ev_default_destroy();

    exit(0);
}

static void __attribute__((noreturn)) sigint_handler(int s)
{
    (void) s;
    exit(0);
}

static void check_userfile(void)
{
    if(access(USERFILE, F_OK) < 0 || userdb_size() == 0)
        first_run_setup();

    if(access(USERFILE, R_OK | W_OK) < 0)
        error("cannot access "USERFILE);
}

static void load_worldfile(void)
{
    extern const struct roomdata_t netcosm_world[];
    extern const size_t netcosm_world_sz;
    extern const char *netcosm_world_name;

    if(access(WORLDFILE, F_OK) < 0)
    {
        world_init(netcosm_world, netcosm_world_sz, netcosm_world_name);

        world_save(WORLDFILE);
    }
    else if(access(WORLDFILE, R_OK | W_OK) < 0)
        error("cannot access "WORLDFILE);
    else
        if(!world_load(WORLDFILE, netcosm_world, netcosm_world_sz, netcosm_world_name))
            error("Failed to load world from disk.\nTry removing "WORLDFILE".");
}

static int server_bind(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock<0)
        error("socket");

    int tmp = 1;

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof tmp) < 0)
        error("setsockopt");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr*) &addr, sizeof addr) < 0)
        error("bind");

    if(listen(sock, BACKLOG) < 0)
        error("listen");

    return sock;
}

static void childreq_cb(EV_P_ ev_io *w, int revents)
{
    (void) EV_A;
    (void) w;
    /* data from a child's pipe */
    if(revents & EV_READ)
    {
        if(!handle_child_req(w->fd))
        {
            handle_disconnects();
        }
    }
    if(reap_children)
        handle_disconnects();
}

static void new_connection_cb(EV_P_ ev_io *w, int revents)
{
    (void) EV_A;
    (void) w;
    (void) revents;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int new_sock = accept(server_socket, (struct sockaddr*) &client, &client_len);
    if(new_sock < 0)
        error("accept");

    ++num_clients;

    int readpipe[2]; /* child->parent */
    int outpipe [2]; /* parent->child */

    if(pipe2(readpipe, O_DIRECT) < 0)
        error("error creating pipe, need linux kernel >= 3.4");
    if(pipe2(outpipe, O_DIRECT) < 0)
        error("error creating pipe, need linux kernel >= 3.4");

    pid_t pid = fork();
    if(pid < 0)
        error("fork");

    if(!pid)
    {
        /* child */
        are_child = true;
        close(readpipe[0]);
        close(outpipe[1]);
        close(server_socket);

        /* only the master process controls the world */
        world_free();
        reqmap_free();
        hash_free(child_map);
        child_map = NULL;

        /* we don't need libev anymore */
        ev_default_destroy();

        /* user DB requests go through the master */
        userdb_shutdown();

        debugf("Child with PID %d spawned\n", getpid());

        server_socket = new_sock;

        handle_client(new_sock, &client, num_clients, readpipe[1], outpipe[0]);

        exit(0);
    }
    else
    {
        /* parent */
        close(readpipe[1]);
        close(outpipe[0]);
        close(new_sock);

        /* add the child to the child map */
        struct child_data *new = calloc(1, sizeof(struct child_data));
        memcpy(new->outpipe, outpipe, sizeof(outpipe));
        memcpy(new->readpipe, readpipe, sizeof(readpipe));
        new->addr = client.sin_addr;
        new->pid = pid;
        new->state = STATE_INIT;
        new->user = NULL;

        ev_io *new_io_watcher = calloc(1, sizeof(ev_io));
        ev_io_init(new_io_watcher, childreq_cb, new->readpipe[0], EV_READ);
        ev_set_priority(new_io_watcher, EV_MINPRI);
        ev_io_start(EV_A_ new_io_watcher);
        new->io_watcher = new_io_watcher;

        pid_t *pidbuf = malloc(sizeof(pid_t));
        *pidbuf = pid;

        hash_insert(child_map, pidbuf, new);
    }
}

static void init_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, NULL) < 0)
        error("sigaction");
    if(sigaction(SIGTERM, &sa, NULL) < 0)
        error("sigaction");

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL) < 0)
        error("sigaction");

    /* libev's default SIGCHLD handler exhibits some really strange
     * behavior, which we don't like, so we use our own ;) */
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sa, NULL) < 0)
        error("sigaction");
}

static SIMP_HASH(pid_t, pid_hash);
static SIMP_EQUAL(pid_t, pid_equal);

int server_main(int argc, char *argv[])
{
    debugf("*** Starting NetCosm %s (libev %d.%d, %s) ***\n",
           NETCOSM_VERSION, EV_VERSION_MAJOR, EV_VERSION_MINOR, OPENSSL_VERSION_TEXT);

    assert(ev_version_major() == EV_VERSION_MAJOR &&
           ev_version_minor() >= EV_VERSION_MINOR);

    if(argc != 2)
        port = PORT;
    else
        port = strtol(argv[1], NULL, 0);

    srand(time(0));

    server_socket = server_bind();

    userdb_init(USERFILE);

    check_userfile();

    load_worldfile();

    reqmap_init();

    /* this initial size very low to make iteration faster */
    child_map = hash_init(16, pid_hash, pid_equal);
    hash_setfreedata_cb(child_map, free_child_data);
    hash_setfreekey_cb(child_map, free);

    debugf("Listening on port %d\n", port);

    struct ev_loop *loop = EV_DEFAULT;

    /* we initialize signals after creating the default event loop
     * because libev grabs SIGCHLD */
    init_signals();

    ev_io server_watcher;
    ev_io_init(&server_watcher, new_connection_cb, server_socket, EV_READ);
    ev_set_priority(&server_watcher, EV_MAXPRI);
    ev_io_start(EV_A_ &server_watcher);

    atexit(serv_cleanup);

    ev_loop(loop, 0);

    /* should never get here */
    error("FIXME: unexpected termination");
}
