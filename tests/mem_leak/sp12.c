/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

//TC12: infinite loop 
int G;
void main(){
	  int i, *p = PLKMALLOC(100);
	    for(i=0;i>=0;i++){
			 if(G){
				free(p);
				return;
			 }
	    }
}
