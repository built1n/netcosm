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

/* child<->master commands */
/* not all of these are implemented by both parties */
/* meanings might be different for the server and child, see comments */
#define REQ_NOP               0 /* server, child: do nothing (used for acknowledgement) */
#define REQ_BCASTMSG          1 /* server: broadcast text; child: print following text */
#define REQ_LISTCLIENTS       2 /* server: list childs */
#define REQ_CHANGESTATE       3 /* server: change child state flag */
#define REQ_CHANGEUSER        4 /* server: change child login name */
#define REQ_HANG              5 /* <UNIMP> server: loop forever */
#define REQ_KICK              6 /* server: kick PID with message; child: print message, quit */
#define REQ_WAIT              7 /* <DEBUG> server: sleep 10s */
#define REQ_GETROOMDESC       8 /* server: send child room description */
#define REQ_SETROOM           9 /* server: set child room */
#define REQ_MOVE              10 /* server: move child based on direction; child: success or failure */
#define REQ_GETROOMNAME       11 /* server: send child's room name */
#define REQ_LISTROOMCLIENTS   12 /* server: list clients in child's room */
#define REQ_GETUSERDATA       13 /* server: send user data; child: get user data */
#define REQ_DELUSERDATA       14 /* server: delete user data; child: success/failure */
#define REQ_ADDUSERDATA       15 /* server: insert user data; child: success/fail */
#define REQ_PRINTNEWLINE      16 /* child: print a newline */
#define REQ_ALLDONE           17 /* child: break out of send_master() */
#define REQ_KICKALL           18 /* server: kick everyone except the sender */
#define REQ_LOOKAT            19 /* server: send object description */
#define REQ_TAKE              20 /* server: add object to user inventory */
#define REQ_PRINTINVENTORY    21 /* server: print user inventory */
#define REQ_DROP              22 /* server: drop user object if allowed */
#define REQ_LISTUSERS         23 /* server: list users in USERFILE */
#define REQ_EXECVERB          24 /* server: execute a verb with its arguments */
#define REQ_RAWMODE           25 /* child: toggle the child's processing of commands and instead sending input directly to master */

/* child states, sent as an int to the master */
#define STATE_INIT      0 /* initial state */
#define STATE_AUTH      1 /* at login screen */
#define STATE_CHECKING  2 /* checking password */
#define STATE_LOGGEDIN  3 /* logged in as user */
#define STATE_ADMIN     4 /* logged in w/ admin privs */
#define STATE_FAILED    5 /* failed a password attempt */

bool handle_child_req(int in_fd);
void master_ack_handler(int s, siginfo_t *info, void *v);
void reqmap_init(void);
void reqmap_free(void);

void send_msg(user_t *child, const char *fmt, ...) __attribute__((format(printf,2,3)));
