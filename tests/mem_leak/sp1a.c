/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

//TC01: inter-procedural argument passing


int ResourceLeak_TC01(int *p) {
	char str[10] = "STRING";
	if (p == NULL)
		return -1;

	strcat(p, str);
	printf(" %s \n", p);
	return 0;
}

int main() {
	char *p;
	p = NFRMALLOC(10);
	ResourceLeak_TC01(p);
	return 0;
}
