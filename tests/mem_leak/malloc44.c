/*
 * Safe malloc and never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void foo(int** p, int *q){

	*p = q;
}

int main(){

	int *p1,*p2,*q1,*q2;
	q1=NFRMALLOC(1);
	q2=SAFEMALLOC(1);
	foo(&p1,q1);
	foo(&p2,q2);
	free(p2);


}
