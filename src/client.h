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

extern int client_fd, to_parent, from_parent;
extern bool are_admin;

/* call from child process ONLY */
void send_master(unsigned char cmd, const void *data, size_t sz);

void out(const char *fmt, ...) __attribute__((format(printf,1,2)));
void out_raw(const void*, size_t);

/* called for every client */
void client_main(int sock, struct sockaddr_in *addr, int, int to_parent, int from_parent);

/* can (and should) be called before forking the child */
void client_init(void);
void client_shutdown(void);
