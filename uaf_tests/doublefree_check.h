#include <stdio.h>
#include <stdlib.h>


/// safe malloc
void* SAFEMALLOC(int n){
  return malloc(n);
}
/// double free malloc
void* DOUBLEFREEMALLOC(int n){
    return malloc(n);
}
/// double free malloc fn (output safe but should be double free)
void* DOUBLEFREEMALLOCFN(int n){
    return malloc(n);
}
/// safe malloc fn for double free (output double free but should be safe)
void* SAFEMALLOCFP(int n){
    return malloc(n);
}
/// safe free function
void* SAFEFREE(int * p){
    free(p);
}
/// double free
void* DOUBLEFREE(int * p){
    free(p);
}

// use after free
int *USEAFTERFREE(int * p){
    return *p;
}

// safe use function
int *SAFEUSE(int * p){
    return *p;
}
