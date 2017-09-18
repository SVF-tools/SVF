#include "aliascheck.h"
#include <iostream>
#include <set>

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

  set<const A*> aset;
  A a;
  aset.insert(&a);

  set<const A*>::const_iterator it = aset.begin();
  const A *aptr = *it;
  aptr->f(ptr);

  return 0;
}
