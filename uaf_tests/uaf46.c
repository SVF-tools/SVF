
#include "doublefree_check.h"
#include "std_testcase.h"
#include <wchar.h>


static void badSink(wchar_t * data)
{
    /* POTENTIAL FLAW: UAF */
    USEAFTERFREE(data);
}

void CWE415_Double_Free__malloc_free_wchar_t_41_bad()
{
    wchar_t * data;
    /* Initialize data */
    data = NULL;
    data = (wchar_t *)DOUBLEFREEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    badSink(data);
}


/* goodG2B uses the GoodSource with the BadSink */
static void goodG2BSink(wchar_t * data)
{
    /* POTENTIAL FLAW: Possibly freeing memory twice */
    SAFEFREE(data);
}

static void goodG2B()
{
    wchar_t * data;
    /* Initialize data */
    data = NULL;
    data = (wchar_t *)SAFEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* FIX: Do NOT free data in the source - the bad sink frees data */
    goodG2BSink(data);
}

/* goodB2G uses the BadSource with the GoodSink */
static void goodB2GSink(wchar_t * data)
{
    /* do nothing */
    /* FIX: Don't attempt to free the memory */
    ; /* empty statement needed for some flow variants */
}

static void goodB2G()
{
    wchar_t * data;
    /* Initialize data */
    data = NULL;
    data = (wchar_t *)SAFEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    goodB2GSink(data);
}

void CWE415_Double_Free__malloc_free_wchar_t_41_good()
{
    goodB2G();
    goodG2B();
}


/* Below is the main(). It is only used when building this testcase on
   its own for testing or for building a binary to use in testing binary
   analysis tools. It is not used when compiling all the testcases as one
   application, which is how source code analysis tools are tested. */


int main(int argc, char * argv[])
{
    /* seed randomness */
    srand( (unsigned)time(NULL) );
    CWE415_Double_Free__malloc_free_wchar_t_41_good();
    CWE415_Double_Free__malloc_free_wchar_t_41_bad();
    return 0;
}

