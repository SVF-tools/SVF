#include "aliascheck.h"

/*
 * inheritance relation:
 *
 *       +-------- B <---+
 *       | virtual       |
 * A <---+               +--- D
 *       | virtual       |
 *       +-------- C <---+
 *
 */

int global_obj_f1;
int *global_ptr_f1 = &global_obj_f1;
int global_obj_f2;
int *global_ptr_f2 = &global_obj_f2;

int global_obj_g1;
int *global_ptr_g1 = &global_obj_g1;
int global_obj_g2;
int *global_ptr_g2 = &global_obj_g2;

int global_obj_h1;
int *global_ptr_h1 = &global_obj_h1;
int global_obj_h2;
int *global_ptr_h2 = &global_obj_h2;

int global_obj_l1;
int *global_ptr_l1 = &global_obj_l1;
int global_obj_l2;
int *global_ptr_l2 = &global_obj_l2;

class A {
  public:
    virtual void f1(int *i) { MUSTALIAS(global_ptr_f1, i); }
    virtual void f2(int *i) { MUSTALIAS(global_ptr_f2, i); }
};

class B: public virtual A {
  public:
    virtual void g1(int *i) { MUSTALIAS(global_ptr_g1, i); }
    virtual void g2(int *i) { MUSTALIAS(global_ptr_g2, i); }
};

class C: public virtual A {
  public:
    virtual void h1(int *i) { MUSTALIAS(global_ptr_h1, i); }
    virtual void h2(int *i) { MUSTALIAS(global_ptr_h2, i); }
};

class D: public B, public C {
  public:
    virtual void l1(int *i) { MUSTALIAS(global_ptr_l1, i); }
    virtual void l2(int *i) { MUSTALIAS(global_ptr_l2, i); }
};


int main(int argc, char **argv)
{
  int *ptr_f1 = &global_obj_f1;
  int *ptr_f2 = &global_obj_f2;
  int *ptr_g1 = &global_obj_g1;
  int *ptr_g2 = &global_obj_g2;
  int *ptr_h1 = &global_obj_h1;
  int *ptr_h2 = &global_obj_h2;
  int *ptr_l1 = &global_obj_l1;
  int *ptr_l2 = &global_obj_l2;

  A *pa;
  B *pb;
  C *pc;
  D *pd;

  D d;

  pa = &d;
  pa->f1(ptr_f1);
  pa->f2(ptr_f2);

  pb = &d;
  pb->g1(ptr_g1);
  pb->g2(ptr_g2);

  pc = &d;
  pc->h1(ptr_h1);
  pc->h2(ptr_h2);

  pd = &d;
  pd->l1(ptr_l1);
  pd->l2(ptr_l2);

  return 0;
}
