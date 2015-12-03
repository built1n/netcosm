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

char *client_read(void)
{
    static char buf[128];
    if(read(client_fd, buf, sizeof(buf) - 1) < 0)
    {
        error("lost connection");
        exit(EXIT_FAILURE);
    }
    buf[sizeof(buf) - 1] = '\0';
    const unsigned char ctrlc[] = { 0xff, 0xf4, 0xff, 0xfd, 0x06 };

    if(!memcmp(buf, ctrlc, sizeof(ctrlc)))
        exit(0);

    return buf;
}

void client_main(int fd, struct sockaddr_in *addr, int total)
{
    client_fd = fd;

    bool admin = false;

    char *ip = inet_ntoa(addr->sin_addr);
    printf("New client %s\n", ip);
    printf("Total clients: %d\n", total);
    out("Hello %s.\n", ip);
    out("Please authenticate to continue.\n");
    out("login: ");
    char *input = client_read();

}
