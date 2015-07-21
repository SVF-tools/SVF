/*
 * Safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */
// from 175.vpr at util.c file

#include "aliascheck.h"


void free_matrix(void ** matrix){
    matrix = malloc(10);
	int i;
    do{

    }while(i<10);
	free(matrix);

}

int main(){


   char** dir_list;
   //dir_list = (char**)alloc_matrix();	
   free_matrix(dir_list);

}
