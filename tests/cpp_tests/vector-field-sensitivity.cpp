/*
 * Simple alias check
 * Author: dye
 * Date: 07/12/2016
 */
#include "aliascheck.h"
#include <vector>

using namespace std;

class C {
public:
    int f1;
    int f2;
};

vector<C> g;


int main(int argc, char *argv[]) {

    C c_;
    g.push_back(c_);
    C &c = g[0];

    NOALIAS(&c_.f1, &c_.f2);
    NOALIAS(&c.f1, &c.f2);

    return 0;
}
