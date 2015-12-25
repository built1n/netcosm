#include "netcosm.h"

void remove_cruft(char *str)
{
    char *junk;
    strtok_r(str, "\r\n", &junk);
}
