/* LINTLIBRARY */

#include <stdio.h>
#include "CUDD/util.h"

#ifdef IBM_WATC		/* IBM Waterloo-C compiler (same as bsd 4.2) */
#define void int
#define BSD
#endif

#ifdef BSD
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#if defined(UNIX60) || defined(UNIX100) || defined(__CYGWIN32__)
#include <sys/types.h>
#include <sys/times.h>
#endif

#ifdef vms		/* VAX/C compiler -- times() with 100 HZ clock */
#include <types.h>
#include <time.h>
#endif



/*
 *   util_cpu_time -- return a long which represents the elapsed processor
 *   time in milliseconds since some constant reference
 */
long
util_cpu_time()
{
    long t = 0;

#ifdef BSD
    struct rusage rusage;
    (void) getrusage(RUSAGE_SELF, &rusage);
    t = (long) rusage.ru_utime.tv_sec*1000 + rusage.ru_utime.tv_usec/1000;
#endif

#ifdef IBMPC
    long ltime;
    (void) time(&ltime);
    t = ltime * 1000;
#endif

#ifdef UNIX60			/* times() with 60 Hz resolution */
    struct tms buffer;
    times(&buffer);
    t = buffer.tms_utime * 16.6667;
#endif

#ifdef UNIX100
    struct tms buffer;		/* times() with 100 Hz resolution */
    times(&buffer);
    t = buffer.tms_utime * 10;
#endif

#ifdef __CYGWIN32__
    /* Works under Windows NT but not Windows 95. */
    struct tms buffer;		/* times() with 1000 Hz resolution */
    times(&buffer);
    t = buffer.tms_utime;
#endif

#ifdef vms
    tbuffer_t buffer;	/* times() with 100 Hz resolution */
    times(&buffer);
    t = buffer.proc_user_time * 10;
#endif

    return t;
}


/*
 *  These are interface routines to be placed between a program and the
 *  system memory allocator.
 *
 *  It forces well-defined semantics for several 'borderline' cases:
 *
 *	malloc() of a 0 size object is guaranteed to return something
 *	    which is not 0, and can safely be freed (but not dereferenced)
 *	free() accepts (silently) an 0 pointer
 *	realloc of a 0 pointer is allowed, and is equiv. to malloc()
 *	For the IBM/PC it forces no object > 64K; note that the size argument
 *	    to malloc/realloc is a 'long' to catch this condition
 *
 *  The function pointer MMoutOfMemory() contains a vector to handle a
 *  'out-of-memory' error (which, by default, points at a simple wrap-up
 *  and exit routine).
 */

#ifdef __cplusplus
extern "C" {
#endif

extern char *MMalloc(long);
extern void MMout_of_memory(long);
extern char *MMrealloc(char *, long);

void (*MMoutOfMemory)(long) = MMout_of_memory;

#ifdef __cplusplus
}
#endif


/* MMout_of_memory -- out of memory for lazy people, flush and exit */
void
MMout_of_memory(long size)
{
    (void) fflush(stdout);
    (void) fprintf(stderr, "\nout of memory allocating %lu bytes\n",
                   (unsigned long) size);
    exit(1);
}


char *
MMalloc(long size)
{
    char *p;

#ifdef IBMPC
    if (size > 65000L) {
	if (MMoutOfMemory != (void (*)(long)) 0 ) (*MMoutOfMemory)(size);
	return NIL(char);
    }
#endif
    if (size == 0) size = sizeof(long);
    if ((p = (char *) malloc((unsigned long) size)) == NIL(char)) {
        if (MMoutOfMemory != 0 ) (*MMoutOfMemory)(size);
        return NIL(char);
    }
    return p;
}


char *
MMrealloc(char *obj, long size)
{
    char *p;

#ifdef IBMPC
    if (size > 65000L) {
	if (MMoutOfMemory != 0 ) (*MMoutOfMemory)(size);
	return NIL(char);
    }
#endif
    if (obj == NIL(char)) return MMalloc(size);
    if (size <= 0) size = sizeof(long);
    if ((p = (char *) realloc(obj, (unsigned long) size)) == NIL(char)) {
        if (MMoutOfMemory != 0 ) (*MMoutOfMemory)(size);
        return NIL(char);
    }
    return p;
}


/* $Id: datalimit.c,v 1.5 2007/08/24 18:17:31 fabio Exp fabio $ */

#ifndef HAVE_SYS_RESOURCE_H
#define HAVE_SYS_RESOURCE_H 1
#endif
#ifndef HAVE_SYS_TIME_H
#define HAVE_SYS_TIME_H 1
#endif
#ifndef HAVE_GETRLIMIT
#define HAVE_GETRLIMIT 1
#endif

#if HAVE_SYS_RESOURCE_H == 1
#if HAVE_SYS_TIME_H == 1
#include <sys/time.h>
#endif
#include <sys/resource.h>
#endif

#ifndef RLIMIT_DATA_DEFAULT
#define RLIMIT_DATA_DEFAULT 67108864	/* assume 64MB by default */
#endif

#ifndef EXTERN
#   ifdef __cplusplus
#	define EXTERN extern "C"
#   else
#	define EXTERN extern
#   endif
#endif

EXTERN unsigned long getSoftDataLimit(void);

unsigned long
getSoftDataLimit(void)
{
#if HAVE_SYS_RESOURCE_H == 1 && HAVE_GETRLIMIT == 1 && defined(RLIMIT_DATA)
    struct rlimit rl;
    int result;

    result = getrlimit(RLIMIT_DATA, &rl);
    if (result != 0 || rl.rlim_cur == RLIM_INFINITY)
        return((unsigned long) RLIMIT_DATA_DEFAULT);
    else
        return((unsigned long) rl.rlim_cur);
#else
    return((unsigned long) RLIMIT_DATA_DEFAULT);
#endif

} /* end of getSoftDataLimit */
