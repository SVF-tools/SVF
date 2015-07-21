/*
 * malloc62.c
 *
 *  false positive : path correlation in loop
 *  Created on: May 1, 2014
 *      Author: Yulei Sui
 */

#include "aliascheck.h"

int * main(int size){
	int flag = 0;
	int *p,c = 10;
	while(flag == 0 && c < 0){
		if(flag == 0){
			p = PLKLEAKFP(1);
			flag = 1;
		}
		else{
			free(p);
			flag = 0;
		}
		c--;
	}
}


