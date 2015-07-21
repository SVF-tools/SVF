/*
 * malloc63.c
 *
 *  false positive : path correlation in recursion
 *  Created on: May 1, 2014
 *      Author: Yulei Sui
 */

#include "aliascheck.h"
void goo(int* p, int flag, int c);

void foo(int *p, int flag, int c){
	if(flag == 0 && c < 0)
		return;

	if(flag == 0){
		p = PLKLEAKFP(1);
		flag = 1;
	}

	goo(p,flag,c);
}


void goo(int *p, int flag, int c){

	if(1 == flag){
		free(p);
		flag = 0;
	}
	c--;
	foo(p,flag,c);
}

int main(){
	int *p;
	foo(p,0,10);
}
