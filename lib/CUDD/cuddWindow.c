/**CFile***********************************************************************

  FileName    [cuddWindow.c]

  PackageName [cudd]

  Synopsis    [Functions for variable reordering by window permutation.]

  Description [Internal procedures included in this module:
		<ul>
		<li> cuddWindowReorder()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> ddWindow2()
		<li> ddWindowConv2()
		<li> ddPermuteWindow3()
		<li> ddWindow3()
		<li> ddWindowConv3()
		<li> ddPermuteWindow4()
		<li> ddWindow4()
		<li> ddWindowConv4()
		</ul>]

  Author      [Fabio Somenzi]

  Copyright   [Copyright (c) 1995-2012, Regents of the University of Colorado

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  Neither the name of the University of Colorado nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.]

******************************************************************************/

#include "CUDD/util.h"
#include "CUDD/cuddInt.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddWindow.c,v 1.15 2012/02/05 01:07:19 fabio Exp $";
#endif

#ifdef DD_STATS
extern  int     ddTotalNumberSwapping;
extern  int     ddTotalNISwaps;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddWindow2 (DdManager *table, int low, int high);
static int ddWindowConv2 (DdManager *table, int low, int high);
static int ddPermuteWindow3 (DdManager *table, int x);
static int ddWindow3 (DdManager *table, int low, int high);
static int ddWindowConv3 (DdManager *table, int low, int high);
static int ddPermuteWindow4 (DdManager *table, int w);
static int ddWindow4 (DdManager *table, int low, int high);
static int ddWindowConv4 (DdManager *table, int low, int high);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Reorders by applying the method of the sliding window.]

  Description [Reorders by applying the method of the sliding window.
  Tries all possible permutations to the variables in a window that
  slides from low to high. The size of the window is determined by
  submethod.  Assumes that no dead nodes are present.  Returns 1 in
  case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int
cuddWindowReorder(
  DdManager * table /* DD table */,
  int low /* lowest index to reorder */,
  int high /* highest index to reorder */,
  Cudd_ReorderingType submethod /* window reordering option */)
{

    int res;
#ifdef DD_DEBUG
    int supposedOpt;
#endif

    switch (submethod) {
    case CUDD_REORDER_WINDOW2:
	res = ddWindow2(table,low,high);
	break;
    case CUDD_REORDER_WINDOW3:
	res = ddWindow3(table,low,high);
	break;
    case CUDD_REORDER_WINDOW4:
	res = ddWindow4(table,low,high);
	break;
    case CUDD_REORDER_WINDOW2_CONV:
	res = ddWindowConv2(table,low,high);
	break;
    case CUDD_REORDER_WINDOW3_CONV:
	res = ddWindowConv3(table,low,high);
#ifdef DD_DEBUG
	supposedOpt = table->keys - table->isolated;
	res = ddWindow3(table,low,high);
	if (table->keys - table->isolated != (unsigned) supposedOpt) {
	    (void) fprintf(table->err, "Convergence failed! (%d != %d)\n",
			   table->keys - table->isolated, supposedOpt);
	}
#endif
	break;
    case CUDD_REORDER_WINDOW4_CONV:
	res = ddWindowConv4(table,low,high);
#ifdef DD_DEBUG
	supposedOpt = table->keys - table->isolated;
	res = ddWindow4(table,low,high);
	if (table->keys - table->isolated != (unsigned) supposedOpt) {
	    (void) fprintf(table->err,"Convergence failed! (%d != %d)\n",
			   table->keys - table->isolated, supposedOpt);
	}
#endif
	break;
    default: return(0);
    }

    return(res);

} /* end of cuddWindowReorder */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Reorders by applying a sliding window of width 2.]

  Description [Reorders by applying a sliding window of width 2.
  Tries both permutations of the variables in a window
  that slides from low to high.  Assumes that no dead nodes are
  present.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddWindow2(
  DdManager * table,
  int  low,
  int  high)
{

    int x;
    int res;
    int size;

#ifdef DD_DEBUG
    assert(low >= 0 && high < table->size);
#endif

    if (high-low < 1) return(0);

    res = table->keys - table->isolated;
    for (x = low; x < high; x++) {
	size = res;
	res = cuddSwapInPlace(table,x,x+1);
	if (res == 0) return(0);
	if (res >= size) { /* no improvement: undo permutation */
	    res = cuddSwapInPlace(table,x,x+1);
	    if (res == 0) return(0);
	}
#ifdef DD_STATS
	if (res < size) {
	    (void) fprintf(table->out,"-");
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
    }

    return(1);

} /* end of ddWindow2 */


/**Function********************************************************************

  Synopsis    [Reorders by repeatedly applying a sliding window of width 2.]

  Description [Reorders by repeatedly applying a sliding window of width
  2. Tries both permutations of the variables in a window
  that slides from low to high.  Assumes that no dead nodes are
  present.  Uses an event-driven approach to determine convergence.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddWindowConv2(
  DdManager * table,
  int  low,
  int  high)
{
    int x;
    int res;
    int nwin;
    int newevent;
    int *events;
    int size;

#ifdef DD_DEBUG
    assert(low >= 0 && high < table->size);
#endif

    if (high-low < 1) return(ddWindowConv2(table,low,high));

    nwin = high-low;
    events = ALLOC(int,nwin);
    if (events == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (x=0; x<nwin; x++) {
	events[x] = 1;
    }

    res = table->keys - table->isolated;
    do {
	newevent = 0;
	for (x=0; x<nwin; x++) {
	    if (events[x]) {
		size = res;
		res = cuddSwapInPlace(table,x+low,x+low+1);
		if (res == 0) {
		    FREE(events);
		    return(0);
		}
		if (res >= size) { /* no improvement: undo permutation */
		    res = cuddSwapInPlace(table,x+low,x+low+1);
		    if (res == 0) {
			FREE(events);
			return(0);
		    }
		}
		if (res < size) {
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 0)		events[x-1] = 1;
		    newevent = 1;
		}
		events[x] = 0;
#ifdef DD_STATS
		if (res < size) {
		    (void) fprintf(table->out,"-");
		} else {
		    (void) fprintf(table->out,"=");
		}
		fflush(table->out);
#endif
	    }
	}
#ifdef DD_STATS
	if (newevent) {
	    (void) fprintf(table->out,"|");
	    fflush(table->out);
	}
#endif
    } while (newevent);

    FREE(events);

    return(1);

} /* end of ddWindowConv3 */


/**Function********************************************************************

  Synopsis [Tries all the permutations of the three variables between
  x and x+2 and retains the best.]

  Description [Tries all the permutations of the three variables between
  x and x+2 and retains the best. Assumes that no dead nodes are
  present.  Returns the index of the best permutation (1-6) in case of
  success; 0 otherwise.Assumes that no dead nodes are present.  Returns
  the index of the best permutation (1-6) in case of success; 0
  otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddPermuteWindow3(
  DdManager * table,
  int  x)
{
    int y,z;
    int	size,sizeNew;
    int	best;

#ifdef DD_DEBUG
    assert(table->dead == 0);
    assert(x+2 < table->size);
#endif

    size = table->keys - table->isolated;
    y = x+1; z = y+1;

    /* The permutation pattern is:
    ** (x,y)(y,z)
    ** repeated three times to get all 3! = 6 permutations.
    */
#define ABC 1
    best = ABC;

#define	BAC 2
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = BAC;
	size = sizeNew;
    }
#define BCA 3
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = BCA;
	size = sizeNew;
    }
#define CBA 4
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = CBA;
	size = sizeNew;
    }
#define CAB 5
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = CAB;
	size = sizeNew;
    }
#define ACB 6
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = ACB;
	size = sizeNew;
    }

    /* Now take the shortest route to the best permuytation.
    ** The initial permutation is ACB.
    */
    switch(best) {
    case BCA: if (!cuddSwapInPlace(table,y,z)) return(0);
    case CBA: if (!cuddSwapInPlace(table,x,y)) return(0);
    case ABC: if (!cuddSwapInPlace(table,y,z)) return(0);
    case ACB: break;
    case BAC: if (!cuddSwapInPlace(table,y,z)) return(0);
    case CAB: if (!cuddSwapInPlace(table,x,y)) return(0);
	       break;
    default: return(0);
    }

#ifdef DD_DEBUG
    assert(table->keys - table->isolated == (unsigned) size);
#endif

    return(best);

} /* end of ddPermuteWindow3 */


/**Function********************************************************************

  Synopsis    [Reorders by applying a sliding window of width 3.]

  Description [Reorders by applying a sliding window of width 3.
  Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddWindow3(
  DdManager * table,
  int  low,
  int  high)
{

    int x;
    int res;

#ifdef DD_DEBUG
    assert(low >= 0 && high < table->size);
#endif

    if (high-low < 2) return(ddWindow2(table,low,high));

    for (x = low; x+1 < high; x++) {
	res = ddPermuteWindow3(table,x);
	if (res == 0) return(0);
#ifdef DD_STATS
	if (res == ABC) {
	    (void) fprintf(table->out,"=");
	} else {
	    (void) fprintf(table->out,"-");
	}
	fflush(table->out);
#endif
    }

    return(1);

} /* end of ddWindow3 */


/**Function********************************************************************

  Synopsis    [Reorders by repeatedly applying a sliding window of width 3.]

  Description [Reorders by repeatedly applying a sliding window of width
  3. Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Uses an event-driven approach to determine convergence.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddWindowConv3(
  DdManager * table,
  int  low,
  int  high)
{
    int x;
    int res;
    int nwin;
    int newevent;
    int *events;

#ifdef DD_DEBUG
    assert(low >= 0 && high < table->size);
#endif

    if (high-low < 2) return(ddWindowConv2(table,low,high));

    nwin = high-low-1;
    events = ALLOC(int,nwin);
    if (events == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (x=0; x<nwin; x++) {
	events[x] = 1;
    }

    do {
	newevent = 0;
	for (x=0; x<nwin; x++) {
	    if (events[x]) {
		res = ddPermuteWindow3(table,x+low);
		switch (res) {
		case ABC:
		    break;
		case BAC:
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 1)		events[x-2] = 1;
		    newevent = 1;
		    break;
		case BCA:
		case CBA:
		case CAB:
		    if (x < nwin-2)	events[x+2] = 1;
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 0)		events[x-1] = 1;
		    if (x > 1)		events[x-2] = 1;
		    newevent = 1;
		    break;
		case ACB:
		    if (x < nwin-2)	events[x+2] = 1;
		    if (x > 0)		events[x-1] = 1;
		    newevent = 1;
		    break;
		default:
		    FREE(events);
		    return(0);
		}
		events[x] = 0;
#ifdef DD_STATS
		if (res == ABC) {
		    (void) fprintf(table->out,"=");
		} else {
		    (void) fprintf(table->out,"-");
		}
		fflush(table->out);
#endif
	    }
	}
#ifdef DD_STATS
	if (newevent) {
	    (void) fprintf(table->out,"|");
	    fflush(table->out);
	}
#endif
    } while (newevent);

    FREE(events);

    return(1);

} /* end of ddWindowConv3 */


/**Function********************************************************************

  Synopsis [Tries all the permutations of the four variables between w
  and w+3 and retains the best.]

  Description [Tries all the permutations of the four variables between
  w and w+3 and retains the best. Assumes that no dead nodes are
  present.  Returns the index of the best permutation (1-24) in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddPermuteWindow4(
  DdManager * table,
  int  w)
{
    int x,y,z;
    int	size,sizeNew;
    int	best;

#ifdef DD_DEBUG
    assert(table->dead == 0);
    assert(w+3 < table->size);
#endif

    size = table->keys - table->isolated;
    x = w+1; y = x+1; z = y+1;

    /* The permutation pattern is:
     * (w,x)(y,z)(w,x)(x,y)
     * (y,z)(w,x)(y,z)(x,y)
     * repeated three times to get all 4! = 24 permutations.
     * This gives a hamiltonian circuit of Cayley's graph.
     * The codes to the permutation are assigned in topological order.
     * The permutations at lower distance from the final permutation are
     * assigned lower codes. This way we can choose, between
     * permutations that give the same size, one that requires the minimum
     * number of swaps from the final permutation of the hamiltonian circuit.
     * There is an exception to this rule: ABCD is given Code 1, to
     * avoid oscillation when convergence is sought.
     */
#define ABCD 1
    best = ABCD;

#define	BACD 7
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = BACD;
	size = sizeNew;
    }
#define BADC 13
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = BADC;
	size = sizeNew;
    }
#define ABDC 8
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size || (sizeNew == size && ABDC < best)) {
	if (sizeNew == 0) return(0);
	best = ABDC;
	size = sizeNew;
    }
#define ADBC 14
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = ADBC;
	size = sizeNew;
    }
#define ADCB 9
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && ADCB < best)) {
	if (sizeNew == 0) return(0);
	best = ADCB;
	size = sizeNew;
    }
#define DACB 15
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = DACB;
	size = sizeNew;
    }
#define DABC 20
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = DABC;
	size = sizeNew;
    }
#define DBAC 23
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = DBAC;
	size = sizeNew;
    }
#define BDAC 19
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size || (sizeNew == size && BDAC < best)) {
	if (sizeNew == 0) return(0);
	best = BDAC;
	size = sizeNew;
    }
#define BDCA 21
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && BDCA < best)) {
	if (sizeNew == 0) return(0);
	best = BDCA;
	size = sizeNew;
    }
#define DBCA 24
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size) {
	if (sizeNew == 0) return(0);
	best = DBCA;
	size = sizeNew;
    }
#define DCBA 22
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size || (sizeNew == size && DCBA < best)) {
	if (sizeNew == 0) return(0);
	best = DCBA;
	size = sizeNew;
    }
#define DCAB 18
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && DCAB < best)) {
	if (sizeNew == 0) return(0);
	best = DCAB;
	size = sizeNew;
    }
#define CDAB 12
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size || (sizeNew == size && CDAB < best)) {
	if (sizeNew == 0) return(0);
	best = CDAB;
	size = sizeNew;
    }
#define CDBA 17
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && CDBA < best)) {
	if (sizeNew == 0) return(0);
	best = CDBA;
	size = sizeNew;
    }
#define CBDA 11
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size || (sizeNew == size && CBDA < best)) {
	if (sizeNew == 0) return(0);
	best = CBDA;
	size = sizeNew;
    }
#define BCDA 16
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size || (sizeNew == size && BCDA < best)) {
	if (sizeNew == 0) return(0);
	best = BCDA;
	size = sizeNew;
    }
#define BCAD 10
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && BCAD < best)) {
	if (sizeNew == 0) return(0);
	best = BCAD;
	size = sizeNew;
    }
#define CBAD 5
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size || (sizeNew == size && CBAD < best)) {
	if (sizeNew == 0) return(0);
	best = CBAD;
	size = sizeNew;
    }
#define CABD 3
    sizeNew = cuddSwapInPlace(table,x,y);
    if (sizeNew < size || (sizeNew == size && CABD < best)) {
	if (sizeNew == 0) return(0);
	best = CABD;
	size = sizeNew;
    }
#define CADB 6
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && CADB < best)) {
	if (sizeNew == 0) return(0);
	best = CADB;
	size = sizeNew;
    }
#define ACDB 4
    sizeNew = cuddSwapInPlace(table,w,x);
    if (sizeNew < size || (sizeNew == size && ACDB < best)) {
	if (sizeNew == 0) return(0);
	best = ACDB;
	size = sizeNew;
    }
#define ACBD 2
    sizeNew = cuddSwapInPlace(table,y,z);
    if (sizeNew < size || (sizeNew == size && ACBD < best)) {
	if (sizeNew == 0) return(0);
	best = ACBD;
	size = sizeNew;
    }

    /* Now take the shortest route to the best permutation.
    ** The initial permutation is ACBD.
    */
    switch(best) {
    case DBCA: if (!cuddSwapInPlace(table,y,z)) return(0);
    case BDCA: if (!cuddSwapInPlace(table,x,y)) return(0);
    case CDBA: if (!cuddSwapInPlace(table,w,x)) return(0);
    case ADBC: if (!cuddSwapInPlace(table,y,z)) return(0);
    case ABDC: if (!cuddSwapInPlace(table,x,y)) return(0);
    case ACDB: if (!cuddSwapInPlace(table,y,z)) return(0);
    case ACBD: break;
    case DCBA: if (!cuddSwapInPlace(table,y,z)) return(0);
    case BCDA: if (!cuddSwapInPlace(table,x,y)) return(0);
    case CBDA: if (!cuddSwapInPlace(table,w,x)) return(0);
	       if (!cuddSwapInPlace(table,x,y)) return(0);
	       if (!cuddSwapInPlace(table,y,z)) return(0);
	       break;
    case DBAC: if (!cuddSwapInPlace(table,x,y)) return(0);
    case DCAB: if (!cuddSwapInPlace(table,w,x)) return(0);
    case DACB: if (!cuddSwapInPlace(table,y,z)) return(0);
    case BACD: if (!cuddSwapInPlace(table,x,y)) return(0);
    case CABD: if (!cuddSwapInPlace(table,w,x)) return(0);
	       break;
    case DABC: if (!cuddSwapInPlace(table,y,z)) return(0);
    case BADC: if (!cuddSwapInPlace(table,x,y)) return(0);
    case CADB: if (!cuddSwapInPlace(table,w,x)) return(0);
	       if (!cuddSwapInPlace(table,y,z)) return(0);
	       break;
    case BDAC: if (!cuddSwapInPlace(table,x,y)) return(0);
    case CDAB: if (!cuddSwapInPlace(table,w,x)) return(0);
    case ADCB: if (!cuddSwapInPlace(table,y,z)) return(0);
    case ABCD: if (!cuddSwapInPlace(table,x,y)) return(0);
	       break;
    case BCAD: if (!cuddSwapInPlace(table,x,y)) return(0);
    case CBAD: if (!cuddSwapInPlace(table,w,x)) return(0);
	       if (!cuddSwapInPlace(table,x,y)) return(0);
	       break;
    default: return(0);
    }

#ifdef DD_DEBUG
    assert(table->keys - table->isolated == (unsigned) size);
#endif

    return(best);

} /* end of ddPermuteWindow4 */


/**Function********************************************************************

  Synopsis    [Reorders by applying a sliding window of width 4.]

  Description [Reorders by applying a sliding window of width 4.
  Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddWindow4(
  DdManager * table,
  int  low,
  int  high)
{

    int w;
    int res;

#ifdef DD_DEBUG
    assert(low >= 0 && high < table->size);
#endif

    if (high-low < 3) return(ddWindow3(table,low,high));

    for (w = low; w+2 < high; w++) {
	res = ddPermuteWindow4(table,w);
	if (res == 0) return(0);
#ifdef DD_STATS
	if (res == ABCD) {
	    (void) fprintf(table->out,"=");
	} else {
	    (void) fprintf(table->out,"-");
	}
	fflush(table->out);
#endif
    }

    return(1);

} /* end of ddWindow4 */


/**Function********************************************************************

  Synopsis    [Reorders by repeatedly applying a sliding window of width 4.]

  Description [Reorders by repeatedly applying a sliding window of width
  4. Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Uses an event-driven approach to determine convergence.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddWindowConv4(
  DdManager * table,
  int  low,
  int  high)
{
    int x;
    int res;
    int nwin;
    int newevent;
    int *events;

#ifdef DD_DEBUG
    assert(low >= 0 && high < table->size);
#endif

    if (high-low < 3) return(ddWindowConv3(table,low,high));

    nwin = high-low-2;
    events = ALLOC(int,nwin);
    if (events == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (x=0; x<nwin; x++) {
	events[x] = 1;
    }

    do {
	newevent = 0;
	for (x=0; x<nwin; x++) {
	    if (events[x]) {
		res = ddPermuteWindow4(table,x+low);
		switch (res) {
		case ABCD:
		    break;
		case BACD:
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 2)		events[x-3] = 1;
		    newevent = 1;
		    break;
		case BADC:
		    if (x < nwin-3)	events[x+3] = 1;
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 0)		events[x-1] = 1;
		    if (x > 2)		events[x-3] = 1;
		    newevent = 1;
		    break;
		case ABDC:
		    if (x < nwin-3)	events[x+3] = 1;
		    if (x > 0)		events[x-1] = 1;
		    newevent = 1;
		    break;
		case ADBC:
		case ADCB:
		case ACDB:
		    if (x < nwin-3)	events[x+3] = 1;
		    if (x < nwin-2)	events[x+2] = 1;
		    if (x > 0)		events[x-1] = 1;
		    if (x > 1)		events[x-2] = 1;
		    newevent = 1;
		    break;
		case DACB:
		case DABC:
		case DBAC:
		case BDAC:
		case BDCA:
		case DBCA:
		case DCBA:
		case DCAB:
		case CDAB:
		case CDBA:
		case CBDA:
		case BCDA:
		case CADB:
		    if (x < nwin-3)	events[x+3] = 1;
		    if (x < nwin-2)	events[x+2] = 1;
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 0)		events[x-1] = 1;
		    if (x > 1)		events[x-2] = 1;
		    if (x > 2)		events[x-3] = 1;
		    newevent = 1;
		    break;
		case BCAD:
		case CBAD:
		case CABD:
		    if (x < nwin-2)	events[x+2] = 1;
		    if (x < nwin-1)	events[x+1] = 1;
		    if (x > 1)		events[x-2] = 1;
		    if (x > 2)		events[x-3] = 1;
		    newevent = 1;
		    break;
		case ACBD:
		    if (x < nwin-2)	events[x+2] = 1;
		    if (x > 1)		events[x-2] = 1;
		    newevent = 1;
		    break;
		default:
		    FREE(events);
		    return(0);
		}
		events[x] = 0;
#ifdef DD_STATS
		if (res == ABCD) {
		    (void) fprintf(table->out,"=");
		} else {
		    (void) fprintf(table->out,"-");
		}
		fflush(table->out);
#endif
	    }
	}
#ifdef DD_STATS
	if (newevent) {
	    (void) fprintf(table->out,"|");
	    fflush(table->out);
	}
#endif
    } while (newevent);

    FREE(events);

    return(1);

} /* end of ddWindowConv4 */
