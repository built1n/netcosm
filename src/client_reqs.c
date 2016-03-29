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
#include "client_reqs.h"
#include "hash.h"

enum reqdata_typespec reqdata_type = TYPE_NONE;
union reqdata_t returned_reqdata;

static int request_complete;

/* for rate-limiting */
static int reqs_since_ts;
static time_t ts = 0;

bool poll_requests(void)
{
    if(!are_child)
        return false;

    bool got_cmd = false;

    while(1)
    {
        unsigned char packet[MSG_MAX + 1];
        memset(packet, 0, sizeof(packet));

        ssize_t packetlen = read(from_parent, packet, MSG_MAX);

        unsigned char *data = packet + 1;
        size_t datalen = packetlen - 1;
        packet[MSG_MAX] = '\0';

        /* no data yet */
        if(packetlen < 0)
            goto fail;

        /* parent closed pipe */
        if(!packetlen)
        {
            debugf("master process died\n");
            exit(0);
        }

        got_cmd = true;

        unsigned char cmd = packet[0];

        switch(cmd)
        {
        case REQ_BCASTMSG:
        {
            out((char*)data, datalen);
            break;
        }
        case REQ_KICK:

        {
            out((char*)data, datalen);
            exit(EXIT_SUCCESS);
        }
        case REQ_MOVE:
        {
            int status = *((int*)data);

            reqdata_type = TYPE_BOOLEAN;
            returned_reqdata.boolean = (status == 1);
            break;
        }
        case REQ_GETUSERDATA:
        {
            if(datalen == sizeof(struct userdata_t))
                reqdata_type = TYPE_USERDATA;
            else
                break;

            struct userdata_t *user = &returned_reqdata.userdata;
            *user = *((struct userdata_t*)data);
            break;
        }
        case REQ_DELUSERDATA:
        {
            reqdata_type = TYPE_BOOLEAN;
            returned_reqdata.boolean = *((bool*)data);
            break;
        }
        case REQ_ADDUSERDATA:
        {
            reqdata_type = TYPE_BOOLEAN;
            returned_reqdata.boolean = *((bool*)data);
            break;
        }
        case REQ_NOP:
            break;
        case REQ_PRINTNEWLINE:
        {
            out("\n");
            break;
        }
        case REQ_ALLDONE:
            request_complete = 1;
            return true;
        default:
            debugf("WARNING: client process received unknown code %d\n", cmd);
            break;
        }
    }
fail:

    return got_cmd;
}

void client_change_state(int state)
{
    send_master(REQ_CHANGESTATE, &state, sizeof(state));
}

void client_change_user(const char *user)
{
    send_master(REQ_CHANGEUSER, user, strlen(user) + 1);
}

void client_change_room(room_id id)
{
    send_master(REQ_SETROOM, &id, sizeof(id));
}

void send_master(unsigned char cmd, const void *data, size_t sz)
{
    if(!are_admin)
    {
        time_t t = time(NULL);
        if(ts != t)
        {
            ts = t;
            reqs_since_ts = 0;
        }
        if(reqs_since_ts++ > 10)
        {
            out("Rate limit exceeded.\n");
            return;
        }
    }

    request_complete = 0;

    pid_t our_pid = getpid();

    if(!data)
        sz = 0;

    /*
     * format of child->parent packets:
     * | PID | CMD | DATA |
     */

    /* pack it all into one write so it's atomic */
    char *req = malloc(sizeof(pid_t) + 1 + sz);

    memcpy(req, &our_pid, sizeof(pid_t));
    memcpy(req + sizeof(pid_t), &cmd, 1);
    if(data)
        memcpy(req + sizeof(pid_t) + 1, data, sz);

    assert(1 + sizeof(pid_t) + sz <= MSG_MAX);
    write(to_parent, req, 1 + sizeof(pid_t) + sz);

    /* poll till we get data */
    struct pollfd pfd;
    pfd.fd = from_parent;
    pfd.events = POLLIN;

    poll(&pfd, 1, -1);

    while(!request_complete) poll_requests();

    free(req);
}

/* freed by server_cleanup */
void *dir_map = NULL;

bool client_move(const char *dir)
{
    const struct dir_pair {
        const char *text;
        enum direction_t val;
    } dirs[] = {
        {  "N",          DIR_N     },
        {  "NORTH",      DIR_N     },
        {  "NE",         DIR_NE    },
        {  "NORTHEAST",  DIR_N     },
        {  "E",          DIR_E     },
        {  "EAST",       DIR_E     },
        {  "SE",         DIR_SE    },
        {  "SOUTHEAST",  DIR_SE    },
        {  "S",          DIR_S     },
        {  "SOUTH",      DIR_S     },
        {  "SW",         DIR_SW    },
        {  "SOUTHWEST",  DIR_SW    },
        {  "W",          DIR_W     },
        {  "WEST",       DIR_W     },
        {  "NW",         DIR_NW    },
        {  "NORTHWEST",  DIR_NW    },
        {  "U",          DIR_UP    },
        {  "UP",         DIR_UP    },
        {  "D",          DIR_DN    },
        {  "DOWN",       DIR_DN    },
        {  "IN",         DIR_IN    },
        {  "OUT",        DIR_OT    },
    };

    if(!dir_map)
    {
        dir_map = hash_init(ARRAYLEN(dirs), hash_djb, compare_strings);
        hash_insert_pairs(dir_map, (struct hash_pair*)dirs, sizeof(struct dir_pair), ARRAYLEN(dirs));
    }

    struct dir_pair *pair = hash_lookup(dir_map, dir);
    if(pair)
    {
        send_master(REQ_MOVE, &pair->val, sizeof(pair->val));
        if(reqdata_type == TYPE_BOOLEAN && returned_reqdata.boolean)
            return true;
        else
            return false;
    }
    else
    {
        out("Unknown direction.\n");
        return false;
    }
}

void client_look(void)
{
    send_master(REQ_GETROOMNAME, NULL, 0);
    out("\n");
    send_master(REQ_GETROOMDESC, NULL, 0);
}

void client_look_at(char *obj)
{
    all_lower(obj);
    send_master(REQ_LOOKAT, obj, strlen(obj) + 1);
}

void client_take(char *obj)
{
    if(obj)
    {
	all_lower(obj);
	send_master(REQ_TAKE, obj, strlen(obj) + 1);
    }
    else
	out("You must supply an object.\n");
}

void client_inventory(void)
{
    send_master(REQ_PRINTINVENTORY, NULL, 0);
}

void client_drop(char *what)
{
    send_master(REQ_DROP, what, strlen(what) + 1);
}

void client_user_list(void)
{
    send_master(REQ_LISTUSERS, NULL, 0);
}
