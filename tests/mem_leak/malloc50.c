/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void tmp(){

  int *p = PLKMALLOC(1);
  int j;
  for(j = 0; j < 10; j++){
    if(j > 2){}
    else return;
    
  }

  free(p);

}


int main(){

  tmp();

}
