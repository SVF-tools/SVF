#include "aliascheck.h"

/*
 * inheritance relation:
 *
 *       +-------- B <---+
 *       | virtual       |
 * A <---+               |
 *       | virtual       |
 *       +-------- C <---+--- E
 *                       |
 *                 D <---+
 *
 */

int global_obj_f;
int *global_ptr_f = &global_obj_f;

int global_obj_g;
int *global_ptr_g = &global_obj_g;

int global_obj_h;
int *global_ptr_h = &global_obj_h;

int global_obj_l;
int *global_ptr_l = &global_obj_l;

int global_obj_m;
int *global_ptr_m = &global_obj_m;

class A {
  public:
    virtual void f(int *i) {
      MUSTALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
      NOALIAS(global_ptr_h, i);
      NOALIAS(global_ptr_l, i);
      NOALIAS(global_ptr_m, i);
    }
};

class B: public virtual A {
  public:
    virtual void g(int *i) {
      NOALIAS(global_ptr_f, i);
      MUSTALIAS(global_ptr_g, i);
      NOALIAS(global_ptr_h, i);
      NOALIAS(global_ptr_l, i);
      NOALIAS(global_ptr_m, i);
    }
};

class C: public virtual A {
  public:
    virtual void h(int *i) {
      NOALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
      MUSTALIAS(global_ptr_h, i);
      NOALIAS(global_ptr_l, i);
      NOALIAS(global_ptr_m, i);
    }
};

class D {
  public:
    virtual void l(int *i) {
      NOALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
      NOALIAS(global_ptr_h, i);
      MUSTALIAS(global_ptr_l, i);
      NOALIAS(global_ptr_m, i);
    }
};

class E: public B, public C, public D {
  public:
    virtual void m(int *i) {
      NOALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
      NOALIAS(global_ptr_h, i);
      NOALIAS(global_ptr_l, i);
      MUSTALIAS(global_ptr_m, i);
    }
};

int main(int argc, char **argv)
{
  int *ptr_f = &global_obj_f;
  int *ptr_g = &global_obj_g;
  int *ptr_h = &global_obj_h;
  int *ptr_l = &global_obj_l;
  int *ptr_m = &global_obj_m;

  A *pa;
  B *pb;
  C *pc;
  D *pd;
  E *pe;
  
  E e;

  pa = &e;
  pa->f(ptr_f);

  pb = &e;
  pb->g(ptr_g);

  pc = &e;
  pc->h(ptr_h);

  pd = &e;
  pd->l(ptr_l);

  pe = &e;
  pe->m(ptr_m);

  return 0;
}
