/*
 * Safe malloc and never free
 * 
 * never free of bar1(y), which requires heap cloning analysis
 * This will issue a false negtive FN
 * 
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void bar1(int** b){
	*b = LEAKFN(10); // FN
}

void barfree(int** a){
	free(*a);
}

int* foo(){
	int** x = SAFEMALLOC(10);
	int** y = SAFEMALLOC(10);
	bar1(x);
	barfree(x);
	free(x);

	bar1(y);
	//barfree(y);
	free(y);

	printf("%d",x,y);
}


int main(){


	 foo();

}
