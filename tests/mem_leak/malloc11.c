/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int* allocation(){
	int k;
	if(k<0)
		return SAFEMALLOC(1);
	else
		return SAFEMALLOC(2);

}

int main(){

	int* p = allocation();
	free(p);
}

