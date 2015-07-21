/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int func(){

	int* p = SAFEMALLOC(1);

	free(p);

	int *q = SAFEMALLOC(1);

	if(q==0)
		return -1;

	free(q);

}


int main(){


	func();
}
