/*
 * Safe malloc, value passed to global
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int* a[100];
int main(){

	//int* a[100];
	int i;
	a[i] = SAFEMALLOC(10);
	printf("%d",a[i]);

}
