/*
 * Safe malloc because of infeasible paths
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
#include "aliascheck.h"

int main(){
	int n;
    int *p = PLKLEAKFP(10);
    if(n>0)
        free(p);
    else if(n<=0)
        free(p);
}
