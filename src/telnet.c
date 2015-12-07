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
                printf("%s ", commands[i].name);
                cmd = true;
                goto found;
            }
        }
        printf("??? ");
    found:

        ++buf;
    }

    if(cmd)
        printf("\n");
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
        IAC, DO, ECHO,
        IAC, WONT, ECHO,
    };
    out_raw(init_seq, ARRAYLEN(init_seq));
}
