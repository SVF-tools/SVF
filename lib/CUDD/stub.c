/* LINTLIBRARY */

#ifdef LACK_SYS5

char *
memcpy(s1, s2, n)
char *s1, *s2;
int n;
{
    extern bcopy();
    bcopy(s2, s1, n);
    return s1;
}

char *
memset(s, c, n)
char *s;
int c;
int n;
{
    extern bzero();
    register int i;

    if (c == 0) {
	bzero(s, n);
    } else {
	for(i = n-1; i >= 0; i--) {
	    *s++ = c;
	}
    }
    return s;
}

char *
strchr(s, c)
char *s;
int c;
{
    extern char *index();
    return index(s, c);
}

char *
strrchr(s, c)
char *s;
int c;
{
    extern char *rindex();
    return rindex(s, c);
}


#endif

#ifndef UNIX
#include <stdio.h>

FILE *
popen(string, mode)
const char *string;
const char *mode;
{
    (void) fprintf(stderr, "popen not supported on your operating system\n");
    return NULL;
}


int
pclose(fp)
FILE *fp;
{
    (void) fprintf(stderr, "pclose not supported on your operating system\n");
    return -1;
}
#endif

/* put something here in case some compilers abort on empty files ... */
int
util_do_nothing()
{
    return 1;
}
