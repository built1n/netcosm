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

#include "room.h"
#include "userdb.h"

enum reqdata_typespec { TYPE_NONE = 0, TYPE_USERDATA, TYPE_BOOLEAN } reqdata_type;

union reqdata_t {
    struct userdata_t userdata;
    bool boolean;
};

extern enum reqdata_typespec reqdata_type;
extern union reqdata_t returned_reqdata;

/* call from child process ONLY */
void send_master(unsigned char cmd, const void *data, size_t sz);

/* the master sends the child SIGRTMIN+0 */
void sig_rt_0_handler(int s, siginfo_t *info, void *v);

void out(const char *fmt, ...) __attribute__((format(printf,1,2)));
void out_raw(const void*, size_t);

/* called for every client */
void client_main(int sock, struct sockaddr_in *addr, int, int to_parent, int from_parent);
