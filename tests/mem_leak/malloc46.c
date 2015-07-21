/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void fun() {

	int *p = SAFEMALLOC(1);
	int *q = &p;

	int *r = p;

	free(r);
}

int main() {

	fun();
}

