/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2015 Franklin Wei
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

#include "netcosm.h"

#define PORT 1234
#define BACKLOG 16

void __attribute__((noreturn)) error(const char *fmt, ...)
{
    char buf[128];
    memset(buf, 0, sizeof(buf));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    perror(buf);
    exit(EXIT_FAILURE);
}

int num_clients = 0;

void sigchld_handler(int s)
{
    (void) s;
    printf("Client disconnect.\n");
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;

    --num_clients;
}

int port;

void handle_client(int fd, struct sockaddr_in *addr, int num_clients)
{
    client_main(fd, addr, num_clients);
}

int server_socket;

void serv_cleanup(void)
{
    printf("Shutdown server.\n");
    if(shutdown(server_socket, SHUT_RDWR) > 0)
        error("shutdown");
    close(server_socket);
}

void sigint_handler(int sig)
{
    (void) sig;
    serv_cleanup();
}

int main(int argc, char *argv[])
{
    if(argc != 2)
        port = PORT;
    else
        port = strtol(argv[1], NULL, 0);
    srand(time(0));
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock<0)
        error("socket");

    int tmp = 1;

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof tmp) < 0)
        error("setsockopt");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr*) &addr, sizeof addr) < 0)
        error("bind");

    if(listen(sock, BACKLOG) < 0)
        error("listen");

    server_socket = sock;

    struct sigaction sa;

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    signal(SIGINT, sigint_handler);

    if(access(USERFILE, F_OK) < 0)
        first_run_setup();

    printf("Listening on port %d\n", port);

    while(1)
    {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int new_sock = accept(sock, (struct sockaddr*) &client, &client_len);
        if(new_sock < 0)
            error("accept");

        ++num_clients;

        pid_t pid = fork();
        if(pid < 0)
            error("fork");

        if(!pid)
        {
            close(sock);

            server_socket = new_sock;

            handle_client(new_sock, &client, num_clients);

            exit(0);
        }
        else
            close(new_sock);
    }
}
