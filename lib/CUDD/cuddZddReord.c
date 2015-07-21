/**CFile***********************************************************************

  FileName    [cuddZddReord.c]

  PackageName [cudd]

  Synopsis    [Procedures for dynamic variable ordering of ZDDs.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_zddReduceHeap()
		    <li> Cudd_zddShuffleHeap()
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    <li> cuddZddAlignToBdd()
		    <li> cuddZddNextHigh()
		    <li> cuddZddNextLow()
		    <li> cuddZddUniqueCompare()
		    <li> cuddZddSwapInPlace()
		    <li> cuddZddSwapping()
		    <li> cuddZddSifting()
		    </ul>
	       Static procedures included in this module:
		    <ul>
		    <li> zddSwapAny()
		    <li> cuddZddSiftingAux()
		    <li> cuddZddSiftingUp()
		    <li> cuddZddSiftingDown()
		    <li> cuddZddSiftingBackward()
		    <li> zddReorderPreprocess()
		    <li> zddReorderPostprocess()
		    <li> zddShuffle()
		    <li> zddSiftUp()
		    </ul>
	      ]

  SeeAlso     []

  Author      [Hyong-Kyoon Shin, In-Ho Moon]

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

#define DD_MAX_SUBTABLE_SPARSITY 8
#define DD_SHRINK_FACTOR 2

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddReord.c,v 1.49 2012/02/05 01:07:19 fabio Exp $";
#endif

int	*zdd_entry;

int	zddTotalNumberSwapping;

static  DdNode	*empty;


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static Move * zddSwapAny (DdManager *table, int x, int y);
static int cuddZddSiftingAux (DdManager *table, int x, int x_low, int x_high);
static Move * cuddZddSiftingUp (DdManager *table, int x, int x_low, int initial_size);
static Move * cuddZddSiftingDown (DdManager *table, int x, int x_high, int initial_size);
static int cuddZddSiftingBackward (DdManager *table, Move *moves, int size);
static void zddReorderPreprocess (DdManager *table);
static int zddReorderPostprocess (DdManager *table);
static int zddShuffle (DdManager *table, int *permutation);
static int zddSiftUp (DdManager *table, int x, int xLow);
static void zddFixTree (DdManager *table, MtrNode *treenode);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Main dynamic reordering routine for ZDDs.]

  Description [Main dynamic reordering routine for ZDDs.
  Calls one of the possible reordering procedures:
  <ul>
  <li>Swapping
  <li>Sifting
  <li>Symmetric Sifting
  </ul>

  For sifting and symmetric sifting it is possible to request reordering
  to convergence.<p>

  The core of all methods is the reordering procedure
  cuddZddSwapInPlace() which swaps two adjacent variables.
  Returns 1 in case of success; 0 otherwise. In the case of symmetric
  sifting (with and without convergence) returns 1 plus the number of
  symmetric variables, in case of success.]

  SideEffects [Changes the variable order for all ZDDs and clears
  the cache.]

******************************************************************************/
int
Cudd_zddReduceHeap(
  DdManager * table /* DD manager */,
  Cudd_ReorderingType heuristic /* method used for reordering */,
  int minsize /* bound below which no reordering occurs */)
{
    DdHook	 *hook;
    int		 result;
    unsigned int nextDyn;
#ifdef DD_STATS
    unsigned int initialSize;
    unsigned int finalSize;
#endif
    unsigned long localTime;

    /* Don't reorder if there are too many dead nodes. */
    if (table->keysZ - table->deadZ < (unsigned) minsize)
	return(1);

    if (heuristic == CUDD_REORDER_SAME) {
	heuristic = table->autoMethodZ;
    }
    if (heuristic == CUDD_REORDER_NONE) {
	return(1);
    }

    /* This call to Cudd_zddReduceHeap does initiate reordering. Therefore
    ** we count it.
    */
    table->reorderings++;
    empty = table->zero;

    localTime = util_cpu_time();

    /* Run the hook functions. */
    hook = table->preReorderingHook;
    while (hook != NULL) {
	int res = (hook->f)(table, "ZDD", (void *)heuristic);
	if (res == 0) return(0);
	hook = hook->next;
    }

    /* Clear the cache and collect garbage. */
    zddReorderPreprocess(table);
    zddTotalNumberSwapping = 0;

#ifdef DD_STATS
    initialSize = table->keysZ;

    switch(heuristic) {
    case CUDD_REORDER_RANDOM:
    case CUDD_REORDER_RANDOM_PIVOT:
	(void) fprintf(table->out,"#:I_RANDOM  ");
	break;
    case CUDD_REORDER_SIFT:
    case CUDD_REORDER_SIFT_CONVERGE:
    case CUDD_REORDER_SYMM_SIFT:
    case CUDD_REORDER_SYMM_SIFT_CONV:
	(void) fprintf(table->out,"#:I_SIFTING ");
	break;
    case CUDD_REORDER_LINEAR:
    case CUDD_REORDER_LINEAR_CONVERGE:
	(void) fprintf(table->out,"#:I_LINSIFT ");
	break;
    default:
	(void) fprintf(table->err,"Unsupported ZDD reordering method\n");
	return(0);
    }
    (void) fprintf(table->out,"%8d: initial size",initialSize); 
#endif

    result = cuddZddTreeSifting(table,heuristic);

#ifdef DD_STATS
    (void) fprintf(table->out,"\n");
    finalSize = table->keysZ;
    (void) fprintf(table->out,"#:F_REORDER %8d: final size\n",finalSize); 
    (void) fprintf(table->out,"#:T_REORDER %8g: total time (sec)\n",
		   ((double)(util_cpu_time() - localTime)/1000.0)); 
    (void) fprintf(table->out,"#:N_REORDER %8d: total swaps\n",
		   zddTotalNumberSwapping);
#endif

    if (result == 0)
	return(0);

    if (!zddReorderPostprocess(table))
	return(0);

    if (table->realignZ) {
	if (!cuddBddAlignToZdd(table))
	    return(0);
    }

    nextDyn = table->keysZ * DD_DYN_RATIO;
    if (table->reorderings < 20 || nextDyn > table->nextDyn)
	table->nextDyn = nextDyn;
    else
	table->nextDyn += 20;

    table->reordered = 1;

    /* Run hook functions. */
    hook = table->postReorderingHook;
    while (hook != NULL) {
	int res = (hook->f)(table, "ZDD", (void *)localTime);
	if (res == 0) return(0);
	hook = hook->next;
    }
    /* Update cumulative reordering time. */
    table->reordTime += util_cpu_time() - localTime;

    return(result);

} /* end of Cudd_zddReduceHeap */


/**Function********************************************************************

  Synopsis    [Reorders ZDD variables according to given permutation.]

  Description [Reorders ZDD variables according to given permutation.
  The i-th entry of the permutation array contains the index of the variable
  that should be brought to the i-th level.  The size of the array should be
  equal or greater to the number of variables currently in use.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [Changes the ZDD variable order for all diagrams and clears
  the cache.]

  SeeAlso [Cudd_zddReduceHeap]

******************************************************************************/
int
Cudd_zddShuffleHeap(
  DdManager * table /* DD manager */,
  int * permutation /* required variable permutation */)
{

    int	result;

    empty = table->zero;
    zddReorderPreprocess(table);

    result = zddShuffle(table,permutation);

    if (!zddReorderPostprocess(table)) return(0);

    return(result);

} /* end of Cudd_zddShuffleHeap */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Reorders ZDD variables according to the order of the BDD
  variables.]

  Description [Reorders ZDD variables according to the order of the
  BDD variables. This function can be called at the end of BDD
  reordering to insure that the order of the ZDD variables is
  consistent with the order of the BDD variables. The number of ZDD
  variables must be a multiple of the number of BDD variables. Let
  <code>M</code> be the ratio of the two numbers. cuddZddAlignToBdd
  then considers the ZDD variables from <code>M*i</code> to
  <code>(M+1)*i-1</code> as corresponding to BDD variable
  <code>i</code>.  This function should be normally called from
  Cudd_ReduceHeap, which clears the cache.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [Changes the ZDD variable order for all diagrams and performs
  garbage collection of the ZDD unique table.]

  SeeAlso [Cudd_zddShuffleHeap Cudd_ReduceHeap]

******************************************************************************/
int
cuddZddAlignToBdd(
  DdManager * table /* DD manager */)
{
    int *invpermZ;		/* permutation array */
    int M;			/* ratio of ZDD variables to BDD variables */
    int i,j;			/* loop indices */
    int result;			/* return value */

    /* We assume that a ratio of 0 is OK. */
    if (table->sizeZ == 0)
	return(1);

    empty = table->zero;
    M = table->sizeZ / table->size;
    /* Check whether the number of ZDD variables is a multiple of the
    ** number of BDD variables.
    */
    if (M * table->size != table->sizeZ)
	return(0);
    /* Create and initialize the inverse permutation array. */
    invpermZ = ALLOC(int,table->sizeZ);
    if (invpermZ == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (i = 0; i < table->size; i++) {
	int index = table->invperm[i];
	int indexZ = index * M;
	int levelZ = table->permZ[indexZ];
	levelZ = (levelZ / M) * M;
	for (j = 0; j < M; j++) {
	    invpermZ[M * i + j] = table->invpermZ[levelZ + j];
	}
    }
    /* Eliminate dead nodes. Do not scan the cache again, because we
    ** assume that Cudd_ReduceHeap has already cleared it.
    */
    cuddGarbageCollect(table,0);

    result = zddShuffle(table, invpermZ);
    FREE(invpermZ);
    /* Fix the ZDD variable group tree. */
    zddFixTree(table,table->treeZ);
    return(result);
    
} /* end of cuddZddAlignToBdd */


/**Function********************************************************************

  Synopsis    [Finds the next subtable with a larger index.]

  Description [Finds the next subtable with a larger index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddNextHigh(
  DdManager * table,
  int  x)
{
    return(x + 1);

} /* end of cuddZddNextHigh */


/**Function********************************************************************

  Synopsis    [Finds the next subtable with a smaller index.]

  Description [Finds the next subtable with a smaller index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddNextLow(
  DdManager * table,
  int  x)
{
    return(x - 1);

} /* end of cuddZddNextLow */


/**Function********************************************************************

  Synopsis [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the
  variables according to the number of keys in the subtables.
  Returns the difference in number of keys between the two
  variables being compared.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddUniqueCompare(
  int * ptr_x,
  int * ptr_y)
{
    return(zdd_entry[*ptr_y] - zdd_entry[*ptr_x]);

} /* end of cuddZddUniqueCompare */


/**Function********************************************************************

  Synopsis    [Swaps two adjacent variables.]

  Description [Swaps two adjacent variables. It assumes that no dead
  nodes are present on entry to this procedure.  The procedure then
  guarantees that no dead nodes will be present when it terminates.
  cuddZddSwapInPlace assumes that x &lt; y.  Returns the number of keys in
  the table if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddSwapInPlace(
  DdManager * table,
  int  x,
  int  y)
{
    DdNodePtr	*xlist, *ylist;
    int		xindex, yindex;
    int		xslots, yslots;
    int		xshift, yshift;
    int         oldxkeys, oldykeys;
    int         newxkeys, newykeys;
    int		i;
    int		posn;
    DdNode	*f, *f1, *f0, *f11, *f10, *f01, *f00;
    DdNode	*newf1, *newf0, *next;
    DdNodePtr	g, *lastP, *previousP;

#ifdef DD_DEBUG
    assert(x < y);
    assert(cuddZddNextHigh(table,x) == y);
    assert(table->subtableZ[x].keys != 0);
    assert(table->subtableZ[y].keys != 0);
    assert(table->subtableZ[x].dead == 0);
    assert(table->subtableZ[y].dead == 0);
#endif

    zddTotalNumberSwapping++;

    /* Get parameters of x subtable. */
    xindex   = table->invpermZ[x];
    xlist    = table->subtableZ[x].nodelist;
    oldxkeys = table->subtableZ[x].keys;
    xslots   = table->subtableZ[x].slots;
    xshift   = table->subtableZ[x].shift;
    newxkeys = 0;

    yindex   = table->invpermZ[y];
    ylist    = table->subtableZ[y].nodelist;
    oldykeys = table->subtableZ[y].keys;
    yslots   = table->subtableZ[y].slots;
    yshift   = table->subtableZ[y].shift;
    newykeys = oldykeys;

    /* The nodes in the x layer that don't depend on y directly
    ** will stay there; the others are put in a chain.
    ** The chain is handled as a FIFO; g points to the beginning and
    ** last points to the end.
    */

    g = NULL;
    lastP = &g;
    for (i = 0; i < xslots; i++) {
	previousP = &(xlist[i]);
	f = *previousP;
	while (f != NULL) {
	    next = f->next;
	    f1 = cuddT(f); f0 = cuddE(f);
	    if ((f1->index != (DdHalfWord) yindex) &&
		(f0->index != (DdHalfWord) yindex)) { /* stays */
	        newxkeys++;
		*previousP = f;
		previousP = &(f->next);
	    } else {
		f->index = yindex;
		*lastP = f;
		lastP = &(f->next);
	    }
	    f = next;
	} /* while there are elements in the collision chain */
	*previousP = NULL;
    } /* for each slot of the x subtable */
    *lastP = NULL;


#ifdef DD_COUNT
    table->swapSteps += oldxkeys - newxkeys;
#endif
    /* Take care of the x nodes that must be re-expressed.
    ** They form a linked list pointed by g. Their index has been
    ** changed to yindex already.
    */
    f = g;
    while (f != NULL) {
	next = f->next;
	/* Find f1, f0, f11, f10, f01, f00. */
	f1 = cuddT(f);
	if ((int) f1->index == yindex) {
	    f11 = cuddT(f1); f10 = cuddE(f1);
	} else {
	    f11 = empty; f10 = f1;
	}
	f0 = cuddE(f);
	if ((int) f0->index == yindex) {
	    f01 = cuddT(f0); f00 = cuddE(f0);
	} else {
	    f01 = empty; f00 = f0;
	}

	/* Decrease ref count of f1. */
	cuddSatDec(f1->ref);
	/* Create the new T child. */
	if (f11 == empty) {
	    if (f01 != empty) {
		newf1 = f01;
		cuddSatInc(newf1->ref);
	    }
	    /* else case was already handled when finding nodes
	    ** with both children below level y
	    */
	} else {
	    /* Check xlist for triple (xindex, f11, f01). */
	    posn = ddHash(f11, f01, xshift);
	    /* For each element newf1 in collision list xlist[posn]. */
	    newf1 = xlist[posn];
	    while (newf1 != NULL) {
		if (cuddT(newf1) == f11 && cuddE(newf1) == f01) {
		    cuddSatInc(newf1->ref);
		    break; /* match */
		}
		newf1 = newf1->next;
	    } /* while newf1 */
	    if (newf1 == NULL) {	/* no match */
		newf1 = cuddDynamicAllocNode(table);
		if (newf1 == NULL)
		    goto zddSwapOutOfMem;
		newf1->index = xindex; newf1->ref = 1;
		cuddT(newf1) = f11;
		cuddE(newf1) = f01;
		/* Insert newf1 in the collision list xlist[pos];
		** increase the ref counts of f11 and f01
		*/
		newxkeys++;
		newf1->next = xlist[posn];
		xlist[posn] = newf1;
		cuddSatInc(f11->ref);
		cuddSatInc(f01->ref);
	    }
	}
	cuddT(f) = newf1;

	/* Do the same for f0. */
	/* Decrease ref count of f0. */
	cuddSatDec(f0->ref);
	/* Create the new E child. */
	if (f10 == empty) {
	    newf0 = f00;
	    cuddSatInc(newf0->ref);
	} else {
	    /* Check xlist for triple (xindex, f10, f00). */
	    posn = ddHash(f10, f00, xshift);
	    /* For each element newf0 in collision list xlist[posn]. */
	    newf0 = xlist[posn];
	    while (newf0 != NULL) {
		if (cuddT(newf0) == f10 && cuddE(newf0) == f00) {
		    cuddSatInc(newf0->ref);
		    break; /* match */
		}
		newf0 = newf0->next;
	    } /* while newf0 */
	    if (newf0 == NULL) {	/* no match */
		newf0 = cuddDynamicAllocNode(table);
		if (newf0 == NULL)
		    goto zddSwapOutOfMem;
		newf0->index = xindex; newf0->ref = 1;
		cuddT(newf0) = f10; cuddE(newf0) = f00;
		/* Insert newf0 in the collision list xlist[posn];
		** increase the ref counts of f10 and f00.
		*/
		newxkeys++;
		newf0->next = xlist[posn];
		xlist[posn] = newf0;
		cuddSatInc(f10->ref);
		cuddSatInc(f00->ref);
	    }
	}
	cuddE(f) = newf0;

	/* Insert the modified f in ylist.
	** The modified f does not already exists in ylist.
	** (Because of the uniqueness of the cofactors.)
	*/
	posn = ddHash(newf1, newf0, yshift);
	newykeys++;
	f->next = ylist[posn];
	ylist[posn] = f;
	f = next;
    } /* while f != NULL */

    /* GC the y layer. */

    /* For each node f in ylist. */
    for (i = 0; i < yslots; i++) {
	previousP = &(ylist[i]);
	f = *previousP;
	while (f != NULL) {
	    next = f->next;
	    if (f->ref == 0) {
		cuddSatDec(cuddT(f)->ref);
		cuddSatDec(cuddE(f)->ref);
		cuddDeallocNode(table, f);
		newykeys--;
	    } else {
		*previousP = f;
		previousP = &(f->next);
	    }
	    f = next;
	} /* while f */
	*previousP = NULL;
    } /* for i */

    /* Set the appropriate fields in table. */
    table->subtableZ[x].nodelist = ylist;
    table->subtableZ[x].slots    = yslots;
    table->subtableZ[x].shift    = yshift;
    table->subtableZ[x].keys     = newykeys;
    table->subtableZ[x].maxKeys  = yslots * DD_MAX_SUBTABLE_DENSITY;

    table->subtableZ[y].nodelist = xlist;
    table->subtableZ[y].slots    = xslots;
    table->subtableZ[y].shift    = xshift;
    table->subtableZ[y].keys     = newxkeys;
    table->subtableZ[y].maxKeys  = xslots * DD_MAX_SUBTABLE_DENSITY;

    table->permZ[xindex] = y; table->permZ[yindex] = x;
    table->invpermZ[x] = yindex; table->invpermZ[y] = xindex;

    table->keysZ += newxkeys + newykeys - oldxkeys - oldykeys;

    /* Update univ section; univ[x] remains the same. */
    table->univ[y] = cuddT(table->univ[x]);

    return (table->keysZ);

zddSwapOutOfMem:
    (void) fprintf(table->err, "Error: cuddZddSwapInPlace out of memory\n");

    return (0);

} /* end of cuddZddSwapInPlace */


/**Function********************************************************************

  Synopsis    [Reorders variables by a sequence of (non-adjacent) swaps.]

  Description [Implementation of Plessier's algorithm that reorders
  variables by a sequence of (non-adjacent) swaps.
    <ol>
    <li> Select two variables (RANDOM or HEURISTIC).
    <li> Permute these variables.
    <li> If the nodes have decreased accept the permutation.
    <li> Otherwise reconstruct the original heap.
    <li> Loop.
    </ol>
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddSwapping(
  DdManager * table,
  int lower,
  int upper,
  Cudd_ReorderingType heuristic)
{
    int	i, j;
    int max, keys;
    int nvars;
    int	x, y;
    int iterate;
    int previousSize;
    Move *moves, *move;
    int	pivot;
    int modulo;
    int result;

#ifdef DD_DEBUG
    /* Sanity check */
    assert(lower >= 0 && upper < table->sizeZ && lower <= upper);
#endif

    nvars = upper - lower + 1;
    iterate = nvars;

    for (i = 0; i < iterate; i++) {
	if (heuristic == CUDD_REORDER_RANDOM_PIVOT) {
	    /* Find pivot <= id with maximum keys. */
	    for (max = -1, j = lower; j <= upper; j++) {
		if ((keys = table->subtableZ[j].keys) > max) {
		    max = keys;
		    pivot = j;
		}
	    }

	    modulo = upper - pivot;
	    if (modulo == 0) {
		y = pivot;	/* y = nvars-1 */
	    } else {
		/* y = random # from {pivot+1 .. nvars-1} */
		y = pivot + 1 + (int) (Cudd_Random() % modulo);
	    }

	    modulo = pivot - lower - 1;
	    if (modulo < 1) {	/* if pivot = 1 or 0 */
		x = lower;
	    } else {
		do { /* x = random # from {0 .. pivot-2} */
		    x = (int) Cudd_Random() % modulo;
		} while (x == y);
		  /* Is this condition really needed, since x and y
		     are in regions separated by pivot? */
	    }
	} else {
	    x = (int) (Cudd_Random() % nvars) + lower;
	    do {
		y = (int) (Cudd_Random() % nvars) + lower;
	    } while (x == y);
	}

	previousSize = table->keysZ;
	moves = zddSwapAny(table, x, y);
	if (moves == NULL)
	    goto cuddZddSwappingOutOfMem;

	result = cuddZddSiftingBackward(table, moves, previousSize);
	if (!result)
	    goto cuddZddSwappingOutOfMem;

	while (moves != NULL) {
	    move = moves->next;
	    cuddDeallocMove(table, moves);
	    moves = move;
	}
#ifdef DD_STATS
	if (table->keysZ < (unsigned) previousSize) {
	    (void) fprintf(table->out,"-");
	} else if (table->keysZ > (unsigned) previousSize) {
	    (void) fprintf(table->out,"+");	/* should never happen */
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
    }

    return(1);

cuddZddSwappingOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(0);

} /* end of cuddZddSwapping */


/**Function********************************************************************

  Synopsis    [Implementation of Rudell's sifting algorithm.]

  Description [Implementation of Rudell's sifting algorithm.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries
    in each unique table.
    <li> Sift the variable up and down, remembering each time the
    total size of the DD heap.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddSifting(
  DdManager * table,
  int  lower,
  int  upper)
{
    int	i;
    int	*var;
    int	size;
    int	x;
    int	result;
#ifdef DD_STATS
    int	previousSize;
#endif

    size = table->sizeZ;

    /* Find order in which to sift variables. */
    var = NULL;
    zdd_entry = ALLOC(int, size);
    if (zdd_entry == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddZddSiftingOutOfMem;
    }
    var = ALLOC(int, size);
    if (var == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddZddSiftingOutOfMem;
    }

    for (i = 0; i < size; i++) {
	x = table->permZ[i];
	zdd_entry[i] = table->subtableZ[x].keys;
	var[i] = i;
    }

    qsort((void *)var, size, sizeof(int), (DD_QSFP)cuddZddUniqueCompare);

    /* Now sift. */
    for (i = 0; i < ddMin(table->siftMaxVar, size); i++) {
	if (zddTotalNumberSwapping >= table->siftMaxSwap)
	    break;
        if (util_cpu_time() - table->startTime > table->timeLimit) {
            table->autoDynZ = 0; /* prevent further reordering */
            break;
        }
	x = table->permZ[var[i]];
	if (x < lower || x > upper) continue;
#ifdef DD_STATS
	previousSize = table->keysZ;
#endif
	result = cuddZddSiftingAux(table, x, lower, upper);
	if (!result)
	    goto cuddZddSiftingOutOfMem;
#ifdef DD_STATS
	if (table->keysZ < (unsigned) previousSize) {
	    (void) fprintf(table->out,"-");
	} else if (table->keysZ > (unsigned) previousSize) {
	    (void) fprintf(table->out,"+");	/* should never happen */
	    (void) fprintf(table->out,"\nSize increased from %d to %d while sifting variable %d\n", previousSize, table->keysZ , var[i]);
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
    }

    FREE(var);
    FREE(zdd_entry);

    return(1);

cuddZddSiftingOutOfMem:

    if (zdd_entry != NULL) FREE(zdd_entry);
    if (var != NULL) FREE(var);

    return(0);

} /* end of cuddZddSifting */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Swaps any two variables.]

  Description [Swaps any two variables. Returns the set of moves.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
zddSwapAny(
  DdManager * table,
  int  x,
  int  y)
{
    Move	*move, *moves;
    int		tmp, size;
    int		x_ref, y_ref;
    int		x_next, y_next;
    int		limit_size;

    if (x > y) {	/* make x precede y */
	tmp = x; x = y;	y = tmp;
    }

    x_ref = x; y_ref = y;

    x_next = cuddZddNextHigh(table, x);
    y_next = cuddZddNextLow(table, y);
    moves = NULL;
    limit_size = table->keysZ;

    for (;;) {
	if (x_next == y_next) {	/* x < x_next = y_next < y */
	    size = cuddZddSwapInPlace(table, x, x_next);
	    if (size == 0)
		goto zddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto zddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    size = cuddZddSwapInPlace(table, y_next, y);
	    if (size == 0)
		goto zddSwapAnyOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto zddSwapAnyOutOfMem;
	    move->x = y_next;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    size = cuddZddSwapInPlace(table, x, x_next);
	    if (size == 0)
		goto zddSwapAnyOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto zddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    tmp = x; x = y; y = tmp;

	} else if (x == y_next) { /* x = y_next < y = x_next */
	    size = cuddZddSwapInPlace(table, x, x_next);
	    if (size == 0)
		goto zddSwapAnyOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto zddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    tmp = x; x = y;  y = tmp;
	} else {
	    size = cuddZddSwapInPlace(table, x, x_next);
	    if (size == 0)
		goto zddSwapAnyOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto zddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    size = cuddZddSwapInPlace(table, y_next, y);
	    if (size == 0)
		goto zddSwapAnyOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto zddSwapAnyOutOfMem;
	    move->x = y_next;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    x = x_next; y = y_next;
	}

	x_next = cuddZddNextHigh(table, x);
	y_next = cuddZddNextLow(table, y);
	if (x_next > y_ref)
	    break;	/* if x == y_ref */

	if ((double) size > table->maxGrowth * (double) limit_size)
	    break;
	if (size < limit_size)
	    limit_size = size;
    }
    if (y_next >= x_ref) {
	size = cuddZddSwapInPlace(table, y_next, y);
	if (size == 0)
	    goto zddSwapAnyOutOfMem;
	move = (Move *)cuddDynamicAllocNode(table);
	if (move == NULL)
	    goto zddSwapAnyOutOfMem;
	move->x = y_next;
	move->y = y;
	move->size = size;
	move->next = moves;
	moves = move;
    }

    return(moves);

zddSwapAnyOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(NULL);

} /* end of zddSwapAny */


/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddZddSiftingAux(
  DdManager * table,
  int  x,
  int  x_low,
  int  x_high)
{
    Move	*move;
    Move	*moveUp;	/* list of up move */
    Move	*moveDown;	/* list of down move */

    int		initial_size;
    int		result;

    initial_size = table->keysZ;

#ifdef DD_DEBUG
    assert(table->subtableZ[x].keys > 0);
#endif

    moveDown = NULL;
    moveUp = NULL;

    if (x == x_low) {
	moveDown = cuddZddSiftingDown(table, x, x_high, initial_size);
	/* after that point x --> x_high */
	if (moveDown == NULL)
	    goto cuddZddSiftingAuxOutOfMem;
	result = cuddZddSiftingBackward(table, moveDown,
	    initial_size);
	/* move backward and stop at best position */
	if (!result)
	    goto cuddZddSiftingAuxOutOfMem;

    }
    else if (x == x_high) {
	moveUp = cuddZddSiftingUp(table, x, x_low, initial_size);
	/* after that point x --> x_low */
	if (moveUp == NULL)
	    goto cuddZddSiftingAuxOutOfMem;
	result = cuddZddSiftingBackward(table, moveUp, initial_size);
	/* move backward and stop at best position */
	if (!result)
	    goto cuddZddSiftingAuxOutOfMem;
    }
    else if ((x - x_low) > (x_high - x)) {
	/* must go down first:shorter */
	moveDown = cuddZddSiftingDown(table, x, x_high, initial_size);
	/* after that point x --> x_high */
	if (moveDown == NULL)
	    goto cuddZddSiftingAuxOutOfMem;
	moveUp = cuddZddSiftingUp(table, moveDown->y, x_low,
	    initial_size);
	if (moveUp == NULL)
	    goto cuddZddSiftingAuxOutOfMem;
	result = cuddZddSiftingBackward(table, moveUp, initial_size);
	/* move backward and stop at best position */
	if (!result)
	    goto cuddZddSiftingAuxOutOfMem;
    }
    else {
	moveUp = cuddZddSiftingUp(table, x, x_low, initial_size);
	/* after that point x --> x_high */
	if (moveUp == NULL)
	    goto cuddZddSiftingAuxOutOfMem;
	moveDown = cuddZddSiftingDown(table, moveUp->x, x_high,
	    initial_size);
	/* then move up */
	if (moveDown == NULL)
	    goto cuddZddSiftingAuxOutOfMem;
	result = cuddZddSiftingBackward(table, moveDown,
	    initial_size);
	/* move backward and stop at best position */
	if (!result)
	    goto cuddZddSiftingAuxOutOfMem;
    }

    while (moveDown != NULL) {
	move = moveDown->next;
	cuddDeallocMove(table, moveDown);
	moveDown = move;
    }
    while (moveUp != NULL) {
	move = moveUp->next;
	cuddDeallocMove(table, moveUp);
	moveUp = move;
    }

    return(1);

cuddZddSiftingAuxOutOfMem:
    while (moveDown != NULL) {
	move = moveDown->next;
	cuddDeallocMove(table, moveDown);
	moveDown = move;
    }
    while (moveUp != NULL) {
	move = moveUp->next;
	cuddDeallocMove(table, moveUp);
	moveUp = move;
    }

    return(0);

} /* end of cuddZddSiftingAux */


/**Function********************************************************************

  Synopsis    [Sifts a variable up.]

  Description [Sifts a variable up. Moves y up until either it reaches
  the bound (x_low) or the size of the ZDD heap increases too much.
  Returns the set of moves in case of success; NULL if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
cuddZddSiftingUp(
  DdManager * table,
  int  x,
  int  x_low,
  int  initial_size)
{
    Move	*moves;
    Move	*move;
    int		y;
    int		size;
    int		limit_size = initial_size;

    moves = NULL;
    y = cuddZddNextLow(table, x);
    while (y >= x_low) {
	size = cuddZddSwapInPlace(table, y, x);
	if (size == 0)
	    goto cuddZddSiftingUpOutOfMem;
	move = (Move *)cuddDynamicAllocNode(table);
	if (move == NULL)
	    goto cuddZddSiftingUpOutOfMem;
	move->x = y;
	move->y = x;
	move->size = size;
	move->next = moves;
	moves = move;

	if ((double)size > (double)limit_size * table->maxGrowth)
	    break;
        if (size < limit_size)
	    limit_size = size;

	x = y;
	y = cuddZddNextLow(table, x);
    }
    return(moves);

cuddZddSiftingUpOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(NULL);

} /* end of cuddZddSiftingUp */


/**Function********************************************************************

  Synopsis    [Sifts a variable down.]

  Description [Sifts a variable down. Moves x down until either it
  reaches the bound (x_high) or the size of the ZDD heap increases too
  much. Returns the set of moves in case of success; NULL if memory is
  full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
cuddZddSiftingDown(
  DdManager * table,
  int  x,
  int  x_high,
  int  initial_size)
{
    Move	*moves;
    Move	*move;
    int		y;
    int		size;
    int		limit_size = initial_size;

    moves = NULL;
    y = cuddZddNextHigh(table, x);
    while (y <= x_high) {
	size = cuddZddSwapInPlace(table, x, y);
	if (size == 0)
	    goto cuddZddSiftingDownOutOfMem;
	move = (Move *)cuddDynamicAllocNode(table);
	if (move == NULL)
	    goto cuddZddSiftingDownOutOfMem;
	move->x = x;
	move->y = y;
	move->size = size;
	move->next = moves;
	moves = move;

	if ((double)size > (double)limit_size * table->maxGrowth)
	    break;
        if (size < limit_size)
	    limit_size = size;

	x = y;
	y = cuddZddNextHigh(table, x);
    }
    return(moves);

cuddZddSiftingDownOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(NULL);

} /* end of cuddZddSiftingDown */


/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the ZDD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the ZDD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddZddSiftingBackward(
  DdManager * table,
  Move * moves,
  int  size)
{
    int	    	i;
    int		i_best;
    Move	*move;
    int		res;

    /* Find the minimum size among moves. */
    i_best = -1;
    for (move = moves, i = 0; move != NULL; move = move->next, i++) {
	if (move->size < size) {
	    i_best = i;
	    size = move->size;
	}
    }

    for (move = moves, i = 0; move != NULL; move = move->next, i++) {
	if (i == i_best)
	    break;
	res = cuddZddSwapInPlace(table, move->x, move->y);
	if (!res)
	    return(0);
	if (i_best == -1 && res == size)
	    break;
    }

    return(1);

} /* end of cuddZddSiftingBackward */


/**Function********************************************************************

  Synopsis    [Prepares the ZDD heap for dynamic reordering.]

  Description [Prepares the ZDD heap for dynamic reordering. Does
  garbage collection, to guarantee that there are no dead nodes;
  and clears the cache, which is invalidated by dynamic reordering.]

  SideEffects [None]

******************************************************************************/
static void
zddReorderPreprocess(
  DdManager * table)
{

    /* Clear the cache. */
    cuddCacheFlush(table);

    /* Eliminate dead nodes. Do not scan the cache again. */
    cuddGarbageCollect(table,0);

    return;

} /* end of ddReorderPreprocess */


/**Function********************************************************************

  Synopsis    [Shrinks almost empty ZDD subtables at the end of reordering
  to guarantee that they have a reasonable load factor.]

  Description [Shrinks almost empty subtables at the end of reordering to
  guarantee that they have a reasonable load factor. However, if there many
  nodes are being reclaimed, then no resizing occurs. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddReorderPostprocess(
  DdManager * table)
{
    int i, j, posn;
    DdNodePtr *nodelist, *oldnodelist;
    DdNode *node, *next;
    unsigned int slots, oldslots;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

#ifdef DD_VERBOSE
    (void) fflush(table->out);
#endif

    /* If we have very many reclaimed nodes, we do not want to shrink
    ** the subtables, because this will lead to more garbage
    ** collections. More garbage collections mean shorter mean life for
    ** nodes with zero reference count; hence lower probability of finding
    ** a result in the cache.
    */
    if (table->reclaimed > table->allocated * 0.5) return(1);

    /* Resize subtables. */
    for (i = 0; i < table->sizeZ; i++) {
	int shift;
	oldslots = table->subtableZ[i].slots;
	if (oldslots < table->subtableZ[i].keys * DD_MAX_SUBTABLE_SPARSITY ||
	    oldslots <= table->initSlots) continue;
	oldnodelist = table->subtableZ[i].nodelist;
	slots = oldslots >> 1;
	saveHandler = MMoutOfMemory;
	MMoutOfMemory = Cudd_OutOfMem;
	nodelist = ALLOC(DdNodePtr, slots);
	MMoutOfMemory = saveHandler;
	if (nodelist == NULL) {
	    return(1);
	}
	table->subtableZ[i].nodelist = nodelist;
	table->subtableZ[i].slots = slots;
	table->subtableZ[i].shift++;
	table->subtableZ[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
#ifdef DD_VERBOSE
	(void) fprintf(table->err,
		       "shrunk layer %d (%d keys) from %d to %d slots\n",
		       i, table->subtableZ[i].keys, oldslots, slots);
#endif

	for (j = 0; (unsigned) j < slots; j++) {
	    nodelist[j] = NULL;
	}
	shift = table->subtableZ[i].shift;
	for (j = 0; (unsigned) j < oldslots; j++) {
	    node = oldnodelist[j];
	    while (node != NULL) {
		next = node->next;
		posn = ddHash(cuddT(node), cuddE(node), shift);
		node->next = nodelist[posn];
		nodelist[posn] = node;
		node = next;
	    }
	}
	FREE(oldnodelist);

	table->memused += (slots - oldslots) * sizeof(DdNode *);
	table->slots += slots - oldslots;
	table->minDead = (unsigned) (table->gcFrac * (double) table->slots);
	table->cacheSlack = (int) ddMin(table->maxCacheHard,
	    DD_MAX_CACHE_TO_SLOTS_RATIO*table->slots) -
	    2 * (int) table->cacheSlots;
    }
    /* We don't look at the constant subtable, because it is not
    ** affected by reordering.
    */

    return(1);

} /* end of zddReorderPostprocess */


/**Function********************************************************************

  Synopsis    [Reorders ZDD variables according to a given permutation.]

  Description [Reorders ZDD variables according to a given permutation.
  The i-th permutation array contains the index of the variable that
  should be brought to the i-th level. zddShuffle assumes that no
  dead nodes are present.  The reordering is achieved by a series of
  upward sifts.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso []

******************************************************************************/
static int
zddShuffle(
  DdManager * table,
  int * permutation)
{
    int		index;
    int		level;
    int		position;
    int		numvars;
    int		result;
#ifdef DD_STATS
    unsigned long localTime;
    int		initialSize;
    int		finalSize;
    int		previousSize;
#endif

    zddTotalNumberSwapping = 0;
#ifdef DD_STATS
    localTime = util_cpu_time();
    initialSize = table->keysZ;
    (void) fprintf(table->out,"#:I_SHUFFLE %8d: initial size\n",
		   initialSize); 
#endif

    numvars = table->sizeZ;

    for (level = 0; level < numvars; level++) {
	index = permutation[level];
	position = table->permZ[index];
#ifdef DD_STATS
	previousSize = table->keysZ;
#endif
	result = zddSiftUp(table,position,level);
	if (!result) return(0);
#ifdef DD_STATS
	if (table->keysZ < (unsigned) previousSize) {
	    (void) fprintf(table->out,"-");
	} else if (table->keysZ > (unsigned) previousSize) {
	    (void) fprintf(table->out,"+");	/* should never happen */
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
    }

#ifdef DD_STATS
    (void) fprintf(table->out,"\n");
    finalSize = table->keysZ;
    (void) fprintf(table->out,"#:F_SHUFFLE %8d: final size\n",finalSize); 
    (void) fprintf(table->out,"#:T_SHUFFLE %8g: total time (sec)\n",
	((double)(util_cpu_time() - localTime)/1000.0)); 
    (void) fprintf(table->out,"#:N_SHUFFLE %8d: total swaps\n",
		   zddTotalNumberSwapping);
#endif

    return(1);

} /* end of zddShuffle */


/**Function********************************************************************

  Synopsis    [Moves one ZDD variable up.]

  Description [Takes a ZDD variable from position x and sifts it up to
  position xLow;  xLow should be less than or equal to x.
  Returns 1 if successful; 0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
zddSiftUp(
  DdManager * table,
  int  x,
  int  xLow)
{
    int        y;
    int        size;

    y = cuddZddNextLow(table,x);
    while (y >= xLow) {
	size = cuddZddSwapInPlace(table,y,x);
	if (size == 0) {
	    return(0);
	}
	x = y;
	y = cuddZddNextLow(table,x);
    }
    return(1);

} /* end of zddSiftUp */


/**Function********************************************************************

  Synopsis    [Fixes the ZDD variable group tree after a shuffle.]

  Description [Fixes the ZDD variable group tree after a
  shuffle. Assumes that the order of the variables in a terminal node
  has not been changed.]

  SideEffects [Changes the ZDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static void
zddFixTree(
  DdManager * table,
  MtrNode * treenode)
{
    if (treenode == NULL) return;
    treenode->low = ((int) treenode->index < table->sizeZ) ?
	table->permZ[treenode->index] : treenode->index;
    if (treenode->child != NULL) {
	zddFixTree(table, treenode->child);
    }
    if (treenode->younger != NULL)
	zddFixTree(table, treenode->younger);
    if (treenode->parent != NULL && treenode->low < treenode->parent->low) {
	treenode->parent->low = treenode->low;
	treenode->parent->index = treenode->index;
    }
    return;

} /* end of zddFixTree */

