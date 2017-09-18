#include "aliascheck.h"

int global_obj_a;
int *global_ptr_a = &global_obj_a;

int global_obj_b;
int *global_ptr_b = &global_obj_b;

class A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr_a, i);
      NOALIAS(global_ptr_b, i);
    }
};

class B: virtual public A {
  public:
    virtual void f(int *i) {
      NOALIAS(global_ptr_a, i);
      MUSTALIAS(global_ptr_b, i);
    }
};

int main(int argc, char **argv)
{
  int *i = &global_obj_a;
  A *pa = new A;
  pa->f(i);


  int *j = &global_obj_b;
  B *pb = new B;
  pb->f(j);

  return 0;
}
