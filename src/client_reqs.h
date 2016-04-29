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

#pragma once

#include "globals.h"

#include "server.h"
#include "server_reqs.h"
#include "userdb.h"

enum room_id;

enum reqdata_typespec { TYPE_NONE = 0, TYPE_USERDATA, TYPE_BOOLEAN };

union reqdata_t {
    struct userdata_t userdata;
    bool boolean;
};

extern enum reqdata_typespec reqdata_type;
extern union reqdata_t returned_reqdata;

void client_change_room(room_id id);
void client_change_user(const char *user);
void client_change_state(int state);
bool client_move(const char *dir);
void client_look(void);
void client_look_at(char *obj);
void client_inventory(void);
void client_drop(char *what);
void client_user_list(void);
void client_take(char *obj);
