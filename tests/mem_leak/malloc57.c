/*
 * Safe malloc
 * Test post dominate of loop exit
 * Author: Yule Sui
 * Date: 02/04/2014
 */

#include <aliascheck.h>
int main(){
    int* newfile = SAFEMALLOC(190);
    int i = 0;
    if(newfile==NULL){
        return 0;
    }
    for( ; i < 10; i++)
    {
    }

    free(newfile);
    return 0;
}
