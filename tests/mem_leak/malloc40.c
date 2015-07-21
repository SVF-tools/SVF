/*
 * Never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void fun(){
	    
	    int *p = NFRMALLOC(1);
		int *q = &p;

		int *r = p;

//		free(r);
		fprintf("%d",r);
}

int main(){

	    fun();
}

