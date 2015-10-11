/*
 * Name:            verbosity.h
 *
 * Purpose:         Verbosity control macros
 *
 * Author:          Conor P. Cahill
 *
 */

#ifndef VERBOSITY_H_INCLUDED

#define VERBOSITY_H_INCLUDED 1

#define V_LOW       1
#define V_MED       2
#define V_MEDIUM    V_MED
#define V_HIGH      3
#define V_ULTRA     4

void        VerbosityTimeNow( FILE * fp);
extern int verbose;

#define VERBOSE_SET(lvl)    verbose = lvl
#define VERBOSE_FP      stdout

#define VERBOSE(lvl, ...) \
        ( verbose >= lvl  ? \
          VerbosityTimeNow(VERBOSE_FP), \
          fprintf(VERBOSE_FP, __VA_ARGS__ ), \
          fputc('\n',VERBOSE_FP), \
          fflush(VERBOSE_FP) : 0)

#endif /* VERBOSITY_H_INCLUDED */
