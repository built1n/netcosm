#include "netcosm.h"

int client_fd;

void __attribute__((format(printf,1,2))) out(const char *fmt, ...)
{
    char buf[128];
    memset(buf, 0, sizeof(buf));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(client_fd, buf, sizeof(buf));
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

#define WSPACE " \t\r\n"

void client_main(int fd, struct sockaddr_in *addr, int total)
{
    client_fd = fd;

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

    /* auth loop */
    while(1)
    {
        out("login: ");
        current_user = client_read();
        out("Password: ");
        char *pass = client_read();
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

    /* authenticated */
    while(1)
    {
        out(">> ");
        char *cmd = client_read();

        char *save = NULL;

        char *tok = strtok_r(cmd, WSPACE, &save);

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
            }
        }

        if(!strcmp(tok, "QUIT"))
        {
            free(cmd);
            goto done;
        }

    next_cmd:

        free(cmd);
    }

done:
    free(current_user);
}
