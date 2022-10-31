/*
 * double free
 * Author: Jiawei Ren
 * Date: 03/02/2022
 */


#include "doublefree_check.h"


/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_long_15.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-15.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 15 Control flow: switch(6) and switch(7)
 *
 * */

#include "std_testcase.h"

#include <wchar.h>


void CWE415_Double_Free__malloc_free_long_15_bad()
{
    long * data;
    /* Initialize data */
    data = NULL;
    switch(6)
    {
        case 6:
            data = (long *)DOUBLEFREEMALLOC(100*sizeof(long));
            if (data == NULL) {exit(-1);}
            /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
            SAFEFREE(data);
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
    switch(7)
    {
        case 7:
            /* POTENTIAL FLAW: UAF */
            USEAFTERFREE(data);
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
}


/* goodB2G1() - use badsource and goodsink by changing the second switch to switch(8) */
static void goodB2G1()
{
    long * data;
    /* Initialize data */
    data = NULL;
    switch(6)
    {
        case 6:
            data = (long *)SAFEMALLOC(100*sizeof(long));
            if (data == NULL) {exit(-1);}
            /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
            SAFEFREE(data);
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
    switch(8)
    {
        case 7:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
        default:
            /* do nothing */
            /* FIX: Don't attempt to free the memory */
            ; /* empty statement needed for some flow variants */
            break;
    }
}

/* goodB2G2() - use badsource and goodsink by reversing the blocks in the second switch */
static void goodB2G2()
{
    long * data;
    /* Initialize data */
    data = NULL;
    switch(6)
    {
        case 6:
            data = (long *)SAFEMALLOC(100*sizeof(long));
            if (data == NULL) {exit(-1);}
            /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
            SAFEFREE(data);
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
    switch(7)
    {
        case 7:
            /* do nothing */
            /* FIX: Don't attempt to free the memory */
            ; /* empty statement needed for some flow variants */
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
}

/* goodG2B1() - use goodsource and badsink by changing the first switch to switch(5) */
static void goodG2B1()
{
    long * data;
    /* Initialize data */
    data = NULL;
    switch(5)
    {
        case 6:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
        default:
            data = (long *)SAFEMALLOC(100*sizeof(long));
            if (data == NULL) {exit(-1);}
            /* FIX: Do NOT free data in the source - the bad sink frees data */
            break;
    }
    switch(7)
    {
        case 7:
            /* POTENTIAL FLAW: Possibly freeing memory twice */
            SAFEFREE(data);
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
}

/* goodG2B2() - use goodsource and badsink by reversing the blocks in the first switch */
static void goodG2B2()
{
    long * data;
    /* Initialize data */
    data = NULL;
    switch(6)
    {
        case 6:
            data = (long *)SAFEMALLOC(100*sizeof(long));
            if (data == NULL) {exit(-1);}
            /* FIX: Do NOT free data in the source - the bad sink frees data */
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
    switch(7)
    {
        case 7:
            /* POTENTIAL FLAW: Possibly freeing memory twice */
            SAFEFREE(data);
            break;
        default:
            /* INCIDENTAL: CWE 561 Dead Code, the code below will never run */
            printLine("Benign, fixed string");
            break;
    }
}

void CWE415_Double_Free__malloc_free_long_15_good()
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
    CWE415_Double_Free__malloc_free_long_15_good();
    CWE415_Double_Free__malloc_free_long_15_bad();
    return 0;
}
