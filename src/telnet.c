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

#define TELCMDS /* see <arpa/telnet.h> */
#define TELOPTS

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

enum telnet_status telnet_parse_data(unsigned char *buf, size_t buflen)
{
    bool iac = false;
    bool found_cmd = false;
    bool in_sb = false;
    bool line_done = false;

    for(unsigned i = 0; i < buflen; ++i)
    {
        unsigned char c = buf[i];

        if(c == IAC)
            iac = true;
        else if(c == '\n' || c == '\r')
            line_done = true;
        else if(c == '\0') // ignore NULLs
            buf[i] = ' ';

        if(iac)
        {
            if(TELCMD_OK(c))
            {
                found_cmd = true;
                switch(c)
                {
                case IP:
                    return TELNET_EXIT;
                case SB:
                    in_sb = true;
                    break;
                case TELOPT_NAWS:
                    if(in_sb)
                    {
                        /* read height/width */
                        uint8_t bytes[4];
                        int j = 0;
                        while(j < 4 && i < buflen)
                        {
                            bytes[j++] = buf[++i];
                            //debugf("%d ", buf[j - 1]);
                            if(bytes[j - 1] == 255) /* 255 is doubled to distinguish from IAC */
                            {
                                ++i;
                            }
                        }
                        if(i >= buflen && j != 4)
                            error("client SB NAWS command to short");
                        term_width = ntohs(*((uint16_t*)bytes));
                        term_height = ntohs(*((uint16_t*)(bytes+2)));
                    }
                    break;
                }
                continue;
            }
        }
    }

    return found_cmd ? TELNET_FOUNDCMD :
        (line_done ? TELNET_LINEOVER : TELNET_DATA);
}

void telnet_echo_off(void)
{
    const unsigned char seq[] = {
        IAC, WILL, TELOPT_ECHO,
        IAC, DONT, TELOPT_ECHO,
    };
    out_raw(seq, ARRAYLEN(seq));
}

void telnet_echo_on(void)
{
    const unsigned char seq[] = {
        IAC, WONT, TELOPT_ECHO,
        IAC, DO,   TELOPT_ECHO,
    };
    out_raw(seq, ARRAYLEN(seq));
}

void telnet_init(void)
{
    const unsigned char init_seq[] = {
        IAC, WONT, TELOPT_SGA,
        IAC, DONT, TELOPT_SGA,

        IAC, DO, TELOPT_NAWS,

        IAC, WONT, TELOPT_STATUS,
        IAC, DONT, TELOPT_STATUS,

        IAC, DO,   TELOPT_ECHO,

        IAC, DONT, TELOPT_LINEMODE,
    };
    term_width = 80;
    term_height = 24;

    out_raw(init_seq, ARRAYLEN(init_seq));
}

void telnet_clear_screen(void)
{
    /* ESC ] 2 J */
    unsigned char clear_seq[] = { '\033',
                                  ']',
                                  '2',
                                  'J' };
    out_raw(clear_seq, ARRAYLEN(clear_seq));
}
