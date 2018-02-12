#include "aliascheck.h"
#include <iostream>
#include <forward_list>

using namespace std;

int global_obj;
int *global_ptr = &global_obj;

class A {
  public:
    virtual void f(int *i) const {
      MUSTALIAS(global_ptr, i);
    }
};

int main(int argc, char **argv)
{
  int *ptr = &global_obj;

  forward_list<const A*> alist;
  A *a = new A;

  alist.push_front(a);

  forward_list<const A*>::iterator it = alist.begin();
  const A *aptr = *it;

  aptr->f(ptr);

  return 0;
}
