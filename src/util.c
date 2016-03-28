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

void __attribute__((noreturn)) error(const char *fmt, ...)
{
    char buf[128];
    memset(buf, 0, sizeof(buf));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    perror(buf);
    abort();
    exit(EXIT_FAILURE);
}

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

    int len;

    va_list ap;
    va_start(ap, fmt);

    char *buf;
    len = vasprintf(&buf, fmt, ap);

    write(STDOUT_FILENO, buf, len);

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

void all_lower(char *s)
{
    while(*s)
    {
        *s = tolower(*s);
        s++;
    }
}

void write_roomid(int fd, room_id *id)
{
    write(fd, id, sizeof(*id));
}

void write_string(int fd, const char *str)
{
    size_t len = strlen(str);
    write(fd, &len, sizeof(len));
    write(fd, str, len);
}

room_id read_roomid(int fd)
{
    room_id ret;
    if(read(fd, &ret, sizeof(ret)) < 0)
        return ROOM_NONE;
    return ret;
}

char *read_string(int fd)
{
    size_t sz;
    if(read(fd, &sz, sizeof(sz)) != sizeof(sz))
    {
        error("read_string: EOF");
    }
    char *ret = malloc(sz + 1);
    if((size_t)read(fd, ret, sz) != sz)
    {
        free(ret);
        error("read_string: EOF");
    }
    ret[sz] = '\0';
    return ret;
}

bool read_bool(int fd)
{
    bool ret;
    if(read(fd, &ret, sizeof(ret)) != sizeof(ret))
        error("unexpected EOF");
    return ret;
}

void write_bool(int fd, bool b)
{
    if(write(fd, &b, sizeof(b)) != sizeof(b))
        error("write failed");
}

uint32_t read_uint32(int fd)
{
    uint32_t ret;
    if(read(fd, &ret, sizeof(ret)) != sizeof(ret))
        error("unexpected EOF");
    return ret;
}

void write_uint32(int fd, uint32_t b)
{
    if(write(fd, &b, sizeof(b)) != sizeof(b))
        error("write failed");
}

uint64_t read_uint64(int fd)
{
    uint64_t ret;
    if(read(fd, &ret, sizeof(ret)) != sizeof(ret))
        error("unexpected EOF");
    return ret;
}

void write_uint64(int fd, uint64_t b)
{
    if(write(fd, &b, sizeof(b)) != sizeof(b))
        error("write failed");
}

size_t read_size(int fd)
{
    size_t ret;
    if(read(fd, &ret, sizeof(ret)) != sizeof(ret))
        error("unexpected EOF");
    return ret;
}

void write_size(int fd, size_t b)
{
    if(write(fd, &b, sizeof(b)) != sizeof(b))
        error("write failed");
}

bool is_vowel(char c)
{
    switch(tolower(c))
    {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
        return true;
    default:
        return false;
    }
}

/*      $NetBSD: strlcat.c,v 1.4 2005/05/16 06:55:48 lukem Exp $        */
/*      from    NetBSD: strlcat.c,v 1.16 2003/10/27 00:12:42 lukem Exp  */
/*      from OpenBSD: strlcat.c,v 1.10 2003/04/12 21:56:39 millert Exp  */

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TODD C. MILLER DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TODD C. MILLER BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
        char *d = dst;
        const char *s = src;
        size_t n = siz;
        size_t dlen;

        /* Find the end of dst and adjust bytes left but don't go past end */
        while (n-- != 0 && *d != '\0')
                d++;
        dlen = d - dst;
        n = siz - dlen;

        if (n == 0)
                return(dlen + strlen(s));
        while (*s != '\0') {
                if (n != 1) {
                        *d++ = *s;
                        n--;
                }
                s++;
        }
        *d = '\0';

        return(dlen + (s - src));       /* count does not include NUL */
}

char *format_noun(char *buf, size_t len, const char *name, size_t count, bool default_article, bool capitalize)
{
    assert(len > 1);
    buf[0] = '\0';
    if(count == 1)
    {
        if(default_article)
        {
            char *article = capitalize?(is_vowel(name[0])? "An" : "A"):(is_vowel(name[0])? "an" : "a");
            strlcat(buf, article, len);
            strlcat(buf, " ", len);
            strlcat(buf, name, len);
        }
        else
        {
            char tmp[2];
            tmp[0] = toupper(name[0]);
            tmp[1] = '\0';
            strlcat(buf, tmp, len);
            strlcat(buf, name + 1, len);
        }
    }
    else
    {
        char n[32];
        snprintf(n, sizeof(n), "%zu", count);
        strlcat(buf, n, len);
        strlcat(buf, " ", len);
        strlcat(buf, name, len);
        strlcat(buf, "s", len);
    }

    return buf;
}
