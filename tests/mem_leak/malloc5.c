/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void foo(int *p){
	free(p);
}

int main(){

	int *a,*b,*c;

	a = SAFEMALLOC(1);
	b = a;
	c = a;
	foo(b);
	foo(c);

}
