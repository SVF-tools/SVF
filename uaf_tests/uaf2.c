#include "doublefree_check.h"

int main(){
    int *i = SAFEMALLOC(sizeof(int));
    int *b = SAFEMALLOC(sizeof(int));
    SAFEFREE(i);
    i = DOUBLEFREEMALLOC(sizeof(int));
    SAFEFREE(i);
    USEAFTERFREE(i);
    SAFEFREE(b);
}