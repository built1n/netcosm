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

#include "globals.h"

enum room_id;

/* everything the server needs to manage its children */
/* aliased as user_t */
struct child_data {
    pid_t    pid;

    /* pipes, packet mode */
    int      readpipe[2];
    int      outpipe[2];

    /* user state */
    int      state;
    room_id  room;
    char     *user;

    /* libev watchers */
    ev_io    *io_watcher;
    ev_child *sigchld_watcher;

    /* raw mode callback (NULL if none, set by world module) */
    void     (*raw_mode_cb)(struct child_data*, char *data, size_t len);

    /* remote IP */
    struct in_addr addr;
};

typedef struct child_data user_t;

extern volatile int num_clients;
extern void *child_map;
extern bool are_child;

int server_main(int argc, char *argv[]);
void server_save_state(bool force);
