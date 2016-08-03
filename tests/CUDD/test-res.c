#include <stdio.h>
#include "CUDD/util.h"


main(argc, argv, environ)
int argc;
char **argv;
char **environ;
{
    int i;
    char **ep, *prog;

    prog = util_path_search(argv[0]);
    if (prog == NIL(char)) {
	(void) fprintf(stderr, "Cannot find current executable\n");
	exit(1);
    }
    util_restart(prog, "a.out", 0);

    i = recur(10);
    (void) fprintf(stderr, "terminated normally with i = %d\n", i);

    (void) printf("argc is %d\n", argc);

    for(i = 0, ep = argv; *ep != 0; i++, ep++) {
	(void) printf("%08x (%08x-%08x)\targv[%d]:\t%s\n", 
	    ep, *ep, *ep + strlen(*ep), i, *ep);
    }

    i = 0;
    for(i = 0, ep = environ; *ep != 0; ep++, i++) {
	(void) printf("%08x (%08x-%08x)\tenviron[%d]:\t%s\n", 
	    ep, *ep, *ep + strlen(*ep), i, *ep);
    }

    (void) fprintf(stderr, "returning with status=4\n");
    return 4;
}


recur(cnt)
{
    int i, j, sum;

    if (cnt > 0) {
	return recur(cnt-1);
    } else {
	sum = 0;
	for(j = 0; j < 20; j++) {
	    for(i = 0; i < 100000; i++) {
	       sum += 1;
	    }
	    (void) printf("done loop %d\n", j);
	}
	return sum;
    }
}
