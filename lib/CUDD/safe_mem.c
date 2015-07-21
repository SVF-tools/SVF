/* LINTLIBRARY */

#include <stdio.h>
#include "CUDD/util.h"

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


void
MMfree(char *obj)
{
    if (obj != 0) {
	free(obj);
    }
}
