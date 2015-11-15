#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include "verbosity.h"

int     verbose;

void
VerbosityOut(FILE * fp, char * fmt, ... )
{
    struct timeval  tv;
    char            outbuf[256];
    va_list         args;

    /*
     * Process the format string... make sure it fits in buffer (will be trucated)
     */
    va_start(args, fmt);
    vsnprintf(outbuf, sizeof(outbuf), fmt, args);
    va_end(args);

    /*
     * Output with time of day in one buffer (hopefully not split this time).
     */
    gettimeofday(&tv, NULL);
    fprintf(fp, "%ld.%.6ld: %s\n", (long)tv.tv_sec, (long)tv.tv_usec, outbuf);
    fflush(fp);
}
