/*
 * Partial leak
 * Author: Yule Sui
 * Date: 02/04/2014
 */

// from spec 188.ammp unonbon.c line 110

#include "aliascheck.h"

int func(){
	int i;
	int* atms = PLKMALLOC(10);

//	if(atms==0)
//		return 0;

	while(i > 10){
	if(i > 1)
		return 0;
	}

	free(atms);
}


int main(){


	func();
}
