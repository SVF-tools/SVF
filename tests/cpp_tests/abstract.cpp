#include "aliascheck.h"

int global_obj_f;
int *global_ptr_f = &global_obj_f;

int global_obj_g;
int *global_ptr_g = &global_obj_g;

class A {
  public:
    virtual void f(int *i) = 0;
    virtual void g(int *i) {
      NOALIAS(global_ptr_f, i);
      MUSTALIAS(global_ptr_g, i);
    }
};

class B: public A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
    }
    virtual void g(int *i) {
      NOALIAS(global_ptr_f, i);
      MUSTALIAS(global_ptr_g, i);
    }
};

int main()
{
  int *ptr_f = &global_obj_f;
  int *ptr_g = &global_obj_g;

  A *a = new B;
  a->f(ptr_f);
  a->g(ptr_g);

  return 0;
}
