#include "aliascheck.h"

int global_obj_b;
int *global_ptr_b = &global_obj_b;

int global_obj_c;
int *global_ptr_c = &global_obj_c;

class A {
  public:
    virtual void f(int *i) {
      NOALIAS(global_ptr_b, i);
      NOALIAS(global_ptr_c, i);
    }
};

class B: public A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr_b, i);
      MUSTALIAS(global_ptr_c, i);
    }
};

class C: public A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr_b, i);
      MUSTALIAS(global_ptr_c, i);
    }
};

class D: public B, public C {
};

int main()
{
  int *ptr_b = &global_obj_b;
  int *ptr_c = &global_obj_c;

  D d;
  ////// if D has not implemented its own f(), this will cause an error
  //d.f();
  
  /////// error
  //A *a = &d;
  //a->f();

  B *b = &d;
  b->f(ptr_b);

  C *c = &d;
  c->f(ptr_c);

  return 0;
}
