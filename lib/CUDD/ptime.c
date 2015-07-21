/* LINTLIBRARY */
#include "CUDD/util.h"

/* backwards compatibility */
long 
ptime()
{
    return util_cpu_time();
}
