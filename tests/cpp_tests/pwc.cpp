#include "aliascheck.h"

#define LEN 100

int global_obj_f;
int *global_ptr_f = &global_obj_f;

int global_obj_g;
int *global_ptr_g = &global_obj_g;

class A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
    }
    virtual void g(int *i) {
      NOALIAS(global_ptr_f, i);
      MUSTALIAS(global_ptr_g, i);
    }
    double *f1;
    int *f2;
    int *f3;
};

int main(int argc, char** argv)
{
  int *ptr_f = &global_obj_f;
  int *ptr_g = &global_obj_g;

  A a_array[LEN];
  A *pa;

  pa = a_array;
  for (int i = 0; i < LEN/2; ++i)
    pa += 1;

  //NOALIAS(&(pa->f2), &(pa->f3));

  pa->f(ptr_f);
  pa->g(ptr_g);

  return 0;
}
