/* LINTLIBRARY */

#include <stdio.h>
#include "CUDD/util.h"


/*
 *  util_print_time -- massage a long which represents a time interval in
 *  milliseconds, into a string suitable for output
 *
 *  Hack for IBM/PC -- avoids using floating point
 */

char *
util_print_time(unsigned long t)
{
    static char s[40];

    (void) sprintf(s, "%lu.%02lu sec", t/1000, (t%1000)/10);
    return s;
}
