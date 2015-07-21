/*
 * Safe malloc and never free
 *
 * Author: Yule Sui
 * Date: 02/04/2014
 */
/// extract from 179.art

#include "aliascheck.h"
unsigned char** cimage;
int main(){
    int i,j;
    char* superbuffer = (char *)NFRMALLOC(100 * sizeof(char ));
    cimage = (unsigned char **) SAFEMALLOC(sizeof(unsigned char *) * 10);
    for (i=0;i<10;i++)
    {
        cimage[i] = (unsigned char *) SAFEMALLOC(10* sizeof(unsigned char));
    }

    for (i=0;i<10;i++)
    {
        for (j=0;j<10;j++)
        {
            cimage[i][j] = superbuffer[i*10 + j];
        }
    }

    return 0;
}
