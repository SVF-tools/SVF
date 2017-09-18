#include "aliascheck.h"
#include <iostream>
#include <map>

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

  map<int, A> amap;
  A a;
  amap.insert(pair<int, A>(0, a));

  map<int, A>::const_iterator it = amap.begin();
  const A *aptr = &(it->second);
  aptr->f(ptr);

  return 0;
}
