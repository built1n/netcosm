#include "globals.h"

#include "hash.h"
#include "server.h"
#include "userdb.h"

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
        strncat(buf, " [YOU]\n", sizeof(buf) - 1);
    else
        strncat(buf, "\n", sizeof(buf) - 1);

    write(sender->outpipe[1], buf, strlen(buf));
}

static void req_change_state(unsigned char *data, size_t datalen,
                             struct child_data *sender, struct child_data *child)
{
    (void) data; (void) datalen; (void) child; (void) sender;
    if(datalen == sizeof(sender->state))
    {
        sender->state = *((int*)data);
        debugf("State changed to %d\n", sender->state);
    }
    else
    {
        debugf("State data is of the wrong size\n");
        for(size_t i = 0; i < datalen; ++i)
            debugf("%02x\n", data[i]);
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
    sig_debugf("Moving in direction %d\n", dir);
    room_id new = current->adjacent[dir];
    bool status;
    if(new != ROOM_NONE)
    {
        child_set_room(sender, new);
        status = true;
    }
    else
    {
        status = false;
    }
    write(sender->outpipe[1], &status, sizeof(status));
}

static void req_send_user(unsigned char *data, size_t datalen, struct child_data *sender)
{
    if(datalen)
    {
        struct userdata_t *user = userdb_lookup((char*)data);

        if(user)
        {
            bool confirm = true;
            write(sender->outpipe[1], &confirm, sizeof(confirm));
            write(sender->outpipe[1], user, sizeof(*user));
            return;
        }

        sig_debugf("failure 2\n");
    }

    sig_debugf("failure 1\n");

    bool fail = false;
    write(sender->outpipe[1], &fail, sizeof(fail));
}

static void req_del_user(unsigned char *data, size_t datalen, struct child_data *sender)
{
    bool success = false;
    if(datalen)
    {
        success = userdb_remove((char*)data);
    }
    write(sender->outpipe[1], &success, sizeof(success));
}

static void req_add_user(unsigned char *data, size_t datalen, struct child_data *sender)
{
    bool success = false;
    if(datalen == sizeof(struct userdata_t))
    {
        success = userdb_add((struct userdata_t*)data);
    }
    write(sender->outpipe[1], &success, sizeof(success));
}

static void req_send_geninfo(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data;
    (void) datalen;
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "Total clients: %d\n", num_clients);
    write(sender->outpipe[1], buf, len);
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
    { REQ_NOP,         false, CHILD_NONE,           NULL,                NULL,              REQ_NOP         },
    { REQ_BCASTMSG,    true,  CHILD_ALL,            req_pass_msg,        NULL,              REQ_BCASTMSG    },
    { REQ_LISTCLIENTS, false, CHILD_ALL,            req_send_clientinfo, req_send_geninfo,  REQ_BCASTMSG    },
    { REQ_CHANGESTATE, true,  CHILD_SENDER,         req_change_state,    NULL,              REQ_NOP         },
    { REQ_CHANGEUSER,  true,  CHILD_SENDER,         req_change_user,     NULL,              REQ_NOP         },
    { REQ_KICK,        true,  CHILD_ALL,            req_kick_client,     NULL,              REQ_NOP         },
    { REQ_WAIT,        false, CHILD_NONE,           NULL,                req_wait,          REQ_NOP         },
    { REQ_GETROOMDESC, false, CHILD_NONE,           NULL,                req_send_desc,     REQ_BCASTMSG    },
    { REQ_GETROOMNAME, false, CHILD_NONE,           NULL,                req_send_roomname, REQ_BCASTMSG    },
    { REQ_SETROOM,     true,  CHILD_NONE,           NULL,                req_set_room,      REQ_NOP         },
    { REQ_MOVE,        true,  CHILD_NONE,           NULL,                req_move_room,     REQ_MOVE        },
    { REQ_GETUSERDATA, true,  CHILD_NONE,           NULL,                req_send_user,     REQ_GETUSERDATA },
    { REQ_DELUSERDATA, true,  CHILD_NONE,           NULL,                req_del_user,      REQ_DELUSERDATA },
    { REQ_ADDUSERDATA, true,  CHILD_NONE,           NULL,                req_add_user,      REQ_ADDUSERDATA },
    //{ REQ_ROOMMSG,     true,  CHILD_ALL,            req_send_room_msg,   NULL,              REQ_BCASTMSG },
};

static SIMP_HASH(unsigned char, uchar_hash);
static SIMP_EQUAL(unsigned char, uchar_equal);

static void *request_map = NULL;

void reqmap_init(void)
{
    request_map = hash_init(ARRAYLEN(requests), uchar_hash, uchar_equal);
    for(unsigned i = 0; i < ARRAYLEN(requests); ++i)
        hash_insert(request_map, &requests[i].code, requests + i);
}

void reqmap_free(void)
{
    if(request_map)
    {
        hash_free(request_map);
        request_map = NULL;
    }
}

/**
 * Here's how child-parent requests work
 * 1. Child writes its PID and length of request to the parent's pipe, followed
 *    by up to MSG_MAX bytes of data. If the length exceeds MSG_MAX bytes, the
 *    request will be ignored.
 * 1.1 Child spins until parent response.
 * 2. Parent handles the request.
 * 3. Parent writes its PID and length of message back to the child(ren).
 * 4. Parent signals child(ren) with SIGRTMIN
 * 5. Child(ren) handle parent's message.
 * 6. Child(ren) send the parent SIGRTMIN+1 to acknowledge receipt of message.
 * 7. Parent spins until the needed number of signals is reached.
 */

bool handle_child_req(int in_fd)
{
    pid_t sender_pid;

    if(read(in_fd, &sender_pid, sizeof(sender_pid)) != sizeof(sender_pid))
    {
        sig_debugf("Couldn't get sender PID\n");
        return false;
    }

    sig_debugf("Got request from PID %d\n", sender_pid);

    size_t msglen;
    const struct child_request *req = NULL;
    size_t datalen;

    unsigned char cmd, msg[MSG_MAX + 1];

    struct child_data *sender = hash_lookup(child_map, &sender_pid);

    if(!sender)
    {
        sig_debugf("WARNING: got data from unknown PID, ignoring.\n");
        goto fail;
    }

    if(read(in_fd, &msglen, sizeof(msglen)) != sizeof(msglen))
    {
        sig_debugf("Couldn't read message length, dropping.\n");
        goto fail;
    }

    if(msglen < 1)
    {
        sig_debugf("message too short to be valid, ignoring.\n");
        goto fail;
    }
    else if(msglen > MSG_MAX)
    {
        sig_debugf("message too long, ignoring.\n");
        goto fail;
    }

    unsigned char *msgptr = msg;
    size_t have = 0;
    while(have < msglen)
    {
        ssize_t ret = read(sender->readpipe[0], msgptr, msglen - have);
        if(ret < 0)
        {
            sig_debugf("unexpected EOF\n");
            goto fail;
        }
        msgptr += ret;
        have += ret;
    }

    cmd = msg[0];
    msg[MSG_MAX] = '\0';

    unsigned char *data = msg + 1;

    datalen = msglen - 1;
    req = hash_lookup(request_map, &cmd);

    sigset_t old, block;

    sigemptyset(&block);
    sigaddset(&block, SIGRTMIN+1);
    sigprocmask(SIG_BLOCK, &block, &old);

    num_acks_wanted = 1;
    num_acks_recvd  = 0;
    inc_acks = 1;

    if(!req)
    {
        sig_debugf("Unknown request.\n");
        goto fail;
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
    if(sender)
        sigqueue(sender->pid, SIGRTMIN, junk);
    else
        sig_debugf("Unknown PID sent request.\n");

    /* 5 ms */
#define ACK_TIMEOUT 5000

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = ACK_TIMEOUT;

    while(num_acks_recvd < MIN(num_clients,num_acks_wanted))
    {
        if(sigtimedwait(&old, NULL, &timeout) < 0 && errno == EAGAIN)
            break;
    }

    inc_acks = 0;

    sigprocmask(SIG_SETMASK, &old, NULL);

    return true;
fail:
    return true;
}

void master_ack_handler(int s, siginfo_t *info, void *v)
{
    (void) s;
    (void) v;
    if(inc_acks && hash_lookup(child_map, &info->si_pid))
    {
        ++num_acks_recvd;
    }
}
