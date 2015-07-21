/*
 * Never free
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int main(){


	int *p = NFRMALLOC(1);
	int i,*q;
	q = p + i;
	printf("%d%d",p,q);
}
