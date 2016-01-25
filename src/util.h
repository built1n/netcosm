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

/* utility functions */
void __attribute__((noreturn,format(printf,1,2))) error(const char *fmt, ...);
void __attribute__((format(printf,4,5))) debugf_real(const char*, int, const char*, const char *fmt, ...);
void remove_cruft(char*);
void all_upper(char*);
void all_lower(char*);

void write_string(int fd, const char *str);
char *read_string(int fd);

void write_roomid(int fd, room_id *id);
room_id read_roomid(int fd);

void write_bool(int fd, bool b);
bool read_bool(int fd);
