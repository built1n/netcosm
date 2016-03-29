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
#include "client_reqs.h"
#include "hash.h"
#include "server.h"
#include "room.h"
#include "telnet.h"
#include "userdb.h"
#include "util.h"

bool are_admin = false;

int client_fd, to_parent, from_parent;

static room_id current_room = 0;

static volatile sig_atomic_t output_locked = 0;

char *current_user = NULL;

bool poll_requests(void);

void out_raw(const void *buf, size_t len)
{
    if(!are_child)
        error("out() called from master");
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
        if(is_newline || pos >= line_width - 1)
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

	    /* skip following spaces */
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

/* still not encrypted, but a bit better than echoing the password! */
char *client_read_password(void)
{
    telnet_echo_off();
    char *ret = client_read();
    telnet_echo_on();
    out("\n");
    return ret;
}

#define CMD_OK      0
#define CMD_LOGOUT  1
#define CMD_QUIT    2

/*** callbacks ***/

int user_cb(char **save)
{
    char *what = strtok_r(NULL, WSPACE, save);
    if(!what)
        return CMD_OK;

    all_upper(what);

    if(!strcmp(what, "DEL"))
    {
        char *user = strtok_r(NULL, WSPACE, save);
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
        char *user = strtok_r(NULL, WSPACE, save);
        if(user)
        {
            if(!strcmp(user, current_user))
            {
                out("Do not modify your own password using USER. User CHPASS instead.\n");
                return CMD_OK;
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
                return CMD_OK;
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

    return CMD_OK;
}

int client_cb(char **save)
{
    char *what = strtok_r(NULL, WSPACE, save);
    if(!what)
    {
        out("Usage: CLIENT <LIST|KICK> <PID>\n");
        return CMD_OK;
    }

    all_upper(what);

    if(!strcmp(what, "LIST"))
    {
        send_master(REQ_LISTCLIENTS, NULL, 0);
    }
    else if(!strcmp(what, "KICK"))
    {
        char *pid_s = strtok_r(NULL, WSPACE, save);
        if(pid_s)
        {
	    all_upper(pid_s);
            if(!strcmp(pid_s, "ALL"))
            {
                const char *msg = "Kicking everyone...\n";
                send_master(REQ_KICKALL, msg, strlen(msg));
                return CMD_OK;
            }
            /* weird pointer voodoo */
            /* TODO: simplify */
            char pidbuf[MAX(sizeof(pid_t), MSG_MAX)];
            char *end;
            pid_t pid = strtol(pid_s, &end, 0);
            if(pid == getpid())
            {
                out("You cannot kick yourself. Use EXIT instead.\n");
                return CMD_OK;
            }
            else if(*end != '\0')
            {
                out("Expected a child PID after KICK.\n");
                return CMD_OK;
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
    return CMD_OK;
}

int quit_cb(char **save)
{
    (void) save;
    return CMD_QUIT;
}

int say_cb(char **save)
{
    char buf[MSG_MAX];
    char *what = strtok_r(NULL, "", save);
    int len = snprintf(buf, sizeof(buf), "%s says %s\n", current_user, what);

    send_master(REQ_BCASTMSG, buf, len);
    return CMD_OK;
}

int date_cb(char **save)
{
    (void) save;
    time_t t = time(NULL);
    out("%s", ctime(&t));
    return CMD_OK;
}

int logout_cb(char **save)
{
    (void) save;
    out("Logged out.\n");
    telnet_clear_screen();
    return CMD_LOGOUT;
}

int look_cb(char **save)
{
    char *what = strtok_r(NULL, "", save);
    if(!what)
        client_look();
    else
    {
        client_look_at(what);
    }
    return CMD_OK;
}

int inventory_cb(char **save)
{
    (void) save;
    client_inventory();
    return CMD_OK;
}

int take_cb(char **save)
{
    char *what = strtok_r(NULL, "", save);
    client_take(what);
    return CMD_OK;
}

int wait_cb(char **save)
{
    (void) save;
    /* debugging */
    send_master(REQ_WAIT, NULL, 0);
    return CMD_OK;
}

int go_cb(char **save)
{
    char *dir = strtok_r(NULL, WSPACE, save);
    if(dir)
    {
        all_upper(dir);
        if(client_move(dir))
            client_look();
    }
    else
        out("I don't understand where you want me to go.\n");
    return CMD_OK;
}

int drop_cb(char **save)
{
    char *what = strtok_r(NULL, "", save);
    if(what)
	client_drop(what);
    else
	out("You must supply an object.\n");
    return CMD_OK;
}

static const struct client_cmd {
    const char *cmd;
    int (*cb)(char **saveptr);
    bool admin_only;
} cmds[] = {
    {  "USER",       user_cb,       true   },
    {  "CLIENT",     client_cb,     true   },
    {  "EXIT",       quit_cb,       false  },
    {  "QUIT",       quit_cb,       false  },
    {  "SAY",        say_cb,        false  },
    {  "DATE",       date_cb,       false  },
    {  "LOGOUT",     logout_cb,     false  },
    {  "LOOK",       look_cb,       false  },
    {  "INVENTORY",  inventory_cb,  false  },
    {  "TAKE",       take_cb,       false  },
    {  "WAIT",       wait_cb,       true   },
    {  "GO",         go_cb,         false  },
    {  "DROP",       drop_cb,       false  },
};

static void *cmd_map = NULL;

void client_init(void)
{
    cmd_map = hash_init(ARRAYLEN(cmds), hash_djb, compare_strings);
    hash_insert_pairs(cmd_map, (const struct hash_pair*)cmds, sizeof(cmds[0]), ARRAYLEN(cmds));
}

void client_shutdown(void)
{
    hash_free(cmd_map);
    cmd_map = NULL;
}

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

    are_admin = (authlevel == PRIV_ADMIN);
    if(are_admin)
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
        char *line = client_read();
        char *orig = strdup(line);
        char *save = NULL;

        char *tok = strtok_r(line, WSPACE, &save);

        if(!tok)
            goto next_cmd;

        all_upper(tok);

        const struct client_cmd *cmd = hash_lookup(cmd_map, tok);
        if(cmd && cmd->cb && (!cmd->admin_only || (cmd->admin_only && are_admin)))
        {
            int ret = cmd->cb(&save);
            switch(ret)
            {
            case CMD_OK:
                goto next_cmd;
            case CMD_LOGOUT:
                free(line);
                free(orig);
                goto auth;
            case CMD_QUIT:
                free(line);
                free(orig);
                goto done;
            default:
                error("client: bad callback return value");
            }
        }
        else if(cmd && cmd->admin_only && !are_admin)
        {
            out("You are not allowed to do that.\n");
            goto next_cmd;
        }

        /* if we can't handle it, let the master process try */
        send_master(REQ_EXECVERB, orig, strlen(orig) + 1);

    next_cmd:

        free(line);
        free(orig);
    }

done:
    free(current_user);
    current_user = NULL;
}
