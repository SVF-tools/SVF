/*
 * double free
 * Author: Jiawei Ren
 * Date: 03/02/2022
 */


#include "doublefree_check.h"

/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_long_11.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-11.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 11 Control flow: if(globalReturnsTrue()) and if(globalReturnsFalse())
 *
 * */

#include "std_testcase.h"

#include <wchar.h>


void CWE415_Double_Free__malloc_free_long_11_bad()
{
    long * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrue())
    {
        data = (long *)DOUBLEFREEMALLOC(100*sizeof(long));
        if (data == NULL) {exit(-1);}
        /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
        SAFEFREE(data);
    }
    if(globalReturnsTrue())
    {
        /* POTENTIAL FLAW: UAF */
        USEAFTERFREE(data);
    }
}


/* goodB2G1() - use badsource and goodsink by changing the second globalReturnsTrue() to globalReturnsFalse() */
static void goodB2G1()
{
    long * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrue())
    {
        data = (long *)SAFEMALLOC(100*sizeof(long));
        if (data == NULL) {exit(-1);}
        /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
        SAFEFREE(data);
    }
    if(globalReturnsFalse())
    {
        /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
        printLine("Benign, fixed string");
    }
    else
    {
        /* do nothing */
        /* FIX: Don't attempt to free the memory */
        ; /* empty statement needed for some flow variants */
    }
}

/* goodB2G2() - use badsource and goodsink by reversing the blocks in the second if */
static void goodB2G2()
{
    long * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrue())
    {
        data = (long *)SAFEMALLOC(100*sizeof(long));
        if (data == NULL) {exit(-1);}
        /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
        SAFEFREE(data);
    }
    if(globalReturnsTrue())
    {
        /* do nothing */
        /* FIX: Don't attempt to free the memory */
        ; /* empty statement needed for some flow variants */
    }
}

/* goodG2B1() - use goodsource and badsink by changing the first globalReturnsTrue() to globalReturnsFalse() */
static void goodG2B1()
{
    long * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsFalse())
    {
        /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
        printLine("Benign, fixed string");
    }
    else
    {
        data = (long *)SAFEMALLOC(100*sizeof(long));
        if (data == NULL) {exit(-1);}
        /* FIX: Do NOT free data in the source - the bad sink frees data */
    }
    if(globalReturnsTrue())
    {
        /* POTENTIAL FLAW: Possibly freeing memory twice */
        SAFEFREE(data);
    }
}

/* goodG2B2() - use goodsource and badsink by reversing the blocks in the first if */
static void goodG2B2()
{
    long * data;
    /* Initialize data */
    data = NULL;
    if(globalReturnsTrue())
    {
        data = (long *)SAFEMALLOC(100*sizeof(long));
        if (data == NULL) {exit(-1);}
        /* FIX: Do NOT free data in the source - the bad sink frees data */
    }
    if(globalReturnsTrue())
    {
        /* POTENTIAL FLAW: Possibly freeing memory twice */
        SAFEFREE(data);
    }
}

void CWE415_Double_Free__malloc_free_long_11_good()
{
    goodB2G1();
    goodB2G2();
    goodG2B1();
    goodG2B2();
}


/* Below is the main(). It is only used when building this testcase on
   its own for testing or for building a binary to use in testing binary
   analysis tools. It is not used when compiling all the testcases as one
   application, which is how source code analysis tools are tested. */

int main(int argc, char * argv[])
{
    /* seed randomness */
    srand( (unsigned)time(NULL) );
    CWE415_Double_Free__malloc_free_long_11_good();
    CWE415_Double_Free__malloc_free_long_11_bad();
    return 0;
}


