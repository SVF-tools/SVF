/* LINTLIBRARY */

#include <stdio.h>
#include "CUDD/util.h"

static int check_file (char const *, char const *);

char *
util_path_search(char const *prog)
{
#ifdef UNIX
    return util_file_search(prog, getenv("PATH"), (char *) "x");
#else
    return util_file_search(prog, NIL(char), (char *) "x");
#endif
}


char *
util_file_search(
  char const *file,		/* file we're looking for */
  char *path,			/* search path, colon separated */
  char const *mode		/* "r", "w", or "x" */)
{
    int quit;
    char *buffer, *filename, *save_path, *cp;

    if (path == 0 || strcmp(path, "") == 0) {
	path = (char *) ".";	/* just look in the current directory */
    }

    save_path = path = strsav(path);
    quit = 0;
    do {
	cp = strchr(path, ':');
	if (cp != 0) {
	    *cp = '\0';
	} else {
	    quit = 1;
	}

	/* cons up the filename out of the path and file name */
	if (strcmp(path, ".") == 0) {
	    buffer = strsav(file);
	} else {
	    buffer = ALLOC(char, strlen(path) + strlen(file) + 4);
	    (void) sprintf(buffer, "%s/%s", path, file);
	}
	filename = util_tilde_expand(buffer);
	FREE(buffer);

	/* see if we can access it */
	if (check_file(filename, mode)) {
	    FREE(save_path);
	    return filename;
	}
	FREE(filename);
	path = ++cp;
    } while (! quit); 

    FREE(save_path);
    return 0;
}


static int
check_file(char const *filename, char const *mode)
{
#ifdef UNIX
    int access_mode = /*F_OK*/ 0;

    if (strcmp(mode, "r") == 0) {
	access_mode = /*R_OK*/ 4;
    } else if (strcmp(mode, "w") == 0) {
	access_mode = /*W_OK*/ 2;
    } else if (strcmp(mode, "x") == 0) {
	access_mode = /*X_OK*/ 1;
    }
    return access(filename, access_mode) == 0;
#else
    FILE *fp;
    int got_file;

    if (strcmp(mode, "x") == 0) {
	mode = "r";
    }
    fp = fopen(filename, mode);
    got_file = (fp != 0);
    if (fp != 0) {
	(void) fclose(fp);
    }
    return got_file;
#endif
}
