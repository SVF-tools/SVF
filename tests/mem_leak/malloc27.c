/*
 * Never free and safe malloc
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void alloc_indirect(int** p){
	//*p = 1000;
	*p = SAFEMALLOC(10);
}

void free_indirect(int ** q){
	free(*q);

}

void alloc_inin(int ***x){

	**x = NFRMALLOC(10);
}

void alloc(int **s){
	alloc_indirect(s);
}
int main(){
	int a;
	int *y = &a;
	int **x = &y;
//	int ***z = &x;
 //  alloc_indirect(x);
	alloc(x);
   free_indirect(x);
   printf("%d%d",y,a);
   alloc_inin(x);

}
