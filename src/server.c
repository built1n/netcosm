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
    const char *msg = "Client disconnect.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        hash_remove(child_map, &pid);
    }

    errno = saved_errno;

    --num_clients;
}

int port;

static void handle_client(int fd, struct sockaddr_in *addr,
                          int nclients, int to, int from)
{
    client_main(fd, addr, nclients, to, from);
}

int server_socket;

static void *request_map = NULL;

static void serv_cleanup(void)
{
    sig_printf("Shutdown server.\n");
    if(shutdown(server_socket, SHUT_RDWR) > 0)
        error("shutdown");
    close(server_socket);
    world_free();
    hash_free(request_map);
    request_map = NULL;
    hash_free(child_map);
    child_map = NULL;
    _exit(0);
}

static void sigint_handler(int s)
{
    (void) s;
    serv_cleanup();
}

static volatile sig_atomic_t num_acks_wanted, num_acks_recvd, inc_acks = 0;

static void req_pass_msg(unsigned char *data, size_t datalen,
                         struct child_data *sender, struct child_data *child)
{
    (void) sender;

    if(sender->pid != child->pid)
    {
        unsigned char cmd = REQ_BCASTMSG;
        write(child->outpipe[1], &cmd, 1);
    }

    write(child->outpipe[1], data, datalen);
    union sigval nothing;

    if(sender->pid != child->pid)
    {
        sigqueue(child->pid, SIGRTMIN, nothing);
        ++num_acks_wanted;
    }
}

static void req_send_clientinfo(unsigned char *data, size_t datalen,
                                struct child_data *sender, struct child_data *child)
{
    (void) data;
    (void) datalen;
    char buf[128];
    const char *state[] = {
        "INIT",
        "LOGIN SCREEN",
        "CHECKING CREDENTIALS",
        "LOGGED IN AS USER",
        "LOGGED IN AS ADMIN",
        "ACCESS DENIED",
    };

    if(child->user)
        snprintf(buf, sizeof(buf), "Client %s PID %d [%s] USER %s",
                 inet_ntoa(child->addr), child->pid, state[child->state], child->user);
    else
        snprintf(buf, sizeof(buf), "Client %s PID %d [%s]",
                 inet_ntoa(child->addr), child->pid, state[child->state]);

    if(sender->pid == child->pid)
        strncat(buf, " [YOU]\n", sizeof(buf));
    else
        strncat(buf, "\n", sizeof(buf));

    write(sender->outpipe[1], buf, strlen(buf));
}

static void req_change_state(unsigned char *data, size_t datalen,
                             struct child_data *sender, struct child_data *child)
{
    (void) data; (void) datalen; (void) child; (void) sender;
    if(datalen == sizeof(sender->state))
    {
        sender->state = *((int*)data);
        printf("State changed to %d\n", sender->state);
    }
    else
    {
        printf("State data is of the wrong size\n");
        for(size_t i = 0; i < datalen; ++i)
            printf("%02x\n", data[i]);
    }
}

static void req_change_user(unsigned char *data, size_t datalen,
                            struct child_data *sender, struct child_data *child)
{
    (void) data; (void) datalen; (void) child; (void) sender;
    if(sender->user)
        free(sender->user);
    sender->user = strdup((char*)data);
}

//void req_hang(unsigned char *data, size_t datalen,
//              struct child_data *sender, struct child_data *child)
//{
//    while(1);
//}

static void req_kick_client(unsigned char *data, size_t datalen,
                            struct child_data *sender, struct child_data *child)
{
    (void) data; (void) datalen; (void) child; (void) sender;
    if(datalen >= sizeof(pid_t))
    {
        pid_t kicked_pid = *((pid_t*)data);
        if(kicked_pid == child->pid)
        {
            unsigned char cmd = REQ_KICK;
            write(child->outpipe[1], &cmd, 1);
            write(child->outpipe[1], data + sizeof(pid_t), datalen - sizeof(pid_t));
            union sigval nothing;
            sigqueue(child->pid, SIGRTMIN, nothing);
        }
    }
}

static void req_wait(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data; (void) datalen; (void) sender;
    sleep(10);
}

static void req_send_desc(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data; (void) datalen; (void) sender;
    struct room_t *room = room_get(sender->room);
    write(sender->outpipe[1], room->data.desc, strlen(room->data.desc) + 1);

    char newline = '\n';
    write(sender->outpipe[1], &newline, 1);
}

static void req_send_roomname(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data; (void) datalen; (void) sender;
    struct room_t *room = room_get(sender->room);
    write(sender->outpipe[1], room->data.name, strlen(room->data.name) + 1);

    char newline = '\n';
    write(sender->outpipe[1], &newline, 1);
}

static void child_set_room(struct child_data *child, room_id id)
{
    child->room = id;
    room_user_add(id, child);
}

static void req_set_room(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data; (void) datalen; (void) sender;
    room_id id = *((room_id*)data);

    child_set_room(sender, id);
}

static void req_move_room(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data; (void) datalen; (void) sender;
    enum direction_t dir = *((enum direction_t*)data);
    struct room_t *current = room_get(sender->room);

    room_user_del(sender->room, sender);

    /* TODO: checking */
    sig_printf("Moving in direction %d\n", dir);
    room_id new = current->adjacent[dir];
    int status;
    if(new != ROOM_NONE)
    {
        child_set_room(sender, new);
        status = 1;
    }
    else
    {
        status = 0;
    }
    write(sender->outpipe[1], &status, sizeof(status));
}

static const struct child_request {
    unsigned char code;

    bool havedata;

    enum { CHILD_NONE, CHILD_SENDER, CHILD_ALL_BUT_SENDER, CHILD_ALL } which;

    /* sender_pipe is the pipe to the sender of the request */
    /* data points to bogus if havedata = false */
    void (*handle_child)(unsigned char *data, size_t len,
                         struct child_data *sender, struct child_data *child);

    void (*finalize)(unsigned char *data, size_t len, struct child_data *sender);

    /* byte to write back to the sender */
    unsigned char cmd_to_send;
} requests[] = {
    { REQ_NOP,         false, CHILD_NONE,           NULL,                NULL,              REQ_NOP      },
    { REQ_BCASTMSG,    true,  CHILD_ALL,            req_pass_msg,        NULL,              REQ_BCASTMSG },
    { REQ_LISTCLIENTS, false, CHILD_ALL,            req_send_clientinfo, NULL,              REQ_BCASTMSG },
    { REQ_CHANGESTATE, true,  CHILD_SENDER,         req_change_state,    NULL,              REQ_NOP      },
    { REQ_CHANGEUSER,  true,  CHILD_SENDER,         req_change_user,     NULL,              REQ_NOP      },
    { REQ_KICK,        true,  CHILD_ALL,            req_kick_client,     NULL,              REQ_NOP      },
    { REQ_WAIT,        false, CHILD_NONE,           NULL,                req_wait,          REQ_NOP      },
    { REQ_GETROOMDESC, false, CHILD_NONE,           NULL,                req_send_desc,     REQ_BCASTMSG },
    { REQ_GETROOMNAME, false, CHILD_NONE,           NULL,                req_send_roomname, REQ_BCASTMSG },
    { REQ_SETROOM,     true,  CHILD_NONE,           NULL,                req_set_room,      REQ_NOP      },
    { REQ_MOVE,        true,  CHILD_NONE,           NULL,                req_move_room,     REQ_MOVE     },
    //{ REQ_ROOMMSG,     true,  CHILD_ALL,            req_send_room_msg,   NULL,              REQ_BCASTMSG },
};

static SIMP_HASH(unsigned char, uchar_hash);
static SIMP_EQUAL(unsigned char, uchar_equal);

static void reqmap_init(void)
{
    request_map = hash_init(ARRAYLEN(requests), uchar_hash, uchar_equal);
    for(unsigned i = 0; i < ARRAYLEN(requests); ++i)
        hash_insert(request_map, &requests[i].code, requests + i);
}

void sig_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char buf[128];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);

    write(STDOUT_FILENO, buf, len);

    va_end(ap);
}

/**
 * Here's how child-parent requests work
 * 1. Child writes its PID and length of request to the parent's pipe, followed
 *    by up to MSG_MAX bytes of data.
 * 1.1 Child spins until parent response.
 * 2. Parent handles the request.
 * 3. Parent writes its PID and length of message back to the child(ren).
 * 4. Parent signals child(ren) with SIGRTMIN
 * 5. Child(ren) handle parent's message.
 * 6. Child(ren) send the parent SIGRTMIN+1 to acknowledge receipt of message.
 * 7. Parent spins until the needed number of signals is reached.
 */

static bool handle_child_req(int in_fd)
{
    pid_t sender_pid;
    if(read(in_fd, &sender_pid, sizeof(sender_pid)) != sizeof(pid_t))
        return false;
    sig_printf("PID %d sends a client request\n", sender_pid);

    size_t msglen;
    if(read(in_fd, &msglen, sizeof(msglen)) != sizeof(msglen))
        return false;

    if(msglen > MSG_MAX)
    {
        sig_printf("message data too long, dropping\n");
        return false;
    }
    else if(msglen < 1)
    {
        sig_printf("message too short to be valid, ignoring.\n");
        return false;
    }

    unsigned char cmd, msg[MSG_MAX + 1];
    struct child_data *sender = hash_lookup(child_map, &sender_pid);

    if(!sender)
    {
        printf("WARNING: got data from unknown PID, ignoring.\n");
        return false;
    }

    sigset_t old, block;

    sigemptyset(&block);
    sigaddset(&block, SIGRTMIN+1);
    sigprocmask(SIG_BLOCK, &block, &old);

    num_acks_wanted = 1;
    num_acks_recvd  = 0;
    inc_acks = 1;

    unsigned char *msgptr = msg;
    size_t have = 0;
    while(have < msglen)
    {
        ssize_t ret = read(sender->readpipe[0], msgptr, msglen - have);
        if(ret < 0)
        {
            sig_printf("unexpected EOF\n");
            return true;
        }
        msgptr += ret;
        have += ret;
    }

    cmd = msg[0];
    msg[MSG_MAX] = '\0';

    unsigned char *data = msg + 1;

    size_t datalen = msglen - 1;

    const struct child_request *req = hash_lookup(request_map, &cmd);

    if(!req)
    {
        sig_printf("Unknown request.\n");
        return true;
    }

    write(sender->outpipe[1], &req->cmd_to_send, 1);

    switch(req->which)
    {
    case CHILD_SENDER:
    case CHILD_ALL:
        req->handle_child(data, datalen, sender, sender);
        if(req->which == CHILD_SENDER)
            goto finish;
        break;
    case CHILD_NONE:
        goto finish;
    default:
        break;
    }

    struct child_data *child = NULL;
    void *ptr = child_map, *save;

    do {
        pid_t *key;
        child = hash_iterate(ptr, &save, (void**)&key);
        ptr = NULL;
        if(!child)
            break;
        sig_printf("Iterating over PID %d\n", *key);
        if(child->pid == sender->pid)
            continue;

        switch(req->which)
        {
        case CHILD_ALL:
        case CHILD_ALL_BUT_SENDER:
            req->handle_child(data, datalen, sender, child);
            break;
        default:
            break;
        }
    } while(1);

finish:

    if(req && req->finalize)
        req->finalize(data, datalen, sender);

    union sigval junk;
    sigqueue(sender->pid, SIGRTMIN, junk);

    sig_printf("Waiting for %d acks\n", num_acks_wanted);

    while(num_acks_recvd < num_acks_wanted)
    {
        sigsuspend(&old);
        sig_printf("Got %d total acks\n", num_acks_recvd);
    }

    inc_acks = 0;

    sigprocmask(SIG_SETMASK, &old, NULL);

    sig_printf("finished handling client request\n");

    return true;
}

static void master_ack_handler(int s, siginfo_t *info, void *v)
{
    (void) s;
    (void) v;
    sig_printf("Parent gets ACK\n");
    if(inc_acks && hash_lookup(child_map, &info->si_pid))
    {
        ++num_acks_recvd;
        sig_printf("%d acks now\n", num_acks_recvd);
    }
}

void init_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigchld_handler; // reap all dead processes
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGCHLD, &sa, NULL) < 0)
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
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGPIPE, &sa, NULL) < 0)
        error("sigaction");

    /* set this now so there's no race condition later */
    sigemptyset(&sa.sa_mask);
    void sig_rt_0_handler(int s, siginfo_t *info, void *v);
    sa.sa_sigaction = sig_rt_0_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if(sigaction(SIGRTMIN, &sa, NULL) < 0)
        error("sigaction");
}

static void check_userfile(void)
{
    if(access(USERFILE, F_OK) < 0)
        first_run_setup();

    if(access(USERFILE, R_OK | W_OK) < 0)
        error("cannot access "USERFILE);
}

/* "raw" world data, provided by the world module */
extern const struct roomdata_t netcosm_world[];
extern const size_t netcosm_world_sz;

static void load_worldfile(void)
{
    if(access(WORLDFILE, F_OK) < 0)
    {
        world_init(netcosm_world, netcosm_world_sz);

        world_save(WORLDFILE);
    }
    else if(access(WORLDFILE, R_OK | W_OK) < 0)
        error("cannot access "WORLDFILE);
    else
        world_load(WORLDFILE, netcosm_world, netcosm_world_sz);
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

    check_userfile();

    load_worldfile();

    reqmap_init();

    /* this is set very low to make iteration faster */
    child_map = hash_init(11, pid_hash, pid_equal);
    hash_setfreedata_cb(child_map, free_child_data);
    hash_setfreekey_cb(child_map, free);

    printf("Listening on port %d\n", port);

    fd_set read_fds, active_fds;
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
                    if(pipe(outpipe) < 0)
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
                        hash_free(request_map);
                        request_map = NULL;
                        hash_free(child_map);
                        child_map = NULL;

                        printf("Child with PID %d spawned\n", getpid());

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
                    handle_child_req(i);
                }
            }
        }
    }
}
