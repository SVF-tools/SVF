/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

//TC01: inter-procedural argument passing 
#include "aliascheck.h"
int ResourceLeak_TC01(int *p) {
	char str[10] = "STRING";
	if (p == NULL)
		return -1;

	strcat(p, str);
	printf(" %s \n", p);
	free(p);
	return 0;
}

int main() {
	char *p;
	p = SAFEMALLOC(10);
	ResourceLeak_TC01(p);
	return 0;
}

