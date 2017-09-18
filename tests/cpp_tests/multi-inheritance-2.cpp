#include "aliascheck.h"

int global_obj_f1;
int *global_ptr_f1 = &global_obj_f1;

int global_obj_f2;
int *global_ptr_f2 = &global_obj_f2;

int global_obj_g1;
int *global_ptr_g1 = &global_obj_g1;

int global_obj_g2;
int *global_ptr_g2 = &global_obj_g2;

class A {
  public:
  virtual void f1(int *i) {
    MUSTALIAS(global_ptr_f1, i);
    NOALIAS(global_ptr_f2, i);
    NOALIAS(global_ptr_g1, i);
    NOALIAS(global_ptr_g2, i);
  }
  virtual void g1(int *i) {
    NOALIAS(global_ptr_f1, i);
    NOALIAS(global_ptr_f2, i);
    MUSTALIAS(global_ptr_g1, i);
    NOALIAS(global_ptr_g2, i);
  }
};

class B {
  public:
  virtual void f2(int *i) {
    NOALIAS(global_ptr_f1, i);
    MUSTALIAS(global_ptr_f2, i);
    NOALIAS(global_ptr_g1, i);
    NOALIAS(global_ptr_g2, i);
  }
  virtual void g2(int *i) {
    NOALIAS(global_ptr_f1, i);
    NOALIAS(global_ptr_f2, i);
    NOALIAS(global_ptr_g1, i);
    MUSTALIAS(global_ptr_g2, i);
  }
};

class C: public A, public B {
  public:
  virtual void f1(int *i) {
    MUSTALIAS(global_ptr_f1, i);
    NOALIAS(global_ptr_f2, i);
    NOALIAS(global_ptr_g1, i);
    NOALIAS(global_ptr_g2, i);
  }
  virtual void g1(int *i) {
    NOALIAS(global_ptr_f1, i);
    NOALIAS(global_ptr_f2, i);
    MUSTALIAS(global_ptr_g1, i);
    NOALIAS(global_ptr_g2, i);
  }
  virtual void f2(int *i) {
    NOALIAS(global_ptr_f1, i);
    MUSTALIAS(global_ptr_f2, i);
    NOALIAS(global_ptr_g1, i);
    NOALIAS(global_ptr_g2, i);
  }
  virtual void g2(int *i) {
    NOALIAS(global_ptr_f1, i);
    NOALIAS(global_ptr_f2, i);
    NOALIAS(global_ptr_g1, i);
    MUSTALIAS(global_ptr_g2, i);
  }
};

int main(int argc, char **argv)
{
  int *ptr_f1 = &global_obj_f1;
  int *ptr_f2 = &global_obj_f2;
  int *ptr_g1 = &global_obj_g1;
  int *ptr_g2 = &global_obj_g2;

  A *pa;
  B *pb;
  C c;

  pa = &c;
  pa->f1(ptr_f1);
  pa->g1(ptr_g1);

  pb = &c;
  pb->f2(ptr_f2);
  pb->g2(ptr_g2);

  return 0;
}
