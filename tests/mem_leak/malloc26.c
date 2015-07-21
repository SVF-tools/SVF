/*
 * Safe malloc
 * We will issue a false positive
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from 175.vpr at util.c file

#include "aliascheck.h"

void** alloc_matrix(){

	int i; char** cptr;
	cptr = (char**) SAFEMALLOC(10);
	for(i = 0; i < 10; i++){
		cptr[i] = (char*) PLKLEAKFP(1); /// False positive
	}
	//cptr+=i;
	return ((void**)cptr);
}

void free_matrix(void ** matrix){
	int i;
	for(i = 0; i < 10; i++){
		free(matrix[i]);
	}
	free(matrix);

}

int main(){


   char** dir_list;
   dir_list = (char**)alloc_matrix();	
   free_matrix(dir_list);

}
