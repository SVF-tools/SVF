/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int* foo(int *p){

	return p;
}

void bar(int *s){
	free(s);
}
int main(){
	int *k;	
	int *a = SAFEMALLOC(sizeof(int));
	k = foo(a);
	*a = 100;
	bar(k);
	printf("%d,%d,%d,%d,",*k,*a,a,k);
}


