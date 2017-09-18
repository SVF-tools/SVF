#include "aliascheck.h"

int global_int_obj;
int *global_int_ptr = &global_int_obj;


float global_float_obj;
float *global_float_ptr = &global_float_obj;


class A {
  public:
  virtual void f(int *i) {
    MUSTALIAS(global_int_ptr, i);
    NOALIAS(global_float_ptr, i);
  }
  virtual void g(float *j) {
    NOALIAS(global_int_ptr, j);
    MUSTALIAS(global_float_ptr, j);
  }
};

class B: virtual public A {
  public:
  virtual void f(int *i) {
    MUSTALIAS(global_int_ptr, i);
    NOALIAS(global_float_ptr, i);
  }
  virtual void g(float *j) {
    NOALIAS(global_int_ptr, j);
    MUSTALIAS(global_float_ptr, j);
  }
};

int main(int argc, char **argv)
{
  int *i = &global_int_obj;
  float *j = &global_float_obj;

  A *p = new B;

  p->f(i);
  p->g(j);

  return 0;
}
