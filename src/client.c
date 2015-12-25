#include "netcosm.h"

static int client_fd, to_parent, from_parent;

static room_id current_room = 0;

static volatile sig_atomic_t output_locked = 0;

void out_raw(const unsigned char *buf, size_t len)
{
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
    char buf[128];
    memset(buf, 0, sizeof(buf));
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    out_raw((unsigned char*)buf, len);
}

static volatile sig_atomic_t request_complete;

static void signal_master(void)
{
    request_complete = 0;
    sigset_t block, old;

    sigemptyset(&block);
    sigaddset(&block, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block, &old);

    kill(getppid(), SIGUSR1);

    /* wait for a signal */
    printf("Waiting for signal.\n");

    sigsuspend(&old);
    sigprocmask(SIG_SETMASK, &old, NULL);

    errno = 0;

    /* spin until we're done handling the request */
    while(!request_complete) usleep(1);
}

void send_master(unsigned char cmd, const void *data, size_t sz)
{
    write(to_parent, &cmd, 1);
    if(data)
        write(to_parent, data, sz);
    signal_master();
}

#define BUFSZ 128

char *client_read(void)
{
    char *buf;

tryagain:

    buf = malloc(BUFSZ);
    memset(buf, 0, BUFSZ);
    if(read(client_fd, buf, BUFSZ - 1) < 0)
        error("lost connection");
    buf[BUFSZ - 1] = '\0';

    printf("Read '%s'\n", buf);
    if(buf[0] & 0x80)
    {
        telnet_handle_command((unsigned char*)buf);

        free(buf);
        goto tryagain;
    }

    return buf;
}

/* still not encrypted, but a bit more secure than echoing the pass */
char *client_read_password(void)
{
    telnet_echo_off();
    char *ret = client_read();
    telnet_echo_on();
    out("\n");
    return ret;
}

void all_upper(char *s)
{
    while(*s)
    {
        *s = toupper(*s);
        s++;
    }
}

void sigusr2_handler(int s, siginfo_t *info, void *vp)
{
    (void) s;
    (void) vp;

    sig_printf("PID %d got SIGUSR2\n", getpid());

    /* we only listen to requests from our parent */
    if(info->si_pid != getppid())
    {
        sig_printf("Unknown PID sent SIGUSR2\n");
        return;
    }

    unsigned char cmd;
    read(from_parent, &cmd, 1);
    sig_printf("Got data from parent.\n");
    unsigned char buf[MSG_MAX + 1];
    switch(cmd)
    {
    case REQ_BCASTMSG:
    {
        sig_printf("reading...\n");
        ssize_t len = read(from_parent, buf, MSG_MAX);
        sig_printf("done reading\n");
        buf[MSG_MAX] = '\0';
        out_raw(buf, len);
        break;
    }
    case REQ_KICK:
    {
        ssize_t len = read(from_parent, buf, MSG_MAX);
        buf[MSG_MAX] = '\0';
        out_raw(buf, len);
        exit(EXIT_SUCCESS);
    }
    case REQ_MOVE:
    {
        int status;
        read(from_parent, &status, sizeof(status));
        if(!status)
            out("Cannot go that way.\n");
    }
    case REQ_NOP:
        sig_printf("NOP from parent.\n");
        break;
    default:
        sig_printf("WARNING: client process received unknown code %d\n", cmd);
        break;
    }

    sig_printf("Client finishes handling request.\n");

    request_complete = 1;
}

static void client_change_state(int state)
{
    printf("Client requesting state transition\n");
    send_master(REQ_CHANGESTATE, &state, sizeof(state));
    printf("State transition completed.\n");
}

static void client_change_user(const char *user)
{
    send_master(REQ_CHANGEUSER, user, strlen(user) + 1);
}

static void client_change_room(room_id id)
{
    send_master(REQ_SETROOM, &id, sizeof(id));
}

static void client_move(const char *dir)
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
    static void *map = NULL;
    if(!map)
    {
        map = hash_init(ARRAYLEN(dirs), hash_djb, compare_strings);
        hash_insert_pairs(map, (struct hash_pair*)dirs, sizeof(struct dir_pair), ARRAYLEN(dirs));
    }

    struct dir_pair *pair = hash_lookup(map, dir);
    if(pair)
    {
        send_master(REQ_MOVE, &pair->val, sizeof(pair->val));
    }
    else
        out("Unknown direction.\n");
}

static void client_look(void)
{
    send_master(REQ_GETROOMNAME, NULL, 0);
    out("\n");
    send_master(REQ_GETROOMDESC, NULL, 0);
}

#define WSPACE " \t\r\n"

void client_main(int fd, struct sockaddr_in *addr, int total, int to, int from)
{
    client_fd = fd;
    to_parent = to;
    from_parent = from;

    output_locked = 0;

    struct sigaction sa;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_sigaction = sigusr2_handler;
    if(sigaction(SIGUSR2, &sa, NULL) < 0)
        error("sigaction");

    telnet_init();

    char *ip = inet_ntoa(addr->sin_addr);
    printf("New client %s\n", ip);
    printf("Total clients: %d\n", total);

auth:

    out("NetCosm " NETCOSM_VERSION "\n");
    if(total > 1)
        out("%d clients connected.\n", total);
    else
        out("%d client connected.\n", total);

    out("\nPlease authenticate to continue.\n\n");

    int failures = 0;

    int authlevel;

    char *current_user;

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
        struct authinfo_t auth = auth_check(current_user, pass);
        memset(pass, 0, strlen(pass));
        free(pass);

        authlevel = auth.authlevel;
        if(auth.success)
        {
            out("Access Granted.\n\n");
            break;
        }
        else
        {
            client_change_state(STATE_FAILED);
            free(current_user);
            out("Access Denied.\n\n");
            if(++failures >= MAX_FAILURES)
                return;
        }
    }

    /* something has gone wrong, but we are here for some reason */
    if(authlevel == PRIV_NONE)
        return;

    bool admin = (authlevel == PRIV_ADMIN);
    if(admin)
        client_change_state(STATE_ADMIN);
    else
        client_change_state(STATE_LOGGEDIN);

    /* authenticated */
    printf("Authenticated as %s\n", current_user);
    client_change_user(current_user);
    current_room = 0;
    client_change_room(current_room);
    client_look();
    while(1)
    {
        out(">> ");
        char *cmd = client_read();

        char *save = NULL;

        char *tok = strtok_r(cmd, WSPACE, &save);

        if(!tok)
            continue;
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
                    auth_user_list();
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
                    if(pid_s)
                    {
                        /* weird pointer voodoo */
                        /* TODO: simplify */
                        char pidbuf[MAX(sizeof(pid_t), MSG_MAX)];
                        pid_t pid = strtol(pid_s, NULL, 0);
                        if(pid == getpid())
                        {
                            out("You cannot kick yourself. Use EXIT instead.\n");
                            goto next_cmd;
                        }
                        memcpy(pidbuf, &pid, sizeof(pid));
                        int len = sizeof(pid_t) + snprintf(pidbuf + sizeof(pid_t),
                                                           sizeof(pidbuf) - sizeof(pid_t),
                                                           "You were kicked.\n");
                        send_master(REQ_KICK, pidbuf, len);
                        printf("Success.\n");
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

        if(!strcmp(tok, "QUIT") || !strcmp(tok, "EXIT"))
        {
            free(cmd);
            goto done;
        }
        else if(!strcmp(tok, "SAY"))
        {
            char buf[MSG_MAX];
            char *what = strtok_r(NULL, "", &save);
            int len = snprintf(buf, sizeof(buf), "%s says %s", current_user, what);

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
            client_look();
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
                client_move(dir);
                client_look();
            }
            else
                out("Expected direction after GO.\n");
        }

    next_cmd:

        free(cmd);
    }

done:
    free(current_user);
}
