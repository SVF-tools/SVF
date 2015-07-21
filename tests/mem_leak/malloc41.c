/*
 * Never free and Safemalloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int* bar1(int* b){  
	  return b;
	    }
   
    
int* foo(int *x,int *y){
	     
	    int*k=bar1(x);
		    free(k);
			                      
			    int* t = bar1(y);
				    printf("%d",t);
}
                                       
                                        
int tmp(){
	                      
	                              
	  int* p = SAFEMALLOC(10);
	    int* q = NFRMALLOC(10);
		  foo(p,q);
		                                                   
}

int main(){

	  tmp();
}

