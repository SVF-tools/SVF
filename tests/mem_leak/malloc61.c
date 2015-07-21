/*
 * malloc61.c
 *
 *  false positive inter-procedurel path correlation
 *  Created on: May 1, 2014
 *      Author: Yulei Sui
 */

#include "aliascheck.h"

int foo(int **p, int n){
	if(n == 0)
		return 0;
	else{
		*p = PLKLEAKFP(n);
		return n;
	}
}

int main(){
	int *q, m;
	int ret = foo(&q,m);

	if(ret!=0)
		free(q);

}

