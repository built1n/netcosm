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

#define SALT_LEN 12
#define ALGO GCRY_MD_SHA512
#define AUTH_HASHLEN (512/8)
//#define HASH_ITERS 500000
#define HASH_ITERS 1

struct authinfo_t {
    bool success;
    const char *user;
    int authlevel;
};

/* makes admin account */
void first_run_setup(void);

struct userdata_t;

/* NULL on failure, user data struct on success */
struct userdata_t *auth_check(const char *user, const char *pass);

bool auth_user_add(const char *user, const char *pass, int authlevel);
bool auth_user_del(const char *user);

/* lists users through out() */
void auth_user_list(void);
