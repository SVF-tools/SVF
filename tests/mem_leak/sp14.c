/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

// TC14: recursion 
int * make(int n) {
	int *y = SAFEMALLOC(10);
	if (n > 0) {
		free(y);
		return make(n - 1);
	} else {
		free(y);
		return SAFEMALLOC(10);
	}
}

void main() {
	int *x = make(10);
	free(x);
}

