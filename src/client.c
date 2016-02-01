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

#include "auth.h"
#include "client.h"
#include "hash.h"
#include "server.h"
#include "room.h"
#include "telnet.h"
#include "util.h"

static bool admin = false;

static int client_fd, to_parent, from_parent;

static room_id current_room = 0;

static volatile sig_atomic_t output_locked = 0;

char *current_user = NULL;

bool poll_requests(void);

void out_raw(const void *buf, size_t len)
{
    if(!len)
        return;

try_again:

    while(output_locked);

    /* something weird happened and the value changed between the loop and here */
    if(!output_locked)
    {
        output_locked = 1;
        write(client_fd, buf, len);
        output_locked = 0;
    }
    else
        goto try_again;
}

void __attribute__((format(printf,1,2))) out(const char *fmt, ...)
{
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    va_list ap;
    va_start(ap, fmt);

    vsnprintf(buf, sizeof(buf), fmt, ap);

    va_end(ap);

    /* do some line wrapping */

    static int pos = 0;
    int last_space = 0;
    char *ptr = buf;
    uint16_t line_width = telnet_get_width() + 1;
    char *line_buf = malloc(line_width + 2);
    size_t line_idx = 0;
    while(ptr[pos])
    {
        bool is_newline = (ptr[pos] == '\n');
        if(is_newline || pos >= line_width)
        {
            if(is_newline || !last_space)
                last_space = pos;

            while(*ptr && last_space-- > 0)
            {
                line_buf[line_idx++] = *ptr++;
            }

            line_buf[line_idx++] = '\r';
            line_buf[line_idx++] = '\n';

            out_raw(line_buf, line_idx);
            line_idx = 0;

            if(is_newline)
                ++ptr; /* skip the newline */
            while(*ptr == ' ')
                ++ptr;
            last_space = 0;
            pos = 0;
        }
        else
        {
            if(ptr[pos] == ' ')
                last_space = pos;
            ++pos;
        }
    }
    out_raw(ptr, strlen(ptr));
    free(line_buf);
}

static volatile sig_atomic_t request_complete;

/* for rate-limiting */
static int reqs_since_ts;
static time_t ts = 0;

void send_master(unsigned char cmd, const void *data, size_t sz)
{
    if(!admin)
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

    /* format of child->parent packets:
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

#define CLIENT_READ_SZ 128

char *client_read(void)
{
    char *buf;
    size_t bufidx;
tryagain:

    buf = calloc(1, CLIENT_READ_SZ);
    bufidx = 0;

    /* set of the client fd and the pipe from our parent */
    struct pollfd fds[2];

    /* order matters here: we first fulfill parent requests, then
     * handle client data */

    fds[0].fd = from_parent;
    fds[0].events = POLLIN;

    fds[1].fd = client_fd;
    fds[1].events = POLLIN;

    while(1)
    {
        poll(fds, ARRAYLEN(fds), -1);
        for(int i = 0; i < 2; ++i)
        {
            if(fds[i].revents & POLLIN)
            {
                if(fds[i].fd == from_parent)
                {
                    poll_requests();
                }
                else if(fds[i].fd == client_fd)
                {
                    ssize_t len = read(client_fd, buf + bufidx, CLIENT_READ_SZ - bufidx - 1);
                    if(len <= 0)
                        error("lost connection (%d)", fds[i].revents);

                    buf[CLIENT_READ_SZ - 1] = '\0';

                    enum telnet_status ret = telnet_parse_data((unsigned char*)buf + bufidx, len);

                    switch(ret)
                    {
                    case TELNET_EXIT:
                    case TELNET_FOUNDCMD:
                        free(buf);
                        if(ret == TELNET_EXIT)
                            exit(0);
                        goto tryagain;
                    case TELNET_DATA:
                        bufidx += len;
                        continue;
                    case TELNET_LINEOVER:
                        break;
                    }

                    remove_cruft(buf);

                    return buf;
                }
            }
        }
    }
}

/* still not encrypted, but a bit more secure than echoing the password! */
char *client_read_password(void)
{
    telnet_echo_off();
    char *ret = client_read();
    telnet_echo_on();
    out("\n");
    return ret;
}

enum reqdata_typespec reqdata_type = TYPE_NONE;
union reqdata_t returned_reqdata;

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

        if(packetlen <= 0)
            goto fail;

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
            returned_reqdata.boolean = status;
            if(!status)
                out("You cannot go that way.\n");
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
    all_lower(obj);
    send_master(REQ_TAKE, obj, strlen(obj) + 1);
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

#define WSPACE " \t\r\n"

void client_main(int fd, struct sockaddr_in *addr, int total, int to, int from)
{
    client_fd = fd;
    to_parent = to;
    from_parent = from;

    output_locked = 0;

    telnet_init();

    char *ip = inet_ntoa(addr->sin_addr);
    debugf("New client %s\n", ip);
    debugf("Total clients: %d\n", total);

    debugf("client is running with uid %d\n", getuid());

auth:

    out("NetCosm " NETCOSM_VERSION "\n");
    if(total > 1)
        out("%d clients connected.\n", total);
    else
        out("%d client connected.\n", total);

    out("\nPlease authenticate to continue.\n\n");

    int failures = 0;

    int authlevel;
    struct userdata_t *current_data = NULL;

    client_change_state(STATE_AUTH);

    /* auth loop */
    while(1)
    {
        out("login: ");

        current_user = client_read();
        remove_cruft(current_user);

        out("Password: ");

        char *pass = client_read_password();

        client_change_state(STATE_CHECKING);

        current_data = auth_check(current_user, pass);

        memset(pass, 0, strlen(pass));
        free(pass);

        if(current_data)
        {
            out("Last login: %s", ctime(&current_data->last_login));
            current_data->last_login = time(0);
            authlevel = current_data->priv;
            userdb_request_add(current_data);
            break;
        }
        else
        {
            client_change_state(STATE_FAILED);
            free(current_user);
            current_user = NULL;
            out("Login incorrect\n\n");
            if(++failures >= MAX_FAILURES)
                return;
        }
    }

    /* something has gone wrong, but we are here for some reason */
    if(authlevel == PRIV_NONE)
        return;

    admin = (authlevel == PRIV_ADMIN);
    if(admin)
        client_change_state(STATE_ADMIN);
    else
        client_change_state(STATE_LOGGEDIN);

    /* authenticated, begin main command loop */
    debugf("client: Authenticated as %s\n", current_user);
    client_change_user(current_user);
    current_room = 0;

    client_change_room(current_room);

    client_look();

    while(1)
    {
        out(">> ");
        char *cmd = client_read();
        char *orig = strdup(cmd);
        char *save = NULL;

        char *tok = strtok_r(cmd, WSPACE, &save);

        if(!tok)
            goto next_cmd;
        all_upper(tok);

        if(admin)
        {
            if(!strcmp(tok, "USER"))
            {
                char *what = strtok_r(NULL, WSPACE, &save);
                if(!what)
                    goto next_cmd;

                all_upper(what);

                if(!strcmp(what, "DEL"))
                {
                    char *user = strtok_r(NULL, WSPACE, &save);
                    if(user)
                    {
                    if(strcmp(user, current_user) && auth_user_del(user))
                        out("Success.\n");
                    else
                        out("Failure.\n");
                    }
                    else
                    {
                        out("Usage: USER DEL <USERNAME>\n");
                    }
                }
                else if(!strcmp(what, "ADD") || !strcmp(what, "MODIFY"))
                {
                    char *user = strtok_r(NULL, WSPACE, &save);
                    if(user)
                    {
                        if(!strcmp(user, current_user))
                        {
                            out("Do not modify your own password using USER. User CHPASS instead.\n");
                            goto next_cmd;
                        }

                        out("Editing user '%s'\n", user);

                        out("New Password (_DO_NOT_USE_A_VALUABLE_PASSWORD_): ");

                        /* BAD BAD BAD BAD BAD BAD BAD CLEARTEXT PASSWORDS!!! */
                        char *pass = client_read_password();

                        out("Verify Password: ");
                        char *pass2 = client_read_password();

                        if(strcmp(pass, pass2))
                        {
                            memset(pass, 0, strlen(pass));
                            memset(pass2, 0, strlen(pass2));
                            free(pass);
                            free(pass2);
                            out("Failure.\n");
                            goto next_cmd;
                        }

                        out("Admin privileges [y/N]? ");
                        char *allow_admin = client_read();
                        int priv = PRIV_USER;
                        if(toupper(allow_admin[0]) == 'Y')
                            priv = PRIV_ADMIN;

                        free(allow_admin);

                        if(auth_user_add(user, pass, priv))
                            out("Success.\n");
                        else
                            out("Failure.\n");
                        memset(pass, 0, strlen(pass));
                        free(pass);
                    }
                    else
                        out("Usage: USER <ADD|MODIFY> <USERNAME>\n");
                }
                else if(!strcmp(what, "LIST"))
                {
                    client_user_list();
                }
                else
                {
                    out("Usage: USER <ADD|DEL|MODIFY|LIST> <ARGS>\n");
                }
            }
            else if(!strcmp(tok, "CLIENT"))
            {
                char *what = strtok_r(NULL, WSPACE, &save);
                if(!what)
                {
                    out("Usage: CLIENT <LIST|KICK> <PID>\n");
                    goto next_cmd;
                }

                all_upper(what);

                if(!strcmp(what, "LIST"))
                {
                    send_master(REQ_LISTCLIENTS, NULL, 0);
                }
                else if(!strcmp(what, "KICK"))
                {
                    char *pid_s = strtok_r(NULL, WSPACE, &save);
                    all_upper(pid_s);
                    if(pid_s)
                    {
                        if(!strcmp(pid_s, "ALL"))
                        {
                            const char *msg = "Kicking everyone...\n";
                            send_master(REQ_KICKALL, msg, strlen(msg));
                            goto next_cmd;
                        }
                        /* weird pointer voodoo */
                        /* TODO: simplify */
                        char pidbuf[MAX(sizeof(pid_t), MSG_MAX)];
                        char *end;
                        pid_t pid = strtol(pid_s, &end, 0);
                        if(pid == getpid())
                        {
                            out("You cannot kick yourself. Use EXIT instead.\n");
                            goto next_cmd;
                        }
                        else if(*end != '\0')
                        {
                            out("Expected a child PID after KICK.\n");
                            goto next_cmd;
                        }
                        memcpy(pidbuf, &pid, sizeof(pid));
                        int len = sizeof(pid_t) + snprintf(pidbuf + sizeof(pid_t),
                                                           sizeof(pidbuf) - sizeof(pid_t),
                                                           "You were kicked.\n");
                        send_master(REQ_KICK, pidbuf, len);
                        debugf("Success.\n");
                    }
                    else
                        out("Usage: CLIENT KICK <PID>\n");
                }
            }
            //else if(!strcmp(tok, "HANG"))
            //{
            //    send_master(REQ_HANG);
            //}
        }

        /* unprivileged commands */
        if(!strcmp(tok, "QUIT") || !strcmp(tok, "EXIT"))
        {
            free(cmd);
            goto done;
        }
        else if(!strcmp(tok, "SAY"))
        {
            char buf[MSG_MAX];
            char *what = strtok_r(NULL, "", &save);
            int len = snprintf(buf, sizeof(buf), "%s says %s\n", current_user, what);

            send_master(REQ_BCASTMSG, buf, len);
        }
        else if(!strcmp(tok, "DATE"))
        {
            time_t t = time(NULL);
            out("%s", ctime(&t));
        }
        else if(!strcmp(tok, "LOGOUT"))
        {
            out("Logged out.\n");
            goto auth;
        }
        else if(!strcmp(tok, "LOOK"))
        {
            char *what = strtok_r(NULL, " ", &save);
            if(!what)
                client_look();
            else
            {
                client_look_at(what);
            }
        }
        else if(!strcmp(tok, "INVENTORY"))
        {
            client_inventory();
        }
        else if(!strcmp(tok, "TAKE"))
        {
            char *what = strtok_r(NULL, " ", &save);
            client_take(what);
        }
        else if(!strcmp(tok, "WAIT"))
        {
            send_master(REQ_WAIT, NULL, 0);
        }
        else if(!strcmp(tok, "GO"))
        {
            char *dir = strtok_r(NULL, WSPACE, &save);
            if(dir)
            {
                all_upper(dir);
                if(client_move(dir))
                    client_look();
            }
            else
                out("Expected direction after GO.\n");
        }
        else if(!strcmp(tok, "DROP"))
        {
            char *what = strtok_r(NULL, " ", &save);
            client_drop(what);
        }
        else
        {
            /* we can't handle it, send it to the master */

            send_master(REQ_EXECVERB, orig, strlen(orig) + 1);
        }

    next_cmd:

        free(cmd);
        free(orig);
    }

done:
    free(current_user);
    current_user = NULL;
}
