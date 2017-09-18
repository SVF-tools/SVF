#include "aliascheck.h"

int global_obj;
int *global_ptr = &global_obj;

class A {
  public:
    A(int *i): aptr(i) {}
    virtual ~A() { f(); }
    virtual void f() { MUSTALIAS(global_ptr, aptr); }
  private:
    int *aptr;
};

class B: public A {
  public:
    B(int *i): A(i), bptr(i) {}
    virtual ~B() { f(); }
    virtual void f() { MUSTALIAS(global_ptr, bptr); }
  private:
    int *bptr;
};

int main(void)
{
  int *i = &global_obj;

  B *b = new B(i);

  delete b;

  return 0;
}
