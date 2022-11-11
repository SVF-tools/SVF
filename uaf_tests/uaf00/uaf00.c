#include "doublefree_check.h"

int main(){
    int *i = DOUBLEFREEMALLOC(sizeof(int));
    int j = 0;
    if (j == 1){
        SAFEFREE(i);
    } else {
        SAFEFREE(i);
    }
    USEAFTERFREE(i);
}