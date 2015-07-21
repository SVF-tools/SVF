/*
 * Context leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

int* bar1(int* b){  
  return b;
  }
   
    
int* foo(int *x){
     
    int*k=bar1(x);
    return k; 
}
                                       
                                        
int tmp(){
                      
                              
  int* p = SAFEMALLOC(10);
  int* q = CLKMALLOC(10);
  int*t =  foo(p);
  int*s = foo(q);
  free(t);
}

int main(){

  tmp();
}
