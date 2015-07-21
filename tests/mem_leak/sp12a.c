/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int G;
void main() {
	int i, *p = PLKMALLOC(100);
	for (i = 0; i <= 10; i++) {
		if (G) {
			free(p);
			return;
		}
	}
}
