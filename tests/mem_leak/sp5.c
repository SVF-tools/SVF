/*
 * Safe malloc
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
//TC05: unclear conditioned 
#include "aliascheck.h"

int f(char *p)
{
    free(p);
    return 0;
}

int main(int arg1)
{
    char *p;
    p = (char *)SAFEMALLOC(sizeof(char)*10);

    if( arg1 ){
        f(p);
        return -1;	
    }
    else{
        f(p);
        return 1;	
    }
}
