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

#include "netcosm.h"

#define PORT 1234
#define BACKLOG 16

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

/* assume int is atomic */
volatile int num_clients = 0;
void *child_map = NULL;
static fd_set read_fds, active_fds;

static void free_child_data(void *ptr)
{
    struct child_data *child = ptr;
    if(child->user)
        free(child->user);
    free(ptr);
}

static void sigchld_handler(int s, siginfo_t *info, void *vp)
{
    (void) s;
    (void) info;
    (void) vp;
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        sig_debugf("Client disconnect.\n");
        struct child_data *child = hash_lookup(child_map, &pid);
        FD_CLR(child->readpipe[0], &active_fds);

        --num_clients;

        hash_remove(child_map, &pid);
    }

    errno = saved_errno;
}

int port;

static void handle_client(int fd, struct sockaddr_in *addr,
                          int nclients, int to, int from)
{
    client_main(fd, addr, nclients, to, from);
}

int server_socket;

static void serv_cleanup(void)
{
    sig_debugf("Shutdown server.\n");

    if(shutdown(server_socket, SHUT_RDWR) > 0)
        error("shutdown");
    close(server_socket);

    world_free();

    reqmap_free();

    hash_free(child_map);
    child_map = NULL;

    userdb_shutdown();

    _exit(0);
}

static void sigint_handler(int s)
{
    (void) s;
    serv_cleanup();
}

void init_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sa.sa_sigaction = sigchld_handler; // reap all dead processes
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if(sigaction(SIGCHLD, &sa, NULL) < 0)
        error("sigaction");

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, NULL) < 0)
        error("sigaction");
    if(sigaction(SIGTERM, &sa, NULL) < 0)
        error("sigaction");

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGRTMIN+1);
    sa.sa_sigaction = master_ack_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if(sigaction(SIGRTMIN+1, &sa, NULL) < 0)
        error("sigaction");

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGPIPE, &sa, NULL) < 0)
        error("sigaction");

    void sig_rt_0_handler(int s, siginfo_t *info, void *v);

    /* we set this now so there's no race condition after a fork() */
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGRTMIN);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sa.sa_sigaction = sig_rt_0_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if(sigaction(SIGRTMIN, &sa, NULL) < 0)
        error("sigaction");
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
    if(access(WORLDFILE, F_OK) < 0)
    {
        world_init(netcosm_world, netcosm_world_sz, netcosm_world_name);

        world_save(WORLDFILE);
    }
    else if(access(WORLDFILE, R_OK | W_OK) < 0)
        error("cannot access "WORLDFILE);
    else
        if(!world_load(WORLDFILE, netcosm_world, netcosm_world_sz, netcosm_world_name))
            error("Failed to load world from disk.\nTry removing "WORLDFILE".\n");
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

static SIMP_HASH(pid_t, pid_hash);
static SIMP_EQUAL(pid_t, pid_equal);

int main(int argc, char *argv[])
{
    if(argc != 2)
        port = PORT;
    else
        port = strtol(argv[1], NULL, 0);

    srand(time(0));

    server_socket = server_bind();

    /* set up signal handlers */
    /* SIGRTMIN+0 is used for broadcast signaling */
    init_signals();

    userdb_init(USERFILE);

    check_userfile();

    load_worldfile();

    reqmap_init();

    /* this initial size very low to make iteration faster */
    child_map = hash_init(7, pid_hash, pid_equal);
    hash_setfreedata_cb(child_map, free_child_data);
    hash_setfreekey_cb(child_map, free);

    debugf("Listening on port %d\n", port);

    FD_ZERO(&active_fds);
    FD_SET(server_socket, &active_fds);

    while(1)
    {
        read_fds = active_fds;
        int num_events;
        sigset_t all, old;
        sigemptyset(&all);
        sigaddset(&all, SIGPIPE);
        sigprocmask(SIG_SETMASK, &all, &old);
        do {
            num_events = select(FD_SETSIZE, &read_fds, NULL, NULL, NULL);
        } while (num_events < 0);

        sigprocmask(SIG_SETMASK, &old, NULL);
        //if(num_events < 0)
        //    error("select");

        for(int i = 0; i < FD_SETSIZE; ++i)
        {
            if(FD_ISSET(i, &read_fds))
            {
                if(server_socket == i)
                {
                    struct sockaddr_in client;
                    socklen_t client_len = sizeof(client);
                    int new_sock = accept(server_socket, (struct sockaddr*) &client, &client_len);
                    if(new_sock < 0)
                        error("accept");

                    ++num_clients;

                    int readpipe[2]; /* child->parent */
                    int outpipe [2]; /* parent->child */

                    if(pipe(readpipe) < 0)
                        error("pipe");
                    if(pipe2(outpipe, O_NONBLOCK) < 0)
                        error("pipe");

                    pid_t pid = fork();
                    if(pid < 0)
                        error("fork");

                    if(!pid)
                    {
                        /* child */
                        close(readpipe[0]);
                        close(outpipe[1]);
                        close(server_socket);

                        /* only the master process controls the world */
                        world_free();
                        reqmap_free();
                        hash_free(child_map);
                        child_map = NULL;

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
                        FD_SET(new->readpipe[0], &active_fds);
                        new->addr = client.sin_addr;
                        new->pid = pid;
                        new->state = STATE_INIT;
                        new->user = NULL;

                        pid_t *pidbuf = malloc(sizeof(pid_t));
                        *pidbuf = pid;
                        hash_insert(child_map, pidbuf, new);
                    }
                }
                else
                {
                    /* data from a child's pipe */
                    if(!handle_child_req(i))
                        FD_CLR(i, &active_fds);
                }
            }
        }
    }
}
