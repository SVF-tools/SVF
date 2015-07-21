/*
 * pass values to globals
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int func(){
	static int* q;
	int *p = SAFEMALLOC(10);
	q = p;
	printf("%d%d",p,q);
	//free(p);
}

int main(){
	func();
}
