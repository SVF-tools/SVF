/*
 * double free
 * Author: Jiawei Ren
 * Date: 03/02/2022
 */


#include "doublefree_check.h"

/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_long_31.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-31.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 31 Data flow using a copy of data within the same function
 *
 * */

#include "std_testcase.h"

#include <wchar.h>


void CWE415_Double_Free__malloc_free_long_31_bad()
{
    long * data;
    /* Initialize data */
    data = NULL;
    data = (long *)DOUBLEFREEMALLOC(100*sizeof(long));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    {
        long * dataCopy = data;
        long * data = dataCopy;
        /* POTENTIAL FLAW: UAF */
        USEAFTERFREE(data);
    }
}


/* goodG2B() uses the GoodSource with the BadSink */
static void goodG2B()
{
    long * data;
    /* Initialize data */
    data = NULL;
    data = (long *)SAFEMALLOC(100*sizeof(long));
    if (data == NULL) {exit(-1);}
    /* FIX: Do NOT free data in the source - the bad sink frees data */
    {
        long * dataCopy = data;
        long * data = dataCopy;
        /* POTENTIAL FLAW: Possibly freeing memory twice */
        SAFEFREE(data);
    }
}

/* goodB2G() uses the BadSource with the GoodSink */
static void goodB2G()
{
    long * data;
    /* Initialize data */
    data = NULL;
    data = (long *)SAFEMALLOC(100*sizeof(long));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    {
        long * dataCopy = data;
        long * data = dataCopy;
        /* do nothing */
        /* FIX: Don't attempt to free the memory */
        ; /* empty statement needed for some flow variants */
    }
}

void CWE415_Double_Free__malloc_free_long_31_good()
{
    goodG2B();
    goodB2G();
}


/* Below is the main(). It is only used when building this testcase on
   its own for testing or for building a binary to use in testing binary
   analysis tools. It is not used when compiling all the testcases as one
   application, which is how source code analysis tools are tested. */

int main(int argc, char * argv[])
{
    /* seed randomness */
    CWE415_Double_Free__malloc_free_long_31_good();
    CWE415_Double_Free__malloc_free_long_31_bad();
    return 0;
}

