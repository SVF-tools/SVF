#include "aliascheck.h"
#include <list>

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

  list<const A*> alist;
  A a;
  alist.push_back(&a);

  const A *aptr = alist.front();

  aptr->f(ptr);

  return 0;
}
