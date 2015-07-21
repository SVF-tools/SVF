/*
 * Safe malloc for program exit
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
/// extract from 177.vpr

#include "aliascheck.h"
int main(){
 int i;
 int *a = SAFEMALLOC(10);
 for(i = 0; i < 10; i++)
 if(i){
    exit(0);
 }

 free(a);
}
