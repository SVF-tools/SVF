#include <stdio.h>
#include <stdlib.h>

void MUSTALIAS(void* p, void* q){
  printf("\n");
}

void PARTAILALIAS(void* p, void* q){
  printf("\n");
}

void MAYALIAS(void* p, void* q){
  printf("\n");
}

void NOALIAS(void* p, void* q){
  printf("\n");
}

void EXPECTEDFAIL_MAYALIAS(void* p, void* q){
  printf("\n");
}

void EXPECTEDFAIL_NOALIAS(void* p, void* q){
  printf("\n");
}

// safe malloc
void* SAFEMALLOC(int n){
  return malloc(n);
}
// partial leak malloc
void* PLKMALLOC(int n){
  return malloc(n);
}
/// never free malloc
void* NFRMALLOC(int n){
  return malloc(n);
}
/// context leak malloc
void* CLKMALLOC(int n){
  return malloc(n);
}
/// expected never free false positive leak
void* NFRLEAKFP(int n){
  return malloc(n);
}
/// expected partial leak false positive leak
void* PLKLEAKFP(int n){
  return malloc(n);
}
/// expected leak false negative leak
void* LEAKFN(int n){
  return malloc(n);
}

// ****************************************************************************************
// RC_ACCESS is used for concurrency analysis validation.
// The target memory access is indicated as the closest memory access before its call site.
// The function calls of RC_ACCESS should always appear in pairs.
//   - Parameter #1 pairId: the id of pairs that RC_ACCESS occurs.
//   - Parameter #2 p: the validation flags, whose predefined values are shown below.
void RC_ACCESS(int id, int flags) {
  printf("\n");
}
// Predefined flag values for RC_ACCESS.
#define RC_MHP 0x01
#define RC_ALIAS 0x02
#define RC_PROTECTED 0x04
#define RC_RACE 0x10

