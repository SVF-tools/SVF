/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// TC11:relation of pointers 

#include "aliascheck.h"

int *aliasing(int **p) {
	*p = SAFEMALLOC(10);
	return *p;
}

int main() {
	int *pp, *t;
	t = aliasing(&pp);
	if (t != (void *) 0) {
		free(t);
	} else {
		exit(0);
	}
	return 1;
}

