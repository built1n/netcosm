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

#include "client.h"
#include "telnet.h"

static uint16_t term_width, term_height;

uint16_t telnet_get_width(void)
{
    return term_width;
}

uint16_t telnet_get_height(void)
{
    return term_height;
}

int telnet_handle_command(const unsigned char *buf, size_t buflen)
{
    bool found_cmd = false;
    bool in_sb = false;

    for(unsigned i = 0; i < buflen; ++i)
    {
        unsigned char c = buf[i];

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
            {  NAWS,    "NAWS"    },
            {  IP,      "IP"      },
        };

        for(unsigned int cmd_idx = 0; cmd_idx < ARRAYLEN(commands); ++cmd_idx)
        {
            if(c == commands[cmd_idx].val)
            {
                debugf("%s ", commands[cmd_idx].name);
                found_cmd = true;
                switch(c)
                {
                case IP:
                    return TELNET_EXIT;
                case SB:
                    in_sb = true;
                    break;
                case NAWS:
                    if(in_sb)
                    {
                        /* read height/width */
                        uint8_t bytes[4];
                        int j = 0;
                        while(j < 4 && i < buflen)
                        {
                            bytes[j++] = buf[++i];
                            sig_debugf("read byte %d %d\n", buf[i], j);
                            if(bytes[j - 1] == 255) /* 255 is doubled */
                            {
                                ++i;
                            }
                        }
                        term_width = ntohs(*((uint16_t*)bytes));
                        term_height = ntohs(*((uint16_t*)(bytes+2)));
                        sig_debugf("term size: %dx%d\n", term_width, term_height);
                    }
                    break;
                }
                goto got_cmd;
            }
        }
        debugf("???: %d ", c);
    got_cmd:
        ;
    }

    if(found_cmd)
        debugf("\n");

    return TELNET_OK;
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

        IAC, DO, NAWS,

        IAC, WONT, STATUS,
        IAC, DONT, STATUS,

        IAC, DO,   ECHO,
        IAC, WONT, ECHO,

        IAC, DONT, LINEMODE,
    };
    term_width = 80;
    term_height = 24;

    out_raw(init_seq, ARRAYLEN(init_seq));
}
