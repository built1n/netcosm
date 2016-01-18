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

/* commands */
#define IAC  255
#define DONT 254
#define DO   253
#define WONT 252
#define WILL 251
#define SB   250
#define GA   249
#define EL   248
#define EC   247
#define AYT  246
#define IP   244
#define NOP  241
#define SE   240

/* options */
#define ECHO     1
#define SGA      3
#define STATUS   5
#define NAWS     31
#define LINEMODE 34

void telnet_init(void);

enum telnet_status { TELNET_DATA = 0,
                     TELNET_FOUNDCMD,
                     TELNET_EXIT };

enum telnet_status telnet_parse_data(const unsigned char*, size_t);

uint16_t telnet_get_width(void);
uint16_t telnet_get_height(void);

void telnet_echo_on(void);
void telnet_echo_off(void);
