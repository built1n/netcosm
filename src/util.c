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

#include "netcosm.h"

void remove_cruft(char *str)
{
    char *junk;
    strtok_r(str, "\r\n", &junk);
}

/**
 * WARNING: not signal-safe
 * TODO: rewrite to avoid calling *printf()
 */
void debugf_real(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char buf[128];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);

    write(STDOUT_FILENO, buf, len);

    va_end(ap);
}
