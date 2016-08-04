#include <stdio.h>
#include "CUDD/util.h"


/* ARGSUSED */
static int
saveit(prog, file2)
char *prog, *file2;
{
    char *file1;

    /* get current executable name by searching the path ... */
    file1 = util_path_search(prog);
    if (file1 == 0) {
	(void) fprintf(stderr, "cannot locate current executable\n");
	return 1;
    }

    /* users name for the new executable -- perform tilde-expansion */
    if (! util_save_image(file1, file2)) {
	(void) fprintf(stderr, "error occured during save ...\n");
	return 1;
    }
    FREE(file1);
    return 0;
}

int restart;

main(argc, argv)
char **argv;
{
    if (restart) {
	(void) printf("restarted ...\n");
	exit(0);
    }
    restart = 1;
    exit(saveit(argv[0], "foobar"));
}
