#include "netcosm.h"

int client_fd, to_parent, from_parent;

void out_raw(const unsigned char *buf, size_t len)
{
    write(client_fd, buf, len);
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
    const unsigned char ctrlc[] = { 0xff, 0xf4, 0xff, 0xfd, 0x06 };

    if(!memcmp(buf, ctrlc, sizeof(ctrlc)))
        exit(0);

    printf("Read '%s'\n", buf);
    if(buf[0] & 0x80)
    {
        telnet_handle_command((unsigned char*)buf);

        free(buf);
        goto tryagain;
    }

    return buf;
}

void all_upper(char *s)
{
    while(*s)
    {
        *s = toupper(*s);
        s++;
    }
}

void sigusr2_handler(int s)
{
    (void) s;
    unsigned char buf[MSG_MAX + 1];
    size_t len = read(from_parent, buf, MSG_MAX);
    buf[MSG_MAX] = '\0';
    out_raw(buf, len);
}

void client_change_state(int state)
{
    unsigned char cmdcode = REQ_CHANGESTATE;
    write(to_parent, &cmdcode, sizeof(cmdcode));
    write(to_parent, &state, sizeof(state));
    kill(getppid(), SIGUSR1);
}

void client_change_user(const char *user)
{
    unsigned char cmdcode = REQ_CHANGEUSER;
    write(to_parent, &cmdcode, sizeof(cmdcode));
    write(to_parent, user, strlen(user) + 1);
    kill(getppid(), SIGUSR1);
}

#define WSPACE " \t\r\n"

void client_main(int fd, struct sockaddr_in *addr, int total, int to, int from)
{
    client_fd = fd;
    to_parent = to;
    from_parent = from;

    signal(SIGUSR2, sigusr2_handler);

    telnet_init();

    char *ip = inet_ntoa(addr->sin_addr);
    printf("New client %s\n", ip);
    printf("Total clients: %d\n", total);

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
        telnet_echo_off();
        out("Password: ");
        char *pass = client_read();
        telnet_echo_on();
        out("\n");
        client_change_state(STATE_CHECKING);
        struct authinfo_t auth = auth_check(current_user, pass);
        memset(pass, 0, strlen(pass));
        free(pass);

        authlevel = auth.authlevel;
        if(auth.success)
        {
            client_change_state(STATE_LOGGEDIN);
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

    /* authenticated */
    printf("Authenticated as %s\n", current_user);
    client_change_user(current_user);
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
                all_upper(what);

                if(!strcmp(what, "DEL"))
                {
                    char *user = strtok_r(NULL, WSPACE, &save);
                    if(user)
                    {
                    if(strcmp(user, current_user) && auth_remove(user))
                        out("Success.\n");
                    else
                        out("Failure.\n");
                    }
                    else
                    {
                        out("Usage: USER DEL <USERNAME>\n");
                    }
                }
                else if(!strcmp(what, "ADD") || !strcmp(what, "PASS"))
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
                        char *pass = client_read();

                        out("Admin privileges [y/N]? ");
                        char *allow_admin = client_read();
                        int priv = PRIV_USER;
                        if(toupper(allow_admin[0]) == 'Y')
                            priv = PRIV_ADMIN;

                        free(allow_admin);

                        if(add_change_user(user, pass, priv))
                            out("Success.\n");
                        else
                            out("Failure.\n");
                        memset(pass, 0, strlen(pass));
                        free(pass);
                    }
                    else
                        out("Usage: USER ADD|CHANGE <USERNAME>\n");
                }
                else if(!strcmp(what, "LIST"))
                {
                    auth_list_users();
                }
            }
            else if(!strcmp(tok, "CLIENTS"))
            {
                unsigned char cmd_code = REQ_LISTCLIENTS;
                write(to_parent, &cmd_code, sizeof(cmd_code));
                kill(getppid(), SIGUSR1);
                waitpid(-1, NULL, 0);
            }
        }

        if(!strcmp(tok, "QUIT") || !strcmp(tok, "EXIT"))
        {
            free(cmd);
            goto done;
        }
        else if(!strcmp(tok, "SAY"))
        {
            char *what = strtok_r(NULL, "", &save);
            unsigned char cmd_code = REQ_BCASTMSG;
            write(to_parent, &cmd_code, sizeof(cmd_code));
            dprintf(to_parent, "%s says %s", current_user, what);
            kill(getppid(), SIGUSR1);
        }

    next_cmd:

        free(cmd);
    }

done:
    free(current_user);
}
