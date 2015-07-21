/* LINTLIBRARY */

#include <stdio.h>
#include "CUDD/util.h"

#ifdef BSD
#include <pwd.h>
#endif


char *
util_tilde_expand(char const *fname)
{
#ifdef BSD
    struct passwd *userRecord;
    char username[256], *filename;
    register int i, j;

    filename = ALLOC(char, strlen(fname) + 256);

    /* Clear the return string */
    i = 0;
    filename[0] = '\0';

    /* Tilde? */
    if (fname[0] == '~') {
	j = 0;
	i = 1;
	while ((fname[i] != '\0') && (fname[i] != '/')) {
	    username[j++] = fname[i++];
	}
	username[j] = '\0';

	if (username[0] == '\0') {
	    /* ~/ resolves to home directory of current user */
	    if ((userRecord = getpwuid(getuid())) != 0) {
		(void) strcat(filename, userRecord->pw_dir);
	    } else {
		i = 0;
	    }
	} else {
	    /* ~user/ resolves to home directory of 'user' */
	    if ((userRecord = getpwnam(username)) != 0) {
		(void) strcat(filename, userRecord->pw_dir);
	    } else {
		i = 0;
	    }
	}
    }

    /* Concantenate remaining portion of file name */
    (void) strcat(filename, fname + i);
    return filename;
#else
    return strsav(fname);
#endif
}
