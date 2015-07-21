/*
 * Never free
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from spec 188.ammp unonbon.c line 110

#include "aliascheck.h"

int func(){
	int i;
	int* atms = NFRMALLOC(10);

	if(atms==0)
		return 0;


	for(i = 0; i < 10; i++){
		if(i > 1)
			break;	
		//free(atms);
	}

}


int main(){


	func();
}
