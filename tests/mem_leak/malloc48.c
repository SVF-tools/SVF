/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int main() {
	int *p = PLKMALLOC(1);
	printf("111");

	int a;
	switch (a) {
	case 1:
		printf("111");
		break;
	case 2:
		printf("112");
		return 0;
		break;
	}
	free(p);
	return 0;
}
