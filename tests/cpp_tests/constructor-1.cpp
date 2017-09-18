#include "aliascheck.h"

int global_obj;
int *global_ptr = &global_obj;

class A {
  public:
    A(int *i) { f(i); }
    virtual void f(int *i) { MUSTALIAS(global_ptr, i); }
};

class B: public A {
  public:
    B(int *i): A(i) { f(i); }
    virtual void f(int *i) { MUSTALIAS(global_ptr, i); }
};

int main(void)
{
  int *i = &global_obj;

  B b(i);

  return 0;
}
