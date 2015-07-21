/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// TC02: aliasing pointer 

#include "aliascheck.h"

int main(int arg1) {
	char *p1;
	char *p2;
	int y = 10;
	p1 = (char *) PLKMALLOC(sizeof(char) * 10);

	if (p1 == 0)
		return 1;

	p2 = (char *) SAFEMALLOC(sizeof(char) * 10);
	if (p2 == 0) {
		if (p1 != 0) {
			free(p1);
		}
		return 2;
	}
	//free(p1);
	p1 = p2;
	free(p1);
	return 0;
}
