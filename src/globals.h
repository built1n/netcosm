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

/* You should use #pragma once everywhere. */
#pragma once

#define _GNU_SOURCE

#include <ev.h>
#include <openssl/sha.h>
#include <openssl/opensslv.h>

#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <assert.h>
#include <bsd/string.h> // for strlcat
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* convenience macros */
#define ARRAYLEN(x) (sizeof(x)/sizeof(x[0]))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

/* global constants */
#define USERFILE "users.dat"
#define WORLDFILE "world.dat"
#define LOGFILE "netcosm.log"

#define WORLD_MAGIC  0x31415926
#define USERDB_MAGIC 0x27182818
#define MAX_FAILURES 3
#define NETCOSM_VERSION "0.4.0-rc1"

/* username length */
#define MAX_NAME_LEN 32

/* for convenience when writing world specs */
#define NONE_N  NULL
#define NONE_NE NULL
#define NONE_E  NULL
#define NONE_SE NULL
#define NONE_S  NULL
#define NONE_SW NULL
#define NONE_W  NULL
#define NONE_NW NULL
#define NONE_UP NULL
#define NONE_DN NULL
#define NONE_IN NULL
#define NONE_OT NULL

#include "util.h"

#define MSG_MAX PIPE_BUF
#ifndef NDEBUG
#define debugf(fmt,...) debugf_real(__func__, __LINE__, __FILE__, fmt, ##__VA_ARGS__)
#else
#define debugf(fmt,...) /* nop */
#endif

#if 0
extern bool are_child;

static void *logged_malloc(const char *func, int line, size_t sz)
{
    if(are_child)
        debugf("%s:%d mallocs %d bytes\n", func, line, sz);
    return malloc(sz);
}

static void *logged_calloc(const char *func, int line, size_t x, size_t y)
{
    if(are_child)
        debugf("%s:%d callocs %dx%d bytes\n", func, line, x,y);
    return calloc(x,y);
}

#define malloc(x) logged_malloc(__func__, __LINE__, x)
#define calloc(x,y) logged_calloc(__func__, __LINE__, x,y)
#endif
