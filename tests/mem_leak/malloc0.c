/*
 * Never free leak
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include "aliascheck.h"

void f(int*a, int*b);
int main(){

    int *a = NFRMALLOC(sizeof(int));
    int *b = NFRMALLOC(sizeof(int));
	int *k  = a;
    *b = 200;
    *a =100;
    *a = 100;
    f(a,b);
}

void f(int*a, int*b){
    *a = 300;
    *b = 200;

}
