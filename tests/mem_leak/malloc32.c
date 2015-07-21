/*
 * Safe malloc and never free
 * 
 * can not detect memory leak without heap cloning
 * we will issue a false negative FN
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void bar1(int** b){
	*b = LEAKFN(10);  // FN
}


int* foo(){
	int** x = SAFEMALLOC(10);
	int** y = SAFEMALLOC(10);
	bar1(x);
	free(*x);
	free(x);

	bar1(y);
	free(y);

	printf("%d",x,y);
}


int main(){


	 foo();

}
