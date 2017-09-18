#include "aliascheck.h"
#include <iostream>
#include <set>

using namespace std;

int global_obj;
int *global_ptr = &global_obj;

class A {
  public:
    A(int i): data(i) {}

    int getData() const {
      return data;
    }

    bool operator == (const A &a) const {
      return data == a.getData();
    }

    bool operator != (const A &a) const {
      return data != a.getData();
    }

    bool operator < (const A &a) const {
      return data < a.getData();
    }

    bool operator > (const A &a) const {
      return data > a.getData();
    }

    /// virtual function
    virtual void f(int *i) const {
      MUSTALIAS(global_ptr, i);
    }

  private:
    int data;
};

int main(int argc, char **argv)
{
  int *ptr = &global_obj;

  set<A> aset;
  A a(0);
  aset.insert(a);

  set<A>::const_iterator it = aset.begin();
  const A *aptr = &(*it);
  aptr->f(ptr);

  return 0;
}
