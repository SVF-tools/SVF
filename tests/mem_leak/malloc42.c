/*
 * Safe malloc and double free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int main(){
 int *p,*q,i;

   while(1){
	   p = SAFEMALLOC(1);
    if(i){
      q = p;
	  break;
	}

	free(p);
   }

   free(q);

}
