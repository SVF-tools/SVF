/*
 * double free
 * Author: Jiawei Ren
 * Date: 07/08/2022
 */


#include "doublefree_check.h"


/* TEMPLATE GENERATED TESTCASE FILE
Filename: CWE415_Double_Free__malloc_free_wchar_t_42.c
Label Definition File: CWE415_Double_Free__malloc_free.label.xml
Template File: sources-sinks-42.tmpl.c
*/
/*
 * @description
 * CWE: 415 Double Free
 * BadSource:  Allocate data using malloc() and Deallocate data using free()
 * GoodSource: Allocate data using malloc()
 * Sinks:
 *    GoodSink: do nothing
 *    BadSink : Deallocate data using free()
 * Flow Variant: 42 Data flow: data returned from one function to another in the same source file
 *
 * */

#include "std_testcase.h"

#include <wchar.h>


static wchar_t * badSource(wchar_t * data)
{
    data = (wchar_t *)DOUBLEFREEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    return data;
}

void CWE415_Double_Free__malloc_free_wchar_t_42_bad()
{
    wchar_t * data;
    /* Initialize data */
    data = NULL;
    data = badSource(data);
    /* POTENTIAL FLAW: UAF */
    USEAFTERFREE(data);
}


/* goodG2B uses the GoodSource with the BadSink */
static wchar_t * goodG2BSource(wchar_t * data)
{
    data = (wchar_t *)SAFEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* FIX: Do NOT free data in the source - the bad sink frees data */
    return data;
}

static void goodG2B()
{
    wchar_t * data;
    /* Initialize data */
    data = NULL;
    data = goodG2BSource(data);
    /* POTENTIAL FLAW: Possibly freeing memory twice */
    SAFEFREE(data);
}

/* goodB2G uses the BadSource with the GoodSink */
static wchar_t * goodB2GSource(wchar_t * data)
{
    data = (wchar_t *)SAFEMALLOC(100*sizeof(wchar_t));
    if (data == NULL) {exit(-1);}
    /* POTENTIAL FLAW: Free data in the source - the bad sink frees data as well */
    SAFEFREE(data);
    return data;
}

static void goodB2G()
{
    wchar_t * data;
    /* Initialize data */
    data = NULL;
    data = goodB2GSource(data);
    /* do nothing */
    /* FIX: Don't attempt to free the memory */
    ; /* empty statement needed for some flow variants */
}

void CWE415_Double_Free__malloc_free_wchar_t_42_good()
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
    CWE415_Double_Free__malloc_free_wchar_t_42_good();
    CWE415_Double_Free__malloc_free_wchar_t_42_bad();
    return 0;
}

