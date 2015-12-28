/*
 *   NetCosm - a MUD server
 *   Copyright (C) 2015 Franklin Wei
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

void telnet_handle_command(const unsigned char *buf)
{
    bool cmd = false;

    while(*buf)
    {
        unsigned char c = *buf;

        const struct telnet_cmd {
            int val;
            const char *name;
        } commands[] = {
            {  IAC,     "IAC"     },
            {  DONT,    "DONT"    },
            {  DO,      "DO"      },
            {  WONT,    "WONT"    },
            {  WILL,    "WILL"    },
            {  GA,      "GA"      },
            {  AYT,     "AYT"     },
            {  NOP,     "NOP"     },
            {  SB,      "SB"      },
            {  SE,      "SE"      },
            {  ECHO,    "ECHO"    },
            {  SGA,     "SGA"     },
            {  STATUS,  "STATUS"  },
            {  NAWS,    "NAWS"    }
        };

        for(unsigned int i = 0; i < ARRAYLEN(commands); ++i)
        {
            if(c == commands[i].val)
            {
                debugf("%s ", commands[i].name);
                cmd = true;
                goto found;
            }
        }
        switch(c)
        {
        case IP:
            exit(0);
        default:
            break;
        }
        debugf("???: %d ", c);
    found:

        ++buf;
    }

    if(cmd)
        debugf("\n");
}

void telnet_echo_off(void)
{
    const unsigned char seq[] = {
        IAC, DONT, ECHO,
        IAC, WILL, ECHO,
    };
    out_raw(seq, ARRAYLEN(seq));
}

void telnet_echo_on(void)
{
    const unsigned char seq[] = {
        IAC, DO, ECHO,
        IAC, WONT, ECHO,
    };
    out_raw(seq, ARRAYLEN(seq));
}

void telnet_init(void)
{
    const unsigned char init_seq[] = {
        IAC, WONT, SGA,
        IAC, DONT, SGA,
        IAC, WONT, NAWS,
        IAC, DONT, NAWS,
        IAC, WONT, STATUS,
        IAC, DONT, STATUS,
        IAC, DO,   ECHO,
        IAC, WONT, ECHO,
        IAC, DONT, LINEMODE,
    };
    out_raw(init_seq, ARRAYLEN(init_seq));
}
