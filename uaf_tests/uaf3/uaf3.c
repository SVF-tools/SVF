#include "doublefree_check.h"

void foo(int *p){

    SAFEFREE(p);
}
void foo2(int *p){

    USEAFTERFREE(p);
}
int main(){
    int *i = DOUBLEFREEMALLOC(sizeof(int));
    foo(i);
    int *j = SAFEMALLOC(sizeof(int));
    foo(j);
    foo2(i);
}