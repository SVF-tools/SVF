/*
 * Partial leak
 * Author: Yule Sui
 * Date: 02/04/2014
 */

// file from spec2000 188.ammp

#include "aliascheck.h"

int func(){

	int* p = PLKMALLOC(1);

	if(!p)
		return -1;
	
	int *q = SAFEMALLOC(1);

	if(!q)
		return -1;

	free(p);

	free(q);

}


int main(){

	func();
}
