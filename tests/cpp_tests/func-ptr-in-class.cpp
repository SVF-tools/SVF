#include "aliascheck.h"

int global_obj_f;
int *global_ptr_f = &global_obj_f;

int global_obj_g;
int *global_ptr_g = &global_obj_g;

int global_obj_f_v;
int *global_ptr_f_v = &global_obj_f_v;

int global_obj_g_v;
int *global_ptr_g_v = &global_obj_g_v;

void f(int *i) {
  MUSTALIAS(global_ptr_f, i);
  NOALIAS(global_ptr_g, i);
  NOALIAS(global_ptr_f_v, i);
  NOALIAS(global_ptr_g_v, i);
}

void g(int *i) {
  NOALIAS(global_ptr_f, i);
  MUSTALIAS(global_ptr_g, i);
  NOALIAS(global_ptr_f_v, i);
  NOALIAS(global_ptr_g_v, i);
}

class A {
  public:
    virtual void f(int *i) {
      NOALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
      MUSTALIAS(global_ptr_f_v, i);
      NOALIAS(global_ptr_g_v, i);
    }
    virtual void g(int *i) {
      NOALIAS(global_ptr_f, i);
      NOALIAS(global_ptr_g, i);
      NOALIAS(global_ptr_f_v, i);
      MUSTALIAS(global_ptr_g_v, i);
    }
    void (*pf)(int *i);
    void (*pg)(int *i);
};

int main(void)
{
  int *ptr_f = &global_obj_f;
  int *ptr_g = &global_obj_g;
  int *ptr_f_v = &global_obj_f_v;
  int *ptr_g_v = &global_obj_g_v;

  A *a = new A;
  a->pf = &f;
  a->pg = &g;

  a->pf(ptr_f);
  a->pg(ptr_g);

  a->f(ptr_f_v);
  a->g(ptr_g_v);

  return 0;
}
