/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int main(){

	int a;
	int *p = PLKMALLOC(10);
	*p = 10;
	if(1==p) return 0;
	if(p){
	printf("%d%d",p,*p);;
	}
	free(p);
}

