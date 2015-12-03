#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define USERFILE "users.dat"

void client_main(int fd, struct sockaddr_in *addr, int);
void __attribute__((noreturn)) error(const char *fmt, ...);
void first_run_setup(void);
