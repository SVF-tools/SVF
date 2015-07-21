/*
 * Safe malloc and invalid free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int main(){

	int i = 3;
	int* p = 0; //=malloc(1);
	while(i>0){
		i--;
		p = SAFEMALLOC(1);
	}

	free(p);

	//	printf("%d",p);
}
