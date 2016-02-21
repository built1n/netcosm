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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server_reqs.h"
#include "room.h"

struct child_data {
    pid_t pid;
    int readpipe[2];
    int outpipe[2];

    int state;
    room_id room;
    char *user;

    ev_io *io_watcher;
    ev_child *sigchld_watcher;

    struct in_addr addr;
};

extern volatile int num_clients;
extern void *child_map;
extern bool are_child;

int server_main(int argc, char *argv[]);
void server_save_state(bool force);
