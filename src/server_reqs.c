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

#include "hash.h"
#include "server.h"
#include "userdb.h"

static void send_packet(struct child_data *child, unsigned char cmd,
                        void *data, size_t datalen)
{
    assert(datalen < MSG_MAX);
    unsigned char pkt[MSG_MAX];
    pkt[0] = cmd;
    if(datalen)
        memcpy(pkt + 1, data, datalen);
tryagain:
    if(write(child->outpipe[1], pkt, datalen + 1) < 0)
    {
        /* write can fail, so we try again */
        if(errno == EAGAIN)
            goto tryagain;
    }
}

static void req_pass_msg(unsigned char *data, size_t datalen,
                         struct child_data *sender, struct child_data *child)
{
    (void) sender;

    send_packet(child, REQ_BCASTMSG, data, datalen);
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

    send_packet(sender, REQ_BCASTMSG, buf, strlen(buf));
}

static void req_change_state(unsigned char *data, size_t datalen,
                             struct child_data *sender, struct child_data *child)
{
    (void) data; (void) datalen; (void) child; (void) sender;
    if(datalen == sizeof(sender->state))
        sender->state = *((int*)data);
    else
        debugf("State data is of the wrong size %*s\n", datalen, data);
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
    /* format is | PID | Message | */
    (void) data; (void) datalen; (void) child; (void) sender;
    if(datalen >= sizeof(pid_t))
    {
        pid_t kicked_pid = *((pid_t*)data);
        if(kicked_pid == child->pid)
            send_packet(child, REQ_BCASTMSG, data + sizeof(pid_t), datalen - sizeof(pid_t));
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
    send_packet(sender, REQ_BCASTMSG, room->data.desc, strlen(room->data.desc));

    send_packet(sender, REQ_PRINTNEWLINE, NULL, 0);
}

static void req_send_roomname(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data; (void) datalen; (void) sender;
    struct room_t *room = room_get(sender->room);
    send_packet(sender, REQ_BCASTMSG, room->data.name, strlen(room->data.name));

    send_packet(sender, REQ_PRINTNEWLINE, NULL, 0);
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
    debugf("Moving in direction %d\n", dir);
    room_id new = current->adjacent[dir];
    int status = 0;
    if(new != ROOM_NONE)
    {
        child_set_room(sender, new);
        status = 1;
    }
    debugf("server status: %d\n", status);

    send_packet(sender, REQ_MOVE, &status, sizeof(status));
}

static void req_send_user(unsigned char *data, size_t datalen, struct child_data *sender)
{
    if(datalen)
    {
        struct userdata_t *user = userdb_lookup((char*)data);

        if(user)
        {
            send_packet(sender, REQ_GETUSERDATA, user, sizeof(*user));
            return;
        }

        debugf("looking up user %s failed\n", data);
        debugf("failure 2\n");
    }

    debugf("failure 1\n");
}

static void req_del_user(unsigned char *data, size_t datalen, struct child_data *sender)
{
    bool success = false;
    if(datalen)
    {
        success = userdb_remove((char*)data);
    }
    send_packet(sender, REQ_DELUSERDATA, &success, sizeof(success));
}

static void req_add_user(unsigned char *data, size_t datalen, struct child_data *sender)
{
    bool success = false;
    if(datalen == sizeof(struct userdata_t))
    {
        success = userdb_add((struct userdata_t*)data);
    }
    send_packet(sender, REQ_ADDUSERDATA, &success, sizeof(success));
}

static void req_send_geninfo(unsigned char *data, size_t datalen, struct child_data *sender)
{
    (void) data;
    (void) datalen;
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "Total clients: %d\n", num_clients);
    send_packet(sender, REQ_BCASTMSG, buf, len);
}

static void req_kick_always(unsigned char *data, size_t datalen,
                            struct child_data *sender, struct child_data *child)
{
    (void) sender;
    send_packet(child, REQ_KICK, data, datalen);
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
} requests[] = {
    { REQ_NOP,         false, CHILD_NONE,           NULL,                NULL,              },
    { REQ_BCASTMSG,    true,  CHILD_ALL,            req_pass_msg,        NULL,              },
    { REQ_LISTCLIENTS, false, CHILD_ALL,            req_send_clientinfo, req_send_geninfo,  },
    { REQ_CHANGESTATE, true,  CHILD_SENDER,         req_change_state,    NULL,              },
    { REQ_CHANGEUSER,  true,  CHILD_SENDER,         req_change_user,     NULL,              },
    { REQ_KICK,        true,  CHILD_ALL,            req_kick_client,     NULL,              },
    { REQ_WAIT,        false, CHILD_NONE,           NULL,                req_wait,          },
    { REQ_GETROOMDESC, false, CHILD_NONE,           NULL,                req_send_desc,     },
    { REQ_GETROOMNAME, false, CHILD_NONE,           NULL,                req_send_roomname, },
    { REQ_SETROOM,     true,  CHILD_NONE,           NULL,                req_set_room,      },
    { REQ_MOVE,        true,  CHILD_NONE,           NULL,                req_move_room,     },
    { REQ_GETUSERDATA, true,  CHILD_NONE,           NULL,                req_send_user,     },
    { REQ_DELUSERDATA, true,  CHILD_NONE,           NULL,                req_del_user,      },
    { REQ_ADDUSERDATA, true,  CHILD_NONE,           NULL,                req_add_user,      },
    { REQ_KICKALL,     true,  CHILD_ALL_BUT_SENDER, req_kick_always,     NULL               },
    //{ REQ_ROOMMSG,     true,  CHILD_ALL,            req_send_room_msg,   NULL,           },
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
    unsigned char packet[MSG_MAX + 1];

    ssize_t packet_len = read(in_fd, packet, MSG_MAX);

    struct child_data *sender = NULL;

    if(packet_len <= 0)
        goto fail;

    pid_t sender_pid;
    memcpy(&sender_pid, packet, sizeof(pid_t));
    debugf("servreq: Got request from PID %d\n", sender_pid);

    sender = hash_lookup(child_map, &sender_pid);

    if(!sender)
    {
        debugf("WARNING: got data from unknown PID, ignoring.\n");
        goto fail;
    }

    unsigned char cmd = packet[sizeof(pid_t)];

    unsigned char *data = packet + sizeof(pid_t) + 1;
    size_t datalen = packet_len - sizeof(pid_t) - 1;

    struct child_request *req = hash_lookup(request_map, &cmd);

    if(!req)
    {
        debugf("Unknown request.\n");
        goto fail;
    }

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

        debugf("iterating over child %d\n", child->pid);

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

    debugf("finalizing request\n");

    if(req && req->finalize)
        req->finalize(data, datalen, sender);

    /* fall through */
fail:
    if(sender)
    {
        send_packet(sender, REQ_ALLDONE, NULL, 0);

        debugf("sending all done code\n");
    }

    return true;
}
