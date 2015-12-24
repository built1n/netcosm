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

static struct child_data {
    pid_t pid;
    int readpipe[2];
    int outpipe[2];

    int state;
    room_id room;
    char *user;

    struct in_addr addr;

    /* a linked list works well for this because no random-access is needed */
    struct child_data *next;
} *child_data;

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
        struct child_data *iter = child_data, *last = NULL;
        while(iter)
        {
            if(iter->pid == pid)
            {
                if(!last)
                    child_data = iter->next;
                else
                    last->next = iter->next;
                free(iter);
                break;
            }
            iter = iter->next;
        }
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

static void serv_cleanup(void)
{
    write(STDOUT_FILENO, "Shutdown server.\n", strlen("Shutdown server.\n"));
    if(shutdown(server_socket, SHUT_RDWR) > 0)
        error("shutdown");
    close(server_socket);
}

static void sigint_handler(int s)
{
    (void) s;
    serv_cleanup();
}

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
    kill(child->pid, SIGUSR2);
}

static void req_send_clientinfo(unsigned char *data, size_t datalen,
                                struct child_data *sender, struct child_data *child)
{
    (void) data;
    (void) datalen;
    char buf[128];
    int len;
    const char *state[] = {
        "INIT",
        "LOGIN SCREEN",
        "CHECKING CREDENTIALS",
        "LOGGED IN AS USER",
        "LOGGED IN AS ADMIN",
        "ACCESS DENIED",
    };

    if(child->user)
        len = snprintf(buf, sizeof(buf), "Client %s PID %d [%s] USER %s\n",
                       inet_ntoa(child->addr), child->pid, state[child->state], child->user);
    else
        len = snprintf(buf, sizeof(buf), "Client %s PID %d [%s]\n",
                       inet_ntoa(child->addr), child->pid, state[child->state]);

    write(sender->outpipe[1], buf, len);
}

static void req_change_state(unsigned char *data, size_t datalen,
                             struct child_data *sender, struct child_data *child)
{
    (void) child;
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
    (void) data;
    (void) datalen;
    (void) child;
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
    (void) sender;
    if(datalen >= sizeof(pid_t))
    {
        pid_t kicked_pid = *((pid_t*)data);
        if(kicked_pid == child->pid)
        {
            unsigned char cmd = REQ_KICK;
            write(child->outpipe[1], &cmd, 1);
            write(child->outpipe[1], data + sizeof(pid_t), datalen - sizeof(pid_t));
            kill(child->pid, SIGUSR2);
        }
    }
}

static void req_wait(unsigned char *data, size_t len, struct child_data *sender)
{
    sleep(10);
}

static void req_send_desc(unsigned char *data, size_t len, struct child_data *sender)
{
    struct room_t *room = room_get(sender->room);
    write(sender->outpipe[1], room->data.desc, strlen(room->data.desc) + 1);

    char newline = '\n';
    write(sender->outpipe[1], &newline, 1);
}

static void req_set_room(unsigned char *data, size_t len, struct child_data *sender)
{
    room_id id = *((room_id*)data);

    sender->room = id;
}

static void req_move_room(unsigned char *data, size_t len, struct child_data *sender)
{
    enum direction_t dir = *((enum direction_t*)data);
    struct room_t *current = room_get(sender->room);

    /* TODO: checking */
    room_id new = current->adjacent[dir];
    int status;
    if(new != ROOM_NONE)
    {
        sender->room = new;
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
    { REQ_LOOK,        false, CHILD_NONE,           NULL,                req_send_desc,     REQ_BCASTMSG },
    { REQ_SETROOM,     true,  CHILD_NONE,           NULL,                req_set_room,      REQ_NOP      },
    { REQ_MOVE,        true,  CHILD_NONE,           NULL,                req_move_room,     REQ_MOVE     },
};

void sig_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char buf[128];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);

    write(STDOUT_FILENO, buf, len);

    va_end(ap);
}

/* SIGUSR1 is used by children to communicate with the master process */
/* the master handles commands that involve multiple children, i.e. message passing, listing clients, etc. */
static void sigusr1_handler(int s, siginfo_t *info, void *vp)
{
    (void) s;
    (void) vp;
    pid_t sender_pid = info->si_pid;
    sig_printf("PID %d sends a client request\n", sender_pid);

    unsigned char cmd, data[MSG_MAX + 1];
    const struct child_request *req = NULL;
    size_t datalen = 0;
    struct child_data *sender = NULL;

    /* we have to iterate over the linked list twice */
    /* first to get the data, second to send it to the rest of the children */

    struct child_data *iter = child_data;

    while(iter)
    {
        if(!req)
        {
            if(iter->pid == sender_pid)
            {
                sender = iter;
                read(iter->readpipe[0], &cmd, 1);
                for(unsigned int i = 0; i < ARRAYLEN(requests); ++i)
                {
                    if(cmd == requests[i].code)
                    {
                        req = requests + i;
                        break;
                    }
                }
                if(!req)
                {
                    sig_printf("Unknown request.\n");
                    return;
                }

                sig_printf("Got command %d\n", cmd);
                if(req->havedata)
                {
                    datalen = read(iter->readpipe[0], data, sizeof(data));
                }

                write(sender->outpipe[1], &req->cmd_to_send, 1);

                switch(req->which)
                {
                case CHILD_SENDER:
                case CHILD_ALL:
                    req->handle_child(data, datalen, sender, iter);
                    if(req->which == CHILD_SENDER)
                        goto finish;
                    break;
                case CHILD_NONE:
                    goto finish;
                default:
                    break;
                }
            }
        }
        else
        {
            switch(req->which)
            {
            case CHILD_ALL:
            case CHILD_ALL_BUT_SENDER:
                req->handle_child(data, datalen, sender, iter);
                break;
            default:
                break;
            }
        }
        iter = iter->next;
    }

    /* iterate over the rest of the children, if needed */
    if(req && req->which != CHILD_SENDER)
    {
        iter = child_data;
        while(iter)
        {
            if(iter->pid == sender_pid)
                break;

            req->handle_child(data, datalen, sender, iter);

            iter = iter->next;
        }
    }

finish:

    if(req && req->finalize)
        req->finalize(data, datalen, sender);

    if(sender)
        kill(sender->pid, SIGUSR2);
    else
        sig_printf("WARN: unknown child sent request.\n");

    if(!req)
        sig_printf("WARN: unknown request, request ignored\n");

    sig_printf("finished handling client request\n");
}

void init_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);

    sa.sa_sigaction = sigchld_handler; // reap all dead processes
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGCHLD, &sa, NULL) < 0)
        error("sigaction");

    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, NULL) < 0)
        error("sigaction");
    if(sigaction(SIGTERM, &sa, NULL) < 0)
        error("sigaction");

    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_sigaction = sigusr1_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if(sigaction(SIGUSR1, &sa, NULL) < 0)
        error("sigaction");
}

static void check_userfile(void)
{
    if(access(USERFILE, F_OK) < 0)
        first_run_setup();

    if(access(USERFILE, R_OK | W_OK) < 0)
        error("cannot access "USERFILE);
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

int main(int argc, char *argv[])
{
    if(argc != 2)
        port = PORT;
    else
        port = strtol(argv[1], NULL, 0);

    srand(time(0));

    server_socket = server_bind();

    /* set up signal handlers for SIGCHLD, SIGUSR1, and SIGINT */
    /* SIGUSR1 is used for broadcast signaling */
    init_signals();

    check_userfile();

    world_init(netcosm_world, netcosm_world_sz);

    child_data = NULL;

    printf("Listening on port %d\n", port);

    while(1)
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

            printf("Child with PID %d spawned\n", getpid());

            server_socket = new_sock;

            handle_client(new_sock, &client, num_clients, readpipe[1], outpipe[0]);

            exit(0);
        }
        else
        {
            close(readpipe[1]);
            close(outpipe[0]);
            close(new_sock);

            /* add the child to the child list */
            struct child_data *old = child_data;
            child_data = malloc(sizeof(struct child_data));
            memcpy(child_data->outpipe, outpipe, sizeof(outpipe));
            memcpy(child_data->readpipe, readpipe, sizeof(readpipe));
            child_data->addr = client.sin_addr;
            child_data->next = old;
            child_data->pid = pid;
            child_data->state = STATE_INIT;
            child_data->user = NULL;
        }
    }
}
