#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "verbosity.h"

int     verbose;

void
VerbosityTimeNow(FILE * fp)
{
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    fprintf(fp, "%ld.%.6ld: ", (long)tv.tv_sec, (long)tv.tv_usec);
}
