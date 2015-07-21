/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */

// file from spec2000 188.ammp

#include "aliascheck.h"

int func(){

	int* p = SAFEMALLOC(1);

	if(!p)
		return -1;
	
    if(p)
	free(p);

}


int main(){

	func();
}
