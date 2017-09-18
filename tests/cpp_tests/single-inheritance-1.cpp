#include "aliascheck.h"

int global_obj;
int *global_ptr = &global_obj;

class A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr, i);
    }
};

class B: public A {
};

int main(int argc, char **argv)
{
  int *ptr = &global_obj;

  A *pb = new B;
  pb->f(ptr);

  return 0;
}
