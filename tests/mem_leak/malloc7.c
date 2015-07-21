/*
 * pass value to globals
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int *g;
int main(){

	int *p = SAFEMALLOC(1);
	g = p;
	printf("%d,%d",g,p);
}
