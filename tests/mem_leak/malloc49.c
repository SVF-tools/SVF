/*
 * Partial leak
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void tmp(){
  int a,b;
  int *p = PLKMALLOC(1);

  if(a > 1){
    
    if(b < 2){
    }
    else
      free(p);

  }
}

int main(){

tmp();

}
