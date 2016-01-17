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

#include "globals.h"

void remove_cruft(char *str)
{
    char *junk;
    strtok_r(str, "\r\n", &junk);
}

/**
 * WARNING: not signal-safe AT ALL
 * TODO: rewrite to avoid calling *printf()
 */
void debugf_real(const char *func, int line, const char *file, const char *fmt, ...)
{
    (void) func;
    (void) line;
    (void) file;
    static int fd = -1;
    if(fd < 0)
        fd = open(LOGFILE, O_APPEND | O_WRONLY | O_CREAT, 0600);
    if(fd < 0)
        error("unknown");
    int len;
#if 0
    char *prefix;
    len = asprintf(&prefix, "%s:%s:%d: ", func, file, line);
    write(STDOUT_FILENO, prefix, len);
    free(prefix);
#endif

    va_list ap;
    va_start(ap, fmt);

    char *buf;
    len = vasprintf(&buf, fmt, ap);

    write(STDOUT_FILENO, buf, len);
    write(fd, buf, len);
    fflush(fdopen(fd, "a"));

    free(buf);

    va_end(ap);
}

void all_upper(char *s)
{
    while(*s)
    {
        *s = toupper(*s);
        s++;
    }
}
