#include "aliascheck.h"
#include <iostream>
#include <queue>

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

  queue<A> aqueue;
  A a;
  aqueue.push(a);

  const A *aptr = &(aqueue.front());
  aqueue.pop();
  aptr->f(ptr);

  return 0;
}
