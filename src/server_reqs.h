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

/* child<->master commands */
/* children might not implement all of these */
/* meanings might be different for the server and child, see comments */
#define REQ_NOP               0 /* server, child: do nothing (used for acknowledgement) */
#define REQ_BCASTMSG          1 /* server: broadcast text; child: print following text */
#define REQ_LISTCLIENTS       2 /* server: list childs */
#define REQ_CHANGESTATE       3 /* server: change child state flag */
#define REQ_CHANGEUSER        4 /* server: change child login name */
#define REQ_HANG              5 /* <UNIMP> server: loop forever */
#define REQ_KICK              6 /* server: kick PID with message; child: print message, quit */
#define REQ_WAIT              7 /* server: sleep 10s */
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
