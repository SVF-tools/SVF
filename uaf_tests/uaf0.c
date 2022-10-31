#include "doublefree_check.h"

int main(){
    int *i = DOUBLEFREEMALLOC(sizeof(int));
    SAFEFREE(i);
    USEAFTERFREE(i);
}