/**CFile***********************************************************************

  FileName    [cuddReorder.c]

  PackageName [cudd]

  Synopsis    [Functions for dynamic variable reordering.]

  Description [External procedures included in this file:
		<ul>
		<li> Cudd_ReduceHeap()
		<li> Cudd_ShuffleHeap()
		</ul>
	Internal procedures included in this module:
		<ul>
		<li> cuddDynamicAllocNode()
		<li> cuddSifting()
		<li> cuddSwapping()
		<li> cuddNextHigh()
		<li> cuddNextLow()
		<li> cuddSwapInPlace()
		<li> cuddBddAlignToZdd()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> ddUniqueCompare()
		<li> ddSwapAny()
		<li> ddSiftingAux()
		<li> ddSiftingUp()
		<li> ddSiftingDown()
		<li> ddSiftingBackward()
		<li> ddReorderPreprocess()
		<li> ddReorderPostprocess()
		<li> ddShuffle()
		<li> ddSiftUp()
		<li> bddFixTree()
		</ul>]

  Author      [Shipra Panda, Bernard Plessier, Fabio Somenzi]

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
static char rcsid[] DD_UNUSED = "$Id: cuddReorder.c,v 1.71 2012/02/05 01:07:19 fabio Exp $";
#endif

static	int	*entry;

int	ddTotalNumberSwapping;
#ifdef DD_STATS
int	ddTotalNISwaps;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddUniqueCompare (int *ptrX, int *ptrY);
static Move * ddSwapAny (DdManager *table, int x, int y);
static int ddSiftingAux (DdManager *table, int x, int xLow, int xHigh);
static Move * ddSiftingUp (DdManager *table, int y, int xLow);
static Move * ddSiftingDown (DdManager *table, int x, int xHigh);
static int ddSiftingBackward (DdManager *table, int size, Move *moves);
static int ddReorderPreprocess (DdManager *table);
static int ddReorderPostprocess (DdManager *table);
static int ddShuffle (DdManager *table, int *permutation);
static int ddSiftUp (DdManager *table, int x, int xLow);
static void bddFixTree (DdManager *table, MtrNode *treenode);
static int ddUpdateMtrTree (DdManager *table, MtrNode *treenode, int *perm, int *invperm);
static int ddCheckPermuation (DdManager *table, MtrNode *treenode, int *perm, int *invperm);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Main dynamic reordering routine.]

  Description [Main dynamic reordering routine.
  Calls one of the possible reordering procedures:
  <ul>
  <li>Swapping
  <li>Sifting
  <li>Symmetric Sifting
  <li>Group Sifting
  <li>Window Permutation
  <li>Simulated Annealing
  <li>Genetic Algorithm
  <li>Dynamic Programming (exact)
  </ul>

  For sifting, symmetric sifting, group sifting, and window
  permutation it is possible to request reordering to convergence.<p>

  The core of all methods is the reordering procedure
  cuddSwapInPlace() which swaps two adjacent variables and is based
  on Rudell's paper.
  Returns 1 in case of success; 0 otherwise. In the case of symmetric
  sifting (with and without convergence) returns 1 plus the number of
  symmetric variables, in case of success.]

  SideEffects [Changes the variable order for all diagrams and clears
  the cache.]

******************************************************************************/
int
Cudd_ReduceHeap(
  DdManager * table /* DD manager */,
  Cudd_ReorderingType heuristic /* method used for reordering */,
  int  minsize /* bound below which no reordering occurs */)
{
    DdHook *hook;
    int	result;
    unsigned int nextDyn;
#ifdef DD_STATS
    unsigned int initialSize;
    unsigned int finalSize;
#endif
    unsigned long localTime;

    /* Don't reorder if there are too many dead nodes. */
    if (table->keys - table->dead < (unsigned) minsize)
	return(1);

    if (heuristic == CUDD_REORDER_SAME) {
	heuristic = table->autoMethod;
    }
    if (heuristic == CUDD_REORDER_NONE) {
	return(1);
    }

    /* This call to Cudd_ReduceHeap does initiate reordering. Therefore
    ** we count it.
    */
    table->reorderings++;

    localTime = util_cpu_time();

    /* Run the hook functions. */
    hook = table->preReorderingHook;
    while (hook != NULL) {
	int res = (hook->f)(table, "BDD", (void *)heuristic);
	if (res == 0) return(0);
	hook = hook->next;
    }

    if (!ddReorderPreprocess(table)) return(0);
    ddTotalNumberSwapping = 0;

    if (table->keys > table->peakLiveNodes) {
	table->peakLiveNodes = table->keys;
    }
#ifdef DD_STATS
    initialSize = table->keys - table->isolated;
    ddTotalNISwaps = 0;

    switch(heuristic) {
    case CUDD_REORDER_RANDOM:
    case CUDD_REORDER_RANDOM_PIVOT:
	(void) fprintf(table->out,"#:I_RANDOM  ");
	break;
    case CUDD_REORDER_SIFT:
    case CUDD_REORDER_SIFT_CONVERGE:
    case CUDD_REORDER_SYMM_SIFT:
    case CUDD_REORDER_SYMM_SIFT_CONV:
    case CUDD_REORDER_GROUP_SIFT:
    case CUDD_REORDER_GROUP_SIFT_CONV:
	(void) fprintf(table->out,"#:I_SIFTING ");
	break;
    case CUDD_REORDER_WINDOW2:
    case CUDD_REORDER_WINDOW3:
    case CUDD_REORDER_WINDOW4:
    case CUDD_REORDER_WINDOW2_CONV:
    case CUDD_REORDER_WINDOW3_CONV:
    case CUDD_REORDER_WINDOW4_CONV:
	(void) fprintf(table->out,"#:I_WINDOW  ");
	break;
    case CUDD_REORDER_ANNEALING:
	(void) fprintf(table->out,"#:I_ANNEAL  ");
	break;
    case CUDD_REORDER_GENETIC:
	(void) fprintf(table->out,"#:I_GENETIC ");
	break;
    case CUDD_REORDER_LINEAR:
    case CUDD_REORDER_LINEAR_CONVERGE:
	(void) fprintf(table->out,"#:I_LINSIFT ");
	break;
    case CUDD_REORDER_EXACT:
	(void) fprintf(table->out,"#:I_EXACT   ");
	break;
    default:
	return(0);
    }
    (void) fprintf(table->out,"%8d: initial size",initialSize);
#endif

    /* See if we should use alternate threshold for maximum growth. */
    if (table->reordCycle && table->reorderings % table->reordCycle == 0) {
	double saveGrowth = table->maxGrowth;
	table->maxGrowth = table->maxGrowthAlt;
	result = cuddTreeSifting(table,heuristic);
	table->maxGrowth = saveGrowth;
    } else {
	result = cuddTreeSifting(table,heuristic);
    }

#ifdef DD_STATS
    (void) fprintf(table->out,"\n");
    finalSize = table->keys - table->isolated;
    (void) fprintf(table->out,"#:F_REORDER %8d: final size\n",finalSize);
    (void) fprintf(table->out,"#:T_REORDER %8g: total time (sec)\n",
		   ((double)(util_cpu_time() - localTime)/1000.0));
    (void) fprintf(table->out,"#:N_REORDER %8d: total swaps\n",
		   ddTotalNumberSwapping);
    (void) fprintf(table->out,"#:M_REORDER %8d: NI swaps\n",ddTotalNISwaps);
#endif

    if (result == 0)
	return(0);

    if (!ddReorderPostprocess(table))
	return(0);

    if (table->realign) {
	if (!cuddZddAlignToBdd(table))
	    return(0);
    }

    nextDyn = (table->keys - table->constants.keys + 1) *
	      DD_DYN_RATIO + table->constants.keys;
    if (table->reorderings < 20 || nextDyn > table->nextDyn)
	table->nextDyn = nextDyn;
    else
	table->nextDyn += 20;
    if (table->randomizeOrder != 0) {
        table->nextDyn += Cudd_Random() & table->randomizeOrder;
    }
    table->reordered = 1;

    /* Run hook functions. */
    hook = table->postReorderingHook;
    while (hook != NULL) {
	int res = (hook->f)(table, "BDD", (void *)localTime);
	if (res == 0) return(0);
	hook = hook->next;
    }
    /* Update cumulative reordering time. */
    table->reordTime += util_cpu_time() - localTime;

    return(result);

} /* end of Cudd_ReduceHeap */


/**Function********************************************************************

  Synopsis    [Reorders variables according to given permutation.]

  Description [Reorders variables according to given permutation.
  The i-th entry of the permutation array contains the index of the variable
  that should be brought to the i-th level.  The size of the array should be
  equal or greater to the number of variables currently in use.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [Changes the variable order for all diagrams and clears
  the cache.]

  SeeAlso [Cudd_ReduceHeap]

******************************************************************************/
int
Cudd_ShuffleHeap(
  DdManager * table /* DD manager */,
  int * permutation /* required variable permutation */)
{

    int	result;
    int i;
    int identity = 1;
    int *perm;

    /* Don't waste time in case of identity permutation. */
    for (i = 0; i < table->size; i++) {
	if (permutation[i] != table->invperm[i]) {
	    identity = 0;
	    break;
	}
    }
    if (identity == 1) {
	return(1);
    }
    if (!ddReorderPreprocess(table)) return(0);
    if (table->keys > table->peakLiveNodes) {
	table->peakLiveNodes = table->keys;
    }

    perm = ALLOC(int, table->size);
    for (i = 0; i < table->size; i++)
	perm[permutation[i]] = i;
    if (!ddCheckPermuation(table,table->tree,perm,permutation)) {
	FREE(perm);
	return(0);
    }
    if (!ddUpdateMtrTree(table,table->tree,perm,permutation)) {
	FREE(perm);
	return(0);
    }
    FREE(perm);

    result = ddShuffle(table,permutation);

    if (!ddReorderPostprocess(table)) return(0);

    return(result);

} /* end of Cudd_ShuffleHeap */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Dynamically allocates a Node.]

  Description [Dynamically allocates a Node. This procedure is similar
  to cuddAllocNode in Cudd_Table.c, but it does not attempt garbage
  collection, because during reordering there are no dead nodes.
  Returns a pointer to a new node if successful; NULL is memory is
  full.]

  SideEffects [None]

  SeeAlso     [cuddAllocNode]

******************************************************************************/
DdNode *
cuddDynamicAllocNode(
  DdManager * table)
{
    int     i;
    DdNodePtr *mem;
    DdNode *list, *node;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    if (table->nextFree == NULL) {        /* free list is empty */
	/* Try to allocate a new block. */
	saveHandler = MMoutOfMemory;
	MMoutOfMemory = Cudd_OutOfMem;
	mem = (DdNodePtr *) ALLOC(DdNode, DD_MEM_CHUNK + 1);
	MMoutOfMemory = saveHandler;
	if (mem == NULL && table->stash != NULL) {
	    FREE(table->stash);
	    table->stash = NULL;
	    /* Inhibit resizing of tables. */
	    table->maxCacheHard = table->cacheSlots - 1;
	    table->cacheSlack = - (int) (table->cacheSlots + 1);
	    for (i = 0; i < table->size; i++) {
		table->subtables[i].maxKeys <<= 2;
	    }
	    mem = (DdNodePtr *) ALLOC(DdNode,DD_MEM_CHUNK + 1);
	}
	if (mem == NULL) {
	    /* Out of luck. Call the default handler to do
	    ** whatever it specifies for a failed malloc.  If this
	    ** handler returns, then set error code, print
	    ** warning, and return. */
	    (*MMoutOfMemory)(sizeof(DdNode)*(DD_MEM_CHUNK + 1));
	    table->errorCode = CUDD_MEMORY_OUT;
#ifdef DD_VERBOSE
	    (void) fprintf(table->err,
			   "cuddDynamicAllocNode: out of memory");
	    (void) fprintf(table->err,"Memory in use = %lu\n",
			   table->memused);
#endif
	    return(NULL);
	} else {	/* successful allocation; slice memory */
	    unsigned long offset;
	    table->memused += (DD_MEM_CHUNK + 1) * sizeof(DdNode);
	    mem[0] = (DdNode *) table->memoryList;
	    table->memoryList = mem;

	    /* Here we rely on the fact that the size of a DdNode is a
	    ** power of 2 and a multiple of the size of a pointer.
	    ** If we align one node, all the others will be aligned
	    ** as well. */
	    offset = (unsigned long) mem & (sizeof(DdNode) - 1);
	    mem += (sizeof(DdNode) - offset) / sizeof(DdNodePtr);
#ifdef DD_DEBUG
	    assert(((unsigned long) mem & (sizeof(DdNode) - 1)) == 0);
#endif
	    list = (DdNode *) mem;

	    i = 1;
	    do {
		list[i - 1].ref = 0;
		list[i - 1].next = &list[i];
	    } while (++i < DD_MEM_CHUNK);

	    list[DD_MEM_CHUNK-1].ref = 0;
	    list[DD_MEM_CHUNK - 1].next = NULL;

	    table->nextFree = &list[0];
	}
    } /* if free list empty */

    node = table->nextFree;
    table->nextFree = node->next;
    return (node);

} /* end of cuddDynamicAllocNode */


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

******************************************************************************/
int
cuddSifting(
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

    size = table->size;

    /* Find order in which to sift variables. */
    var = NULL;
    entry = ALLOC(int,size);
    if (entry == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddSiftingOutOfMem;
    }
    var = ALLOC(int,size);
    if (var == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddSiftingOutOfMem;
    }

    for (i = 0; i < size; i++) {
	x = table->perm[i];
	entry[i] = table->subtables[x].keys;
	var[i] = i;
    }

    qsort((void *)var,size,sizeof(int),(DD_QSFP)ddUniqueCompare);

    /* Now sift. */
    for (i = 0; i < ddMin(table->siftMaxVar,size); i++) {
	if (ddTotalNumberSwapping >= table->siftMaxSwap)
	    break;
        if (util_cpu_time() - table->startTime + table->reordTime
            > table->timeLimit) {
            table->autoDyn = 0; /* prevent further reordering */
            break;
        }
	x = table->perm[var[i]];

	if (x < lower || x > upper || table->subtables[x].bindVar == 1)
	    continue;
#ifdef DD_STATS
	previousSize = table->keys - table->isolated;
#endif
	result = ddSiftingAux(table, x, lower, upper);
	if (!result) goto cuddSiftingOutOfMem;
#ifdef DD_STATS
	if (table->keys < (unsigned) previousSize + table->isolated) {
	    (void) fprintf(table->out,"-");
	} else if (table->keys > (unsigned) previousSize + table->isolated) {
	    (void) fprintf(table->out,"+");	/* should never happen */
	    (void) fprintf(table->err,"\nSize increased from %d to %d while sifting variable %d\n", previousSize, table->keys - table->isolated, var[i]);
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
    }

    FREE(var);
    FREE(entry);

    return(1);

cuddSiftingOutOfMem:

    if (entry != NULL) FREE(entry);
    if (var != NULL) FREE(var);

    return(0);

} /* end of cuddSifting */


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

******************************************************************************/
int
cuddSwapping(
  DdManager * table,
  int lower,
  int upper,
  Cudd_ReorderingType heuristic)
{
    int	i, j;
    int	max, keys;
    int	nvars;
    int	x, y;
    int	iterate;
    int previousSize;
    Move *moves, *move;
    int	pivot;
    int	modulo;
    int result;

#ifdef DD_DEBUG
    /* Sanity check */
    assert(lower >= 0 && upper < table->size && lower <= upper);
#endif

    nvars = upper - lower + 1;
    iterate = nvars;

    for (i = 0; i < iterate; i++) {
	if (ddTotalNumberSwapping >= table->siftMaxSwap)
	    break;
	if (heuristic == CUDD_REORDER_RANDOM_PIVOT) {
	    max = -1;
	    for (j = lower; j <= upper; j++) {
		if ((keys = table->subtables[j].keys) > max) {
		    max = keys;
		    pivot = j;
		}
	    }

	    modulo = upper - pivot;
	    if (modulo == 0) {
		y = pivot;
	    } else{
		y = pivot + 1 + ((int) Cudd_Random() % modulo);
	    }

	    modulo = pivot - lower - 1;
	    if (modulo < 1) {
		x = lower;
	    } else{
		do {
		    x = (int) Cudd_Random() % modulo;
		} while (x == y);
	    }
	} else {
	    x = ((int) Cudd_Random() % nvars) + lower;
	    do {
		y = ((int) Cudd_Random() % nvars) + lower;
	    } while (x == y);
	}
	previousSize = table->keys - table->isolated;
	moves = ddSwapAny(table,x,y);
	if (moves == NULL) goto cuddSwappingOutOfMem;
	result = ddSiftingBackward(table,previousSize,moves);
	if (!result) goto cuddSwappingOutOfMem;
	while (moves != NULL) {
	    move = moves->next;
	    cuddDeallocMove(table, moves);
	    moves = move;
	}
#ifdef DD_STATS
	if (table->keys < (unsigned) previousSize + table->isolated) {
	    (void) fprintf(table->out,"-");
	} else if (table->keys > (unsigned) previousSize + table->isolated) {
	    (void) fprintf(table->out,"+");	/* should never happen */
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
#if 0
	(void) fprintf(table->out,"#:t_SWAPPING %8d: tmp size\n",
		       table->keys - table->isolated);
#endif
    }

    return(1);

cuddSwappingOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }

    return(0);

} /* end of cuddSwapping */


/**Function********************************************************************

  Synopsis    [Finds the next subtable with a larger index.]

  Description [Finds the next subtable with a larger index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     [cuddNextLow]

******************************************************************************/
int
cuddNextHigh(
  DdManager * table,
  int  x)
{
    return(x+1);

} /* end of cuddNextHigh */


/**Function********************************************************************

  Synopsis    [Finds the next subtable with a smaller index.]

  Description [Finds the next subtable with a smaller index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     [cuddNextHigh]

******************************************************************************/
int
cuddNextLow(
  DdManager * table,
  int  x)
{
    return(x-1);

} /* end of cuddNextLow */


/**Function********************************************************************

  Synopsis    [Swaps two adjacent variables.]

  Description [Swaps two adjacent variables. It assumes that no dead
  nodes are present on entry to this procedure.  The procedure then
  guarantees that no dead nodes will be present when it terminates.
  cuddSwapInPlace assumes that x &lt; y.  Returns the number of keys in
  the table if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int
cuddSwapInPlace(
  DdManager * table,
  int  x,
  int  y)
{
    DdNodePtr *xlist, *ylist;
    int    xindex, yindex;
    int    xslots, yslots;
    int    xshift, yshift;
    int    oldxkeys, oldykeys;
    int    newxkeys, newykeys;
    int    comple, newcomplement;
    int    i;
    Cudd_VariableType varType;
    Cudd_LazyGroupType groupType;
    int    posn;
    int    isolated;
    DdNode *f,*f0,*f1,*f01,*f00,*f11,*f10,*newf1,*newf0;
    DdNode *g,*next;
    DdNodePtr *previousP;
    DdNode *tmp;
    DdNode *sentinel = &(table->sentinel);
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

#ifdef DD_DEBUG
    int    count,idcheck;
#endif

#ifdef DD_DEBUG
    assert(x < y);
    assert(cuddNextHigh(table,x) == y);
    assert(table->subtables[x].keys != 0);
    assert(table->subtables[y].keys != 0);
    assert(table->subtables[x].dead == 0);
    assert(table->subtables[y].dead == 0);
#endif

    ddTotalNumberSwapping++;

    /* Get parameters of x subtable. */
    xindex = table->invperm[x];
    xlist = table->subtables[x].nodelist;
    oldxkeys = table->subtables[x].keys;
    xslots = table->subtables[x].slots;
    xshift = table->subtables[x].shift;

    /* Get parameters of y subtable. */
    yindex = table->invperm[y];
    ylist = table->subtables[y].nodelist;
    oldykeys = table->subtables[y].keys;
    yslots = table->subtables[y].slots;
    yshift = table->subtables[y].shift;

    if (!cuddTestInteract(table,xindex,yindex)) {
#ifdef DD_STATS
	ddTotalNISwaps++;
#endif
	newxkeys = oldxkeys;
	newykeys = oldykeys;
    } else {
	newxkeys = 0;
	newykeys = oldykeys;

	/* Check whether the two projection functions involved in this
	** swap are isolated. At the end, we'll be able to tell how many
	** isolated projection functions are there by checking only these
	** two functions again. This is done to eliminate the isolated
	** projection functions from the node count.
	*/
	isolated = - ((table->vars[xindex]->ref == 1) +
		     (table->vars[yindex]->ref == 1));

	/* The nodes in the x layer that do not depend on
	** y will stay there; the others are put in a chain.
	** The chain is handled as a LIFO; g points to the beginning.
	*/
	g = NULL;
	if ((oldxkeys >= xslots || (unsigned) xslots == table->initSlots) &&
	    oldxkeys <= DD_MAX_SUBTABLE_DENSITY * xslots) {
	    for (i = 0; i < xslots; i++) {
		previousP = &(xlist[i]);
		f = *previousP;
		while (f != sentinel) {
		    next = f->next;
		    f1 = cuddT(f); f0 = cuddE(f);
		    if (f1->index != (DdHalfWord) yindex &&
			Cudd_Regular(f0)->index != (DdHalfWord) yindex) {
			/* stays */
			newxkeys++;
			*previousP = f;
			previousP = &(f->next);
		    } else {
			f->index = yindex;
			f->next = g;
			g = f;
		    }
		    f = next;
		} /* while there are elements in the collision chain */
		*previousP = sentinel;
	    } /* for each slot of the x subtable */
	} else {		/* resize xlist */
	    DdNode *h = NULL;
	    DdNodePtr *newxlist;
	    unsigned int newxslots;
	    int newxshift;
	    /* Empty current xlist. Nodes that stay go to list h;
	    ** nodes that move go to list g. */
	    for (i = 0; i < xslots; i++) {
		f = xlist[i];
		while (f != sentinel) {
		    next = f->next;
		    f1 = cuddT(f); f0 = cuddE(f);
		    if (f1->index != (DdHalfWord) yindex &&
			Cudd_Regular(f0)->index != (DdHalfWord) yindex) {
			/* stays */
			f->next = h;
			h = f;
			newxkeys++;
		    } else {
			f->index = yindex;
			f->next = g;
			g = f;
		    }
		    f = next;
		} /* while there are elements in the collision chain */
	    } /* for each slot of the x subtable */
	    /* Decide size of new subtable. */
	    newxshift = xshift;
	    newxslots = xslots;
	    while ((unsigned) oldxkeys > DD_MAX_SUBTABLE_DENSITY * newxslots) {
		newxshift--;
		newxslots <<= 1;
	    }
	    while ((unsigned) oldxkeys < newxslots &&
		   newxslots > table->initSlots) {
		newxshift++;
		newxslots >>= 1;
	    }
	    /* Try to allocate new table. Be ready to back off. */
	    saveHandler = MMoutOfMemory;
	    MMoutOfMemory = Cudd_OutOfMem;
	    newxlist = ALLOC(DdNodePtr, newxslots);
	    MMoutOfMemory = saveHandler;
	    if (newxlist == NULL) {
		(void) fprintf(table->err, "Unable to resize subtable %d for lack of memory\n", i);
		newxlist = xlist;
		newxslots = xslots;
		newxshift = xshift;
	    } else {
		table->slots += ((int) newxslots - xslots);
		table->minDead = (unsigned)
		    (table->gcFrac * (double) table->slots);
		table->cacheSlack = (int)
		    ddMin(table->maxCacheHard, DD_MAX_CACHE_TO_SLOTS_RATIO
			  * table->slots) - 2 * (int) table->cacheSlots;
		table->memused +=
		    ((int) newxslots - xslots) * sizeof(DdNodePtr);
		FREE(xlist);
		xslots =  newxslots;
		xshift = newxshift;
		xlist = newxlist;
	    }
	    /* Initialize new subtable. */
	    for (i = 0; i < xslots; i++) {
		xlist[i] = sentinel;
	    }
	    /* Move nodes that were parked in list h to their new home. */
	    f = h;
	    while (f != NULL) {
		next = f->next;
		f1 = cuddT(f);
		f0 = cuddE(f);
		/* Check xlist for pair (f11,f01). */
		posn = ddHash(f1, f0, xshift);
		/* For each element tmp in collision list xlist[posn]. */
		previousP = &(xlist[posn]);
		tmp = *previousP;
		while (f1 < cuddT(tmp)) {
		    previousP = &(tmp->next);
		    tmp = *previousP;
		}
		while (f1 == cuddT(tmp) && f0 < cuddE(tmp)) {
		    previousP = &(tmp->next);
		    tmp = *previousP;
		}
		f->next = *previousP;
		*previousP = f;
		f = next;
	    }
	}

#ifdef DD_COUNT
	table->swapSteps += oldxkeys - newxkeys;
#endif
	/* Take care of the x nodes that must be re-expressed.
	** They form a linked list pointed by g. Their index has been
	** already changed to yindex.
	*/
	f = g;
	while (f != NULL) {
	    next = f->next;
	    /* Find f1, f0, f11, f10, f01, f00. */
	    f1 = cuddT(f);
#ifdef DD_DEBUG
	    assert(!(Cudd_IsComplement(f1)));
#endif
	    if ((int) f1->index == yindex) {
		f11 = cuddT(f1); f10 = cuddE(f1);
	    } else {
		f11 = f10 = f1;
	    }
#ifdef DD_DEBUG
	    assert(!(Cudd_IsComplement(f11)));
#endif
	    f0 = cuddE(f);
	    comple = Cudd_IsComplement(f0);
	    f0 = Cudd_Regular(f0);
	    if ((int) f0->index == yindex) {
		f01 = cuddT(f0); f00 = cuddE(f0);
	    } else {
		f01 = f00 = f0;
	    }
	    if (comple) {
		f01 = Cudd_Not(f01);
		f00 = Cudd_Not(f00);
	    }
	    /* Decrease ref count of f1. */
	    cuddSatDec(f1->ref);
	    /* Create the new T child. */
	    if (f11 == f01) {
		newf1 = f11;
		cuddSatInc(newf1->ref);
	    } else {
		/* Check xlist for triple (xindex,f11,f01). */
		posn = ddHash(f11, f01, xshift);
		/* For each element newf1 in collision list xlist[posn]. */
		previousP = &(xlist[posn]);
		newf1 = *previousP;
		while (f11 < cuddT(newf1)) {
		    previousP = &(newf1->next);
		    newf1 = *previousP;
		}
		while (f11 == cuddT(newf1) && f01 < cuddE(newf1)) {
		    previousP = &(newf1->next);
		    newf1 = *previousP;
		}
		if (cuddT(newf1) == f11 && cuddE(newf1) == f01) {
		    cuddSatInc(newf1->ref);
		} else { /* no match */
		    newf1 = cuddDynamicAllocNode(table);
		    if (newf1 == NULL)
			goto cuddSwapOutOfMem;
		    newf1->index = xindex; newf1->ref = 1;
		    cuddT(newf1) = f11;
		    cuddE(newf1) = f01;
		    /* Insert newf1 in the collision list xlist[posn];
		    ** increase the ref counts of f11 and f01.
		    */
		    newxkeys++;
		    newf1->next = *previousP;
		    *previousP = newf1;
		    cuddSatInc(f11->ref);
		    tmp = Cudd_Regular(f01);
		    cuddSatInc(tmp->ref);
		}
	    }
	    cuddT(f) = newf1;
#ifdef DD_DEBUG
	    assert(!(Cudd_IsComplement(newf1)));
#endif

	    /* Do the same for f0, keeping complement dots into account. */
	    /* Decrease ref count of f0. */
	    tmp = Cudd_Regular(f0);
	    cuddSatDec(tmp->ref);
	    /* Create the new E child. */
	    if (f10 == f00) {
		newf0 = f00;
		tmp = Cudd_Regular(newf0);
		cuddSatInc(tmp->ref);
	    } else {
		/* make sure f10 is regular */
		newcomplement = Cudd_IsComplement(f10);
		if (newcomplement) {
		    f10 = Cudd_Not(f10);
		    f00 = Cudd_Not(f00);
		}
		/* Check xlist for triple (xindex,f10,f00). */
		posn = ddHash(f10, f00, xshift);
		/* For each element newf0 in collision list xlist[posn]. */
		previousP = &(xlist[posn]);
		newf0 = *previousP;
		while (f10 < cuddT(newf0)) {
		    previousP = &(newf0->next);
		    newf0 = *previousP;
		}
		while (f10 == cuddT(newf0) && f00 < cuddE(newf0)) {
		    previousP = &(newf0->next);
		    newf0 = *previousP;
		}
		if (cuddT(newf0) == f10 && cuddE(newf0) == f00) {
		    cuddSatInc(newf0->ref);
		} else { /* no match */
		    newf0 = cuddDynamicAllocNode(table);
		    if (newf0 == NULL)
			goto cuddSwapOutOfMem;
		    newf0->index = xindex; newf0->ref = 1;
		    cuddT(newf0) = f10;
		    cuddE(newf0) = f00;
		    /* Insert newf0 in the collision list xlist[posn];
		    ** increase the ref counts of f10 and f00.
		    */
		    newxkeys++;
		    newf0->next = *previousP;
		    *previousP = newf0;
		    cuddSatInc(f10->ref);
		    tmp = Cudd_Regular(f00);
		    cuddSatInc(tmp->ref);
		}
		if (newcomplement) {
		    newf0 = Cudd_Not(newf0);
		}
	    }
	    cuddE(f) = newf0;

	    /* Insert the modified f in ylist.
	    ** The modified f does not already exists in ylist.
	    ** (Because of the uniqueness of the cofactors.)
	    */
	    posn = ddHash(newf1, newf0, yshift);
	    newykeys++;
	    previousP = &(ylist[posn]);
	    tmp = *previousP;
	    while (newf1 < cuddT(tmp)) {
		previousP = &(tmp->next);
		tmp = *previousP;
	    }
	    while (newf1 == cuddT(tmp) && newf0 < cuddE(tmp)) {
		previousP = &(tmp->next);
		tmp = *previousP;
	    }
	    f->next = *previousP;
	    *previousP = f;
	    f = next;
	} /* while f != NULL */

	/* GC the y layer. */

	/* For each node f in ylist. */
	for (i = 0; i < yslots; i++) {
	    previousP = &(ylist[i]);
	    f = *previousP;
	    while (f != sentinel) {
		next = f->next;
		if (f->ref == 0) {
		    tmp = cuddT(f);
		    cuddSatDec(tmp->ref);
		    tmp = Cudd_Regular(cuddE(f));
		    cuddSatDec(tmp->ref);
		    cuddDeallocNode(table,f);
		    newykeys--;
		} else {
		    *previousP = f;
		    previousP = &(f->next);
		}
		f = next;
	    } /* while f */
	    *previousP = sentinel;
	} /* for i */

#ifdef DD_DEBUG
#if 0
	(void) fprintf(table->out,"Swapping %d and %d\n",x,y);
#endif
	count = 0;
	idcheck = 0;
	for (i = 0; i < yslots; i++) {
	    f = ylist[i];
	    while (f != sentinel) {
		count++;
		if (f->index != (DdHalfWord) yindex)
		    idcheck++;
		f = f->next;
	    }
	}
	if (count != newykeys) {
	    (void) fprintf(table->out,
			   "Error in finding newykeys\toldykeys = %d\tnewykeys = %d\tactual = %d\n",
			   oldykeys,newykeys,count);
	}
	if (idcheck != 0)
	    (void) fprintf(table->out,
			   "Error in id's of ylist\twrong id's = %d\n",
			   idcheck);
	count = 0;
	idcheck = 0;
	for (i = 0; i < xslots; i++) {
	    f = xlist[i];
	    while (f != sentinel) {
		count++;
		if (f->index != (DdHalfWord) xindex)
		    idcheck++;
		f = f->next;
	    }
	}
	if (count != newxkeys) {
	    (void) fprintf(table->out,
			   "Error in finding newxkeys\toldxkeys = %d \tnewxkeys = %d \tactual = %d\n",
			   oldxkeys,newxkeys,count);
	}
	if (idcheck != 0)
	    (void) fprintf(table->out,
			   "Error in id's of xlist\twrong id's = %d\n",
			   idcheck);
#endif

	isolated += (table->vars[xindex]->ref == 1) +
		    (table->vars[yindex]->ref == 1);
	table->isolated += isolated;
    }

    /* Set the appropriate fields in table. */
    table->subtables[x].nodelist = ylist;
    table->subtables[x].slots = yslots;
    table->subtables[x].shift = yshift;
    table->subtables[x].keys = newykeys;
    table->subtables[x].maxKeys = yslots * DD_MAX_SUBTABLE_DENSITY;
    i = table->subtables[x].bindVar;
    table->subtables[x].bindVar = table->subtables[y].bindVar;
    table->subtables[y].bindVar = i;
    /* Adjust filds for lazy sifting. */
    varType = table->subtables[x].varType;
    table->subtables[x].varType = table->subtables[y].varType;
    table->subtables[y].varType = varType;
    i = table->subtables[x].pairIndex;
    table->subtables[x].pairIndex = table->subtables[y].pairIndex;
    table->subtables[y].pairIndex = i;
    i = table->subtables[x].varHandled;
    table->subtables[x].varHandled = table->subtables[y].varHandled;
    table->subtables[y].varHandled = i;
    groupType = table->subtables[x].varToBeGrouped;
    table->subtables[x].varToBeGrouped = table->subtables[y].varToBeGrouped;
    table->subtables[y].varToBeGrouped = groupType;

    table->subtables[y].nodelist = xlist;
    table->subtables[y].slots = xslots;
    table->subtables[y].shift = xshift;
    table->subtables[y].keys = newxkeys;
    table->subtables[y].maxKeys = xslots * DD_MAX_SUBTABLE_DENSITY;

    table->perm[xindex] = y; table->perm[yindex] = x;
    table->invperm[x] = yindex; table->invperm[y] = xindex;

    table->keys += newxkeys + newykeys - oldxkeys - oldykeys;

    return(table->keys - table->isolated);

cuddSwapOutOfMem:
    (void) fprintf(table->err,"Error: cuddSwapInPlace out of memory\n");

    return (0);

} /* end of cuddSwapInPlace */


/**Function********************************************************************

  Synopsis    [Reorders BDD variables according to the order of the ZDD
  variables.]

  Description [Reorders BDD variables according to the order of the
  ZDD variables. This function can be called at the end of ZDD
  reordering to insure that the order of the BDD variables is
  consistent with the order of the ZDD variables. The number of ZDD
  variables must be a multiple of the number of BDD variables. Let
  <code>M</code> be the ratio of the two numbers. cuddBddAlignToZdd
  then considers the ZDD variables from <code>M*i</code> to
  <code>(M+1)*i-1</code> as corresponding to BDD variable
  <code>i</code>.  This function should be normally called from
  Cudd_zddReduceHeap, which clears the cache.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [Changes the BDD variable order for all diagrams and performs
  garbage collection of the BDD unique table.]

  SeeAlso [Cudd_ShuffleHeap Cudd_zddReduceHeap]

******************************************************************************/
int
cuddBddAlignToZdd(
  DdManager * table /* DD manager */)
{
    int *invperm;		/* permutation array */
    int M;			/* ratio of ZDD variables to BDD variables */
    int i;			/* loop index */
    int result;			/* return value */

    /* We assume that a ratio of 0 is OK. */
    if (table->size == 0)
	return(1);

    M = table->sizeZ / table->size;
    /* Check whether the number of ZDD variables is a multiple of the
    ** number of BDD variables.
    */
    if (M * table->size != table->sizeZ)
	return(0);
    /* Create and initialize the inverse permutation array. */
    invperm = ALLOC(int,table->size);
    if (invperm == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (i = 0; i < table->sizeZ; i += M) {
	int indexZ = table->invpermZ[i];
	int index  = indexZ / M;
	invperm[i / M] = index;
    }
    /* Eliminate dead nodes. Do not scan the cache again, because we
    ** assume that Cudd_zddReduceHeap has already cleared it.
    */
    cuddGarbageCollect(table,0);

    /* Initialize number of isolated projection functions. */
    table->isolated = 0;
    for (i = 0; i < table->size; i++) {
	if (table->vars[i]->ref == 1) table->isolated++;
    }

    /* Initialize the interaction matrix. */
    result = cuddInitInteract(table);
    if (result == 0) return(0);

    result = ddShuffle(table, invperm);
    FREE(invperm);
    /* Free interaction matrix. */
    FREE(table->interact);
    /* Fix the BDD variable group tree. */
    bddFixTree(table,table->tree);
    return(result);

} /* end of cuddBddAlignToZdd */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the
  variables according to the number of keys in the subtables.
  Returns the difference in number of keys between the two
  variables being compared.]

  SideEffects [None]

******************************************************************************/
static int
ddUniqueCompare(
  int * ptrX,
  int * ptrY)
{
#if 0
    if (entry[*ptrY] == entry[*ptrX]) {
	return((*ptrX) - (*ptrY));
    }
#endif
    return(entry[*ptrY] - entry[*ptrX]);

} /* end of ddUniqueCompare */


/**Function********************************************************************

  Synopsis    [Swaps any two variables.]

  Description [Swaps any two variables. Returns the set of moves.]

  SideEffects [None]

******************************************************************************/
static Move *
ddSwapAny(
  DdManager * table,
  int  x,
  int  y)
{
    Move	*move, *moves;
    int		xRef,yRef;
    int		xNext,yNext;
    int		size;
    int		limitSize;
    int		tmp;

    if (x >y) {
	tmp = x; x = y; y = tmp;
    }

    xRef = x; yRef = y;

    xNext = cuddNextHigh(table,x);
    yNext = cuddNextLow(table,y);
    moves = NULL;
    limitSize = table->keys - table->isolated;

    for (;;) {
	if ( xNext == yNext) {
	    size = cuddSwapInPlace(table,x,xNext);
	    if (size == 0) goto ddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = xNext;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    size = cuddSwapInPlace(table,yNext,y);
	    if (size == 0) goto ddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddSwapAnyOutOfMem;
	    move->x = yNext;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    size = cuddSwapInPlace(table,x,xNext);
	    if (size == 0) goto ddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = xNext;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    tmp = x; x = y; y = tmp;

	} else if (x == yNext) {

	    size = cuddSwapInPlace(table,x,xNext);
	    if (size == 0) goto ddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = xNext;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    tmp = x; x = y; y = tmp;

	} else {
	    size = cuddSwapInPlace(table,x,xNext);
	    if (size == 0) goto ddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddSwapAnyOutOfMem;
	    move->x = x;
	    move->y = xNext;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    size = cuddSwapInPlace(table,yNext,y);
	    if (size == 0) goto ddSwapAnyOutOfMem;
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddSwapAnyOutOfMem;
	    move->x = yNext;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    x = xNext;
	    y = yNext;
	}

	xNext = cuddNextHigh(table,x);
	yNext = cuddNextLow(table,y);
	if (xNext > yRef) break;

	if ((double) size > table->maxGrowth * (double) limitSize) break;
	if (size < limitSize) limitSize = size;
    }
    if (yNext>=xRef) {
	size = cuddSwapInPlace(table,yNext,y);
	if (size == 0) goto ddSwapAnyOutOfMem;
	move = (Move *) cuddDynamicAllocNode(table);
	if (move == NULL) goto ddSwapAnyOutOfMem;
	move->x = yNext;
	move->y = y;
	move->size = size;
	move->next = moves;
	moves = move;
    }

    return(moves);

ddSwapAnyOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(NULL);

} /* end of ddSwapAny */


/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddSiftingAux(
  DdManager * table,
  int  x,
  int  xLow,
  int  xHigh)
{

    Move	*move;
    Move	*moveUp;		/* list of up moves */
    Move	*moveDown;		/* list of down moves */
    int		initialSize;
    int		result;

    initialSize = table->keys - table->isolated;

    moveDown = NULL;
    moveUp = NULL;

    if (x == xLow) {
	moveDown = ddSiftingDown(table,x,xHigh);
	/* At this point x --> xHigh unless bounding occurred. */
	if (moveDown == (Move *) CUDD_OUT_OF_MEM) goto ddSiftingAuxOutOfMem;
	/* Move backward and stop at best position. */
	result = ddSiftingBackward(table,initialSize,moveDown);
	if (!result) goto ddSiftingAuxOutOfMem;

    } else if (x == xHigh) {
	moveUp = ddSiftingUp(table,x,xLow);
	/* At this point x --> xLow unless bounding occurred. */
	if (moveUp == (Move *) CUDD_OUT_OF_MEM) goto ddSiftingAuxOutOfMem;
	/* Move backward and stop at best position. */
	result = ddSiftingBackward(table,initialSize,moveUp);
	if (!result) goto ddSiftingAuxOutOfMem;

    } else if ((x - xLow) > (xHigh - x)) { /* must go down first: shorter */
	moveDown = ddSiftingDown(table,x,xHigh);
	/* At this point x --> xHigh unless bounding occurred. */
	if (moveDown == (Move *) CUDD_OUT_OF_MEM) goto ddSiftingAuxOutOfMem;
	if (moveDown != NULL) {
	    x = moveDown->y;
	}
	moveUp = ddSiftingUp(table,x,xLow);
	if (moveUp == (Move *) CUDD_OUT_OF_MEM) goto ddSiftingAuxOutOfMem;
	/* Move backward and stop at best position */
	result = ddSiftingBackward(table,initialSize,moveUp);
	if (!result) goto ddSiftingAuxOutOfMem;

    } else { /* must go up first: shorter */
	moveUp = ddSiftingUp(table,x,xLow);
	/* At this point x --> xLow unless bounding occurred. */
	if (moveUp == (Move *) CUDD_OUT_OF_MEM) goto ddSiftingAuxOutOfMem;
	if (moveUp != NULL) {
	    x = moveUp->x;
	}
	moveDown = ddSiftingDown(table,x,xHigh);
	if (moveDown == (Move *) CUDD_OUT_OF_MEM) goto ddSiftingAuxOutOfMem;
	/* Move backward and stop at best position. */
	result = ddSiftingBackward(table,initialSize,moveDown);
	if (!result) goto ddSiftingAuxOutOfMem;
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

ddSiftingAuxOutOfMem:
    if (moveDown != (Move *) CUDD_OUT_OF_MEM) {
	while (moveDown != NULL) {
	    move = moveDown->next;
	    cuddDeallocMove(table, moveDown);
	    moveDown = move;
	}
    }
    if (moveUp != (Move *) CUDD_OUT_OF_MEM) {
	while (moveUp != NULL) {
	    move = moveUp->next;
	    cuddDeallocMove(table, moveUp);
	    moveUp = move;
	}
    }

    return(0);

} /* end of ddSiftingAux */


/**Function********************************************************************

  Synopsis    [Sifts a variable up.]

  Description [Sifts a variable up. Moves y up until either it reaches
  the bound (xLow) or the size of the DD heap increases too much.
  Returns the set of moves in case of success; NULL if memory is full.]

  SideEffects [None]

******************************************************************************/
static Move *
ddSiftingUp(
  DdManager * table,
  int  y,
  int  xLow)
{
    Move	*moves;
    Move	*move;
    int		x;
    int		size;
    int		limitSize;
    int		xindex, yindex;
    int		isolated;
    int		L;	/* lower bound on DD size */
#ifdef DD_DEBUG
    int checkL;
    int z;
    int zindex;
#endif

    moves = NULL;
    yindex = table->invperm[y];

    /* Initialize the lower bound.
    ** The part of the DD below y will not change.
    ** The part of the DD above y that does not interact with y will not
    ** change. The rest may vanish in the best case, except for
    ** the nodes at level xLow, which will not vanish, regardless.
    */
    limitSize = L = table->keys - table->isolated;
    for (x = xLow + 1; x < y; x++) {
	xindex = table->invperm[x];
	if (cuddTestInteract(table,xindex,yindex)) {
	    isolated = table->vars[xindex]->ref == 1;
	    L -= table->subtables[x].keys - isolated;
	}
    }
    isolated = table->vars[yindex]->ref == 1;
    L -= table->subtables[y].keys - isolated;

    x = cuddNextLow(table,y);
    while (x >= xLow && L <= limitSize) {
	xindex = table->invperm[x];
#ifdef DD_DEBUG
	checkL = table->keys - table->isolated;
	for (z = xLow + 1; z < y; z++) {
	    zindex = table->invperm[z];
	    if (cuddTestInteract(table,zindex,yindex)) {
		isolated = table->vars[zindex]->ref == 1;
		checkL -= table->subtables[z].keys - isolated;
	    }
	}
	isolated = table->vars[yindex]->ref == 1;
	checkL -= table->subtables[y].keys - isolated;
	assert(L == checkL);
#endif
	size = cuddSwapInPlace(table,x,y);
	if (size == 0) goto ddSiftingUpOutOfMem;
	/* Update the lower bound. */
	if (cuddTestInteract(table,xindex,yindex)) {
	    isolated = table->vars[xindex]->ref == 1;
	    L += table->subtables[y].keys - isolated;
	}
	move = (Move *) cuddDynamicAllocNode(table);
	if (move == NULL) goto ddSiftingUpOutOfMem;
	move->x = x;
	move->y = y;
	move->size = size;
	move->next = moves;
	moves = move;
	if ((double) size > (double) limitSize * table->maxGrowth) break;
	if (size < limitSize) limitSize = size;
	y = x;
	x = cuddNextLow(table,y);
    }
    return(moves);

ddSiftingUpOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return((Move *) CUDD_OUT_OF_MEM);

} /* end of ddSiftingUp */


/**Function********************************************************************

  Synopsis    [Sifts a variable down.]

  Description [Sifts a variable down. Moves x down until either it
  reaches the bound (xHigh) or the size of the DD heap increases too
  much. Returns the set of moves in case of success; NULL if memory is
  full.]

  SideEffects [None]

******************************************************************************/
static Move *
ddSiftingDown(
  DdManager * table,
  int  x,
  int  xHigh)
{
    Move	*moves;
    Move	*move;
    int		y;
    int		size;
    int		R;	/* upper bound on node decrease */
    int		limitSize;
    int		xindex, yindex;
    int		isolated;
#ifdef DD_DEBUG
    int		checkR;
    int		z;
    int		zindex;
#endif

    moves = NULL;
    /* Initialize R */
    xindex = table->invperm[x];
    limitSize = size = table->keys - table->isolated;
    R = 0;
    for (y = xHigh; y > x; y--) {
	yindex = table->invperm[y];
	if (cuddTestInteract(table,xindex,yindex)) {
	    isolated = table->vars[yindex]->ref == 1;
	    R += table->subtables[y].keys - isolated;
	}
    }

    y = cuddNextHigh(table,x);
    while (y <= xHigh && size - R < limitSize) {
#ifdef DD_DEBUG
	checkR = 0;
	for (z = xHigh; z > x; z--) {
	    zindex = table->invperm[z];
	    if (cuddTestInteract(table,xindex,zindex)) {
		isolated = table->vars[zindex]->ref == 1;
		checkR += table->subtables[z].keys - isolated;
	    }
	}
	assert(R == checkR);
#endif
	/* Update upper bound on node decrease. */
	yindex = table->invperm[y];
	if (cuddTestInteract(table,xindex,yindex)) {
	    isolated = table->vars[yindex]->ref == 1;
	    R -= table->subtables[y].keys - isolated;
	}
	size = cuddSwapInPlace(table,x,y);
	if (size == 0) goto ddSiftingDownOutOfMem;
	move = (Move *) cuddDynamicAllocNode(table);
	if (move == NULL) goto ddSiftingDownOutOfMem;
	move->x = x;
	move->y = y;
	move->size = size;
	move->next = moves;
	moves = move;
	if ((double) size > (double) limitSize * table->maxGrowth) break;
	if (size < limitSize) limitSize = size;
	x = y;
	y = cuddNextHigh(table,x);
    }
    return(moves);

ddSiftingDownOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return((Move *) CUDD_OUT_OF_MEM);

} /* end of ddSiftingDown */


/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the DD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the DD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddSiftingBackward(
  DdManager * table,
  int  size,
  Move * moves)
{
    Move *move;
    int	res;

    for (move = moves; move != NULL; move = move->next) {
	if (move->size < size) {
	    size = move->size;
	}
    }

    for (move = moves; move != NULL; move = move->next) {
	if (move->size == size) return(1);
	res = cuddSwapInPlace(table,(int)move->x,(int)move->y);
	if (!res) return(0);
    }

    return(1);

} /* end of ddSiftingBackward */


/**Function********************************************************************

  Synopsis    [Prepares the DD heap for dynamic reordering.]

  Description [Prepares the DD heap for dynamic reordering. Does
  garbage collection, to guarantee that there are no dead nodes;
  clears the cache, which is invalidated by dynamic reordering; initializes
  the number of isolated projection functions; and initializes the
  interaction matrix.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
ddReorderPreprocess(
  DdManager * table)
{
    int i;
    int res;

    /* Clear the cache. */
    cuddCacheFlush(table);
    cuddLocalCacheClearAll(table);

    /* Eliminate dead nodes. Do not scan the cache again. */
    cuddGarbageCollect(table,0);

    /* Initialize number of isolated projection functions. */
    table->isolated = 0;
    for (i = 0; i < table->size; i++) {
	if (table->vars[i]->ref == 1) table->isolated++;
    }

    /* Initialize the interaction matrix. */
    res = cuddInitInteract(table);
    if (res == 0) return(0);

    return(1);

} /* end of ddReorderPreprocess */


/**Function********************************************************************

  Synopsis    [Cleans up at the end of reordering.]

  Description []

  SideEffects [None]

******************************************************************************/
static int
ddReorderPostprocess(
  DdManager * table)
{

#ifdef DD_VERBOSE
    (void) fflush(table->out);
#endif

    /* Free interaction matrix. */
    FREE(table->interact);

    return(1);

} /* end of ddReorderPostprocess */


/**Function********************************************************************

  Synopsis    [Reorders variables according to a given permutation.]

  Description [Reorders variables according to a given permutation.
  The i-th permutation array contains the index of the variable that
  should be brought to the i-th level. ddShuffle assumes that no
  dead nodes are present and that the interaction matrix is properly
  initialized.  The reordering is achieved by a series of upward sifts.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso []

******************************************************************************/
static int
ddShuffle(
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

    ddTotalNumberSwapping = 0;
#ifdef DD_STATS
    localTime = util_cpu_time();
    initialSize = table->keys - table->isolated;
    (void) fprintf(table->out,"#:I_SHUFFLE %8d: initial size\n",
		   initialSize);
    ddTotalNISwaps = 0;
#endif

    numvars = table->size;

    for (level = 0; level < numvars; level++) {
	index = permutation[level];
	position = table->perm[index];
#ifdef DD_STATS
	previousSize = table->keys - table->isolated;
#endif
	result = ddSiftUp(table,position,level);
	if (!result) return(0);
#ifdef DD_STATS
	if (table->keys < (unsigned) previousSize + table->isolated) {
	    (void) fprintf(table->out,"-");
	} else if (table->keys > (unsigned) previousSize + table->isolated) {
	    (void) fprintf(table->out,"+");	/* should never happen */
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif
    }

#ifdef DD_STATS
    (void) fprintf(table->out,"\n");
    finalSize = table->keys - table->isolated;
    (void) fprintf(table->out,"#:F_SHUFFLE %8d: final size\n",finalSize);
    (void) fprintf(table->out,"#:T_SHUFFLE %8g: total time (sec)\n",
	((double)(util_cpu_time() - localTime)/1000.0));
    (void) fprintf(table->out,"#:N_SHUFFLE %8d: total swaps\n",
		   ddTotalNumberSwapping);
    (void) fprintf(table->out,"#:M_SHUFFLE %8d: NI swaps\n",ddTotalNISwaps);
#endif

    return(1);

} /* end of ddShuffle */


/**Function********************************************************************

  Synopsis    [Moves one variable up.]

  Description [Takes a variable from position x and sifts it up to
  position xLow;  xLow should be less than or equal to x.
  Returns 1 if successful; 0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
ddSiftUp(
  DdManager * table,
  int  x,
  int  xLow)
{
    int        y;
    int        size;

    y = cuddNextLow(table,x);
    while (y >= xLow) {
	size = cuddSwapInPlace(table,y,x);
	if (size == 0) {
	    return(0);
	}
	x = y;
	y = cuddNextLow(table,x);
    }
    return(1);

} /* end of ddSiftUp */


/**Function********************************************************************

  Synopsis    [Fixes the BDD variable group tree after a shuffle.]

  Description [Fixes the BDD variable group tree after a
  shuffle. Assumes that the order of the variables in a terminal node
  has not been changed.]

  SideEffects [Changes the BDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static void
bddFixTree(
  DdManager * table,
  MtrNode * treenode)
{
    if (treenode == NULL) return;
    treenode->low = ((int) treenode->index < table->size) ?
	table->perm[treenode->index] : treenode->index;
    if (treenode->child != NULL) {
	bddFixTree(table, treenode->child);
    }
    if (treenode->younger != NULL)
	bddFixTree(table, treenode->younger);
    if (treenode->parent != NULL && treenode->low < treenode->parent->low) {
	treenode->parent->low = treenode->low;
	treenode->parent->index = treenode->index;
    }
    return;

} /* end of bddFixTree */


/**Function********************************************************************

  Synopsis    [Updates the BDD variable group tree before a shuffle.]

  Description [Updates the BDD variable group tree before a shuffle.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Changes the BDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static int
ddUpdateMtrTree(
  DdManager * table,
  MtrNode * treenode,
  int * perm,
  int * invperm)
{
    unsigned int i, size;
    int index, level, minLevel, maxLevel, minIndex;

    if (treenode == NULL) return(1);

    minLevel = CUDD_MAXINDEX;
    maxLevel = 0;
    minIndex = -1;
    /* i : level */
    for (i = treenode->low; i < treenode->low + treenode->size; i++) {
	index = table->invperm[i];
	level = perm[index];
	if (level < minLevel) {
	    minLevel = level;
	    minIndex = index;
	}
	if (level > maxLevel)
	    maxLevel = level;
    }
    size = maxLevel - minLevel + 1;
    if (minIndex == -1) return(0);
    if (size == treenode->size) {
	treenode->low = minLevel;
	treenode->index = minIndex;
    } else {
	return(0);
    }

    if (treenode->child != NULL) {
	if (!ddUpdateMtrTree(table, treenode->child, perm, invperm))
	    return(0);
    }
    if (treenode->younger != NULL) {
	if (!ddUpdateMtrTree(table, treenode->younger, perm, invperm))
	    return(0);
    }
    return(1);
}


/**Function********************************************************************

  Synopsis    [Checks the BDD variable group tree before a shuffle.]

  Description [Checks the BDD variable group tree before a shuffle.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Changes the BDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static int
ddCheckPermuation(
  DdManager * table,
  MtrNode * treenode,
  int * perm,
  int * invperm)
{
    unsigned int i, size;
    int index, level, minLevel, maxLevel;

    if (treenode == NULL) return(1);

    minLevel = table->size;
    maxLevel = 0;
    /* i : level */
    for (i = treenode->low; i < treenode->low + treenode->size; i++) {
	index = table->invperm[i];
	level = perm[index];
	if (level < minLevel)
	    minLevel = level;
	if (level > maxLevel)
	    maxLevel = level;
    }
    size = maxLevel - minLevel + 1;
    if (size != treenode->size)
	return(0);

    if (treenode->child != NULL) {
	if (!ddCheckPermuation(table, treenode->child, perm, invperm))
	    return(0);
    }
    if (treenode->younger != NULL) {
	if (!ddCheckPermuation(table, treenode->younger, perm, invperm))
	    return(0);
    }
    return(1);
}
