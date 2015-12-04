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
    char *buf = malloc(BUFSZ);
    memset(buf, 0, BUFSZ);
    if(read(client_fd, buf, BUFSZ - 1) < 0)
        error("lost connection");
    buf[BUFSZ - 1] = '\0';
    const unsigned char ctrlc[] = { 0xff, 0xf4, 0xff, 0xfd, 0x06 };

    if(!memcmp(buf, ctrlc, sizeof(ctrlc)))
        exit(0);

    return buf;
}

void client_main(int fd, struct sockaddr_in *addr, int total)
{
    client_fd = fd;

    char *ip = inet_ntoa(addr->sin_addr);
    printf("New client %s\n", ip);
    printf("Total clients: %d\n", total);
    out("Hello %s.\n", ip);
    out("Please authenticate to continue.\n\n");

    int failures = 0;

    int authlevel;

    /* auth loop */
    while(1)
    {
        out("NetCosm login: ");
        char *user = client_read();
        out("Password: ");
        char *pass = client_read();
        printf("pass is %s\n", pass);
        struct authinfo_t auth = auth_check(user, pass);
        free(user);
        free(pass);
        authlevel = auth.authlevel;
        if(auth.success)
        {
            out("Access Granted.\n\n");
            break;
        }
        else
        {
            out("Access Denied.\n\n");
            if(++failures >= MAX_FAILURES)
                return;
        }
    }

    /* authenticated */
    while(1)
    {
        out(">> ");
        char *cmd = client_read();

        char *tok = strtok(cmd, " \t\r\n");

        if(!strcmp(tok, "USER"))
        {
            void change_user(const char *name2, const char *pass2, int level);
            add_user("admin", "test", 0);
        }

        free(cmd);
    }
}
