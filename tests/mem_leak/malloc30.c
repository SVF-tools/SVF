/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int main(){

	int i = 10;
	int *p = (int*)SAFEMALLOC(100);
	int *q = (int*)(p + i );
		free(q);
	printf("%d%d",p,q);

}
