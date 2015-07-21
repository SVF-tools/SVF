/**CFile***********************************************************************

  FileName    [cuddTable.c]

  PackageName [cudd]

  Synopsis    [Unique table management functions.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_Prime()
                <li> Cudd_Reserve()
		</ul>
	Internal procedures included in this module:
		<ul>
		<li> cuddAllocNode()
		<li> cuddInitTable()
		<li> cuddFreeTable()
		<li> cuddGarbageCollect()
		<li> cuddZddGetNode()
		<li> cuddZddGetNodeIVO()
		<li> cuddUniqueInter()
		<li> cuddUniqueInterIVO()
		<li> cuddUniqueInterZdd()
		<li> cuddUniqueConst()
		<li> cuddRehash()
		<li> cuddShrinkSubtable()
		<li> cuddInsertSubtables()
		<li> cuddDestroySubtables()
		<li> cuddResizeTableZdd()
		<li> cuddSlowTableGrowth()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> ddRehashZdd()
		<li> ddResizeTable()
		<li> cuddFindParent()
		<li> cuddOrderedInsert()
		<li> cuddOrderedThread()
		<li> cuddRotateLeft()
		<li> cuddRotateRight()
		<li> cuddDoRebalance()
		<li> cuddCheckCollisionOrdering()
		</ul>]

  SeeAlso     []

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

#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
/* Constants for red/black trees. */
#define DD_STACK_SIZE 128
#define DD_RED   0
#define DD_BLACK 1
#define DD_PAGE_SIZE 8192
#define DD_PAGE_MASK ~(DD_PAGE_SIZE - 1)
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/* This is a hack for when CUDD_VALUE_TYPE is double */
typedef union hack {
    CUDD_VALUE_TYPE value;
    unsigned int bits[2];
} hack;

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddTable.c,v 1.126 2012/02/05 01:07:19 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
/* Macros for red/black trees. */
#define DD_INSERT_COMPARE(x,y) \
	(((ptruint) (x) & DD_PAGE_MASK) - ((ptruint) (y) & DD_PAGE_MASK))
#define DD_COLOR(p)  ((p)->index)
#define DD_IS_BLACK(p) ((p)->index == DD_BLACK)
#define DD_IS_RED(p) ((p)->index == DD_RED)
#define DD_LEFT(p) cuddT(p)
#define DD_RIGHT(p) cuddE(p)
#define DD_NEXT(p) ((p)->next)
#endif
#endif


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void ddRehashZdd (DdManager *unique, int i);
static int ddResizeTable (DdManager *unique, int index, int amount);
static int cuddFindParent (DdManager *table, DdNode *node);
DD_INLINE static void ddFixLimits (DdManager *unique);
#ifdef DD_RED_BLACK_FREE_LIST
static void cuddOrderedInsert (DdNodePtr *root, DdNodePtr node);
static DdNode * cuddOrderedThread (DdNode *root, DdNode *list);
static void cuddRotateLeft (DdNodePtr *nodeP);
static void cuddRotateRight (DdNodePtr *nodeP);
static void cuddDoRebalance (DdNodePtr **stack, int stackN);
#endif
static void ddPatchTree (DdManager *dd, MtrNode *treenode);
#ifdef DD_DEBUG
static int cuddCheckCollisionOrdering (DdManager *unique, int i, int j);
#endif
static void ddReportRefMess (DdManager *unique, int i, const char *caller);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Returns the next prime &gt;= p.]

  Description []

  SideEffects [None]

******************************************************************************/
unsigned int
Cudd_Prime(
  unsigned int  p)
{
    int i,pn;

    p--;
    do {
	p++;
	if (p&1) {
	    pn = 1;
	    i = 3;
	    while ((unsigned) (i * i) <= p) {
		if (p % i == 0) {
		    pn = 0;
		    break;
		}
		i += 2;
	    }
	} else {
	    pn = 0;
	}
    } while (!pn);
    return(p);

} /* end of Cudd_Prime */


/**Function********************************************************************

  Synopsis    [Expand manager without creating variables.]

  Description [Expand a manager by a specified number of subtables without
  actually creating new variables.  This function can be used to reduce the
  frequency of resizing when an estimate of the number of variables is
  available.  One would call this function instead of passing the number
  of variables to Cudd_Init if variables should not be created right away
  of if the estimate on their number became available only after the manager
  has been created.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Init]

******************************************************************************/
int
Cudd_Reserve(
  DdManager *manager,
  int amount)
{
    int currentSize = manager->size;
    if (amount < 0)
        return(0);
    if (currentSize + amount < currentSize) /* overflow */
        return(0);
    if (amount <= manager->maxSize - manager->size)
        return(1);
    return ddResizeTable(manager, -1, amount);

} /* end of Cudd_Reserve */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Fast storage allocation for DdNodes in the table.]

  Description [Fast storage allocation for DdNodes in the table. The
  first 4 bytes of a chunk contain a pointer to the next block; the
  rest contains DD_MEM_CHUNK spaces for DdNodes.  Returns a pointer to
  a new node if successful; NULL is memory is full.]

  SideEffects [None]

  SeeAlso     [cuddDynamicAllocNode]

******************************************************************************/
DdNode *
cuddAllocNode(
  DdManager * unique)
{
    int i;
    DdNodePtr *mem;
    DdNode *list, *node;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    if (unique->nextFree == NULL) {	/* free list is empty */
	/* Check for exceeded limits. */
	if ((unique->keys - unique->dead) + (unique->keysZ - unique->deadZ) >
	    unique->maxLive) {
	    unique->errorCode = CUDD_TOO_MANY_NODES;
	    return(NULL);
	}
        if (util_cpu_time() - unique->startTime > unique->timeLimit) {
            unique->errorCode = CUDD_TIMEOUT_EXPIRED;
            return(NULL);
        }
	if (unique->stash == NULL || unique->memused > unique->maxmemhard) {
	    (void) cuddGarbageCollect(unique,1);
	    mem = NULL;
	}
	if (unique->nextFree == NULL) {
	    if (unique->memused > unique->maxmemhard) {
		unique->errorCode = CUDD_MAX_MEM_EXCEEDED;
		return(NULL);
	    }
	    /* Try to allocate a new block. */
	    saveHandler = MMoutOfMemory;
	    MMoutOfMemory = Cudd_OutOfMem;
	    mem = (DdNodePtr *) ALLOC(DdNode,DD_MEM_CHUNK + 1);
	    MMoutOfMemory = saveHandler;
	    if (mem == NULL) {
		/* No more memory: Try collecting garbage. If this succeeds,
		** we end up with mem still NULL, but unique->nextFree !=
		** NULL. */
		if (cuddGarbageCollect(unique,1) == 0) {
		    /* Last resort: Free the memory stashed away, if there
		    ** any. If this succeeeds, mem != NULL and
		    ** unique->nextFree still NULL. */
		    if (unique->stash != NULL) {
			FREE(unique->stash);
			unique->stash = NULL;
			/* Inhibit resizing of tables. */
			cuddSlowTableGrowth(unique);
			/* Now try again. */
			mem = (DdNodePtr *) ALLOC(DdNode,DD_MEM_CHUNK + 1);
		    }
		    if (mem == NULL) {
			/* Out of luck. Call the default handler to do
			** whatever it specifies for a failed malloc.
			** If this handler returns, then set error code,
			** print warning, and return. */
			(*MMoutOfMemory)(sizeof(DdNode)*(DD_MEM_CHUNK + 1));
			unique->errorCode = CUDD_MEMORY_OUT;
#ifdef DD_VERBOSE
			(void) fprintf(unique->err,
				       "cuddAllocNode: out of memory");
			(void) fprintf(unique->err, "Memory in use = %lu\n",
				       unique->memused);
#endif
			return(NULL);
		    }
		}
	    }
	    if (mem != NULL) {	/* successful allocation; slice memory */
		ptruint offset;
		unique->memused += (DD_MEM_CHUNK + 1) * sizeof(DdNode);
		mem[0] = (DdNodePtr) unique->memoryList;
		unique->memoryList = mem;

		/* Here we rely on the fact that a DdNode is as large
		** as 4 pointers.  */
		offset = (ptruint) mem & (sizeof(DdNode) - 1);
		mem += (sizeof(DdNode) - offset) / sizeof(DdNodePtr);
		assert(((ptruint) mem & (sizeof(DdNode) - 1)) == 0);
		list = (DdNode *) mem;

		i = 1;
		do {
		    list[i - 1].ref = 0;
		    list[i - 1].next = &list[i];
		} while (++i < DD_MEM_CHUNK);

		list[DD_MEM_CHUNK-1].ref = 0;
		list[DD_MEM_CHUNK-1].next = NULL;

		unique->nextFree = &list[0];
	    }
	}
    }
    unique->allocated++;
    node = unique->nextFree;
    unique->nextFree = node->next;
    return(node);

} /* end of cuddAllocNode */


/**Function********************************************************************

  Synopsis    [Creates and initializes the unique table.]

  Description [Creates and initializes the unique table. Returns a pointer
  to the table if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Init cuddFreeTable]

******************************************************************************/
DdManager *
cuddInitTable(
  unsigned int numVars  /* Initial number of BDD variables (and subtables) */,
  unsigned int numVarsZ /* Initial number of ZDD variables (and subtables) */,
  unsigned int numSlots /* Initial size of the BDD subtables */,
  unsigned int looseUpTo /* Limit for fast table growth */)
{
    DdManager	*unique = ALLOC(DdManager,1);
    int		i, j;
    DdNodePtr	*nodelist;
    DdNode	*sentinel;
    unsigned int slots;
    int shift;

    if (unique == NULL) {
	return(NULL);
    }
    sentinel = &(unique->sentinel);
    sentinel->ref = 0;
    sentinel->index = 0;
    cuddT(sentinel) = NULL;
    cuddE(sentinel) = NULL;
    sentinel->next = NULL;
    unique->epsilon = DD_EPSILON;
    unique->size = numVars;
    unique->sizeZ = numVarsZ;
    unique->maxSize = ddMax(DD_DEFAULT_RESIZE, numVars);
    unique->maxSizeZ = ddMax(DD_DEFAULT_RESIZE, numVarsZ);

    /* Adjust the requested number of slots to a power of 2. */
    slots = 8;
    while (slots < numSlots) {
	slots <<= 1;
    }
    unique->initSlots = slots;
    shift = sizeof(int) * 8 - cuddComputeFloorLog2(slots);

    unique->slots = (numVars + numVarsZ + 1) * slots;
    unique->keys = 0;
    unique->maxLive = ~0;	/* very large number */
    unique->keysZ = 0;
    unique->dead = 0;
    unique->deadZ = 0;
    unique->gcFrac = DD_GC_FRAC_HI;
    unique->minDead = (unsigned) (DD_GC_FRAC_HI * (double) unique->slots);
    unique->looseUpTo = looseUpTo;
    unique->gcEnabled = 1;
    unique->allocated = 0;
    unique->reclaimed = 0;
    unique->subtables = ALLOC(DdSubtable,unique->maxSize);
    if (unique->subtables == NULL) {
	FREE(unique);
	return(NULL);
    }
    unique->subtableZ = ALLOC(DdSubtable,unique->maxSizeZ);
    if (unique->subtableZ == NULL) {
	FREE(unique->subtables);
	FREE(unique);
	return(NULL);
    }
    unique->perm = ALLOC(int,unique->maxSize);
    if (unique->perm == NULL) {
	FREE(unique->subtables);
	FREE(unique->subtableZ);
	FREE(unique);
	return(NULL);
    }
    unique->invperm = ALLOC(int,unique->maxSize);
    if (unique->invperm == NULL) {
	FREE(unique->subtables);
	FREE(unique->subtableZ);
	FREE(unique->perm);
	FREE(unique);
	return(NULL);
    }
    unique->permZ = ALLOC(int,unique->maxSizeZ);
    if (unique->permZ == NULL) {
	FREE(unique->subtables);
	FREE(unique->subtableZ);
	FREE(unique->perm);
	FREE(unique->invperm);
	FREE(unique);
	return(NULL);
    }
    unique->invpermZ = ALLOC(int,unique->maxSizeZ);
    if (unique->invpermZ == NULL) {
	FREE(unique->subtables);
	FREE(unique->subtableZ);
	FREE(unique->perm);
	FREE(unique->invperm);
	FREE(unique->permZ);
	FREE(unique);
	return(NULL);
    }
    unique->map = NULL;
    unique->stack = ALLOC(DdNodePtr,ddMax(unique->maxSize,unique->maxSizeZ)+1);
    if (unique->stack == NULL) {
	FREE(unique->subtables);
	FREE(unique->subtableZ);
	FREE(unique->perm);
	FREE(unique->invperm);
	FREE(unique->permZ);
	FREE(unique->invpermZ);
	FREE(unique);
	return(NULL);
    }
    unique->stack[0] = NULL; /* to suppress harmless UMR */

#ifndef DD_NO_DEATH_ROW
    unique->deathRowDepth = 1 << cuddComputeFloorLog2(unique->looseUpTo >> 2);
    unique->deathRow = ALLOC(DdNodePtr,unique->deathRowDepth);
    if (unique->deathRow == NULL) {
	FREE(unique->subtables);
	FREE(unique->subtableZ);
	FREE(unique->perm);
	FREE(unique->invperm);
	FREE(unique->permZ);
	FREE(unique->invpermZ);
	FREE(unique->stack);
	FREE(unique);
	return(NULL);
    }
    for (i = 0; i < unique->deathRowDepth; i++) {
	unique->deathRow[i] = NULL;
    }
    unique->nextDead = 0;
    unique->deadMask = unique->deathRowDepth - 1;
#endif

    for (i = 0; (unsigned) i < numVars; i++) {
	unique->subtables[i].slots = slots;
	unique->subtables[i].shift = shift;
	unique->subtables[i].keys = 0;
	unique->subtables[i].dead = 0;
	unique->subtables[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
	unique->subtables[i].bindVar = 0;
	unique->subtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
	unique->subtables[i].pairIndex = 0;
	unique->subtables[i].varHandled = 0;
	unique->subtables[i].varToBeGrouped = CUDD_LAZY_NONE;

	nodelist = unique->subtables[i].nodelist = ALLOC(DdNodePtr,slots);
	if (nodelist == NULL) {
	    for (j = 0; j < i; j++) {
		FREE(unique->subtables[j].nodelist);
	    }
	    FREE(unique->subtables);
	    FREE(unique->subtableZ);
	    FREE(unique->perm);
	    FREE(unique->invperm);
	    FREE(unique->permZ);
	    FREE(unique->invpermZ);
	    FREE(unique->stack);
	    FREE(unique);
	    return(NULL);
	}
	for (j = 0; (unsigned) j < slots; j++) {
	    nodelist[j] = sentinel;
	}
	unique->perm[i] = i;
	unique->invperm[i] = i;
    }
    for (i = 0; (unsigned) i < numVarsZ; i++) {
	unique->subtableZ[i].slots = slots;
	unique->subtableZ[i].shift = shift;
	unique->subtableZ[i].keys = 0;
	unique->subtableZ[i].dead = 0;
	unique->subtableZ[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
	nodelist = unique->subtableZ[i].nodelist = ALLOC(DdNodePtr,slots);
	if (nodelist == NULL) {
	    for (j = 0; (unsigned) j < numVars; j++) {
		FREE(unique->subtables[j].nodelist);
	    }
	    FREE(unique->subtables);
	    for (j = 0; j < i; j++) {
		FREE(unique->subtableZ[j].nodelist);
	    }
	    FREE(unique->subtableZ);
	    FREE(unique->perm);
	    FREE(unique->invperm);
	    FREE(unique->permZ);
	    FREE(unique->invpermZ);
	    FREE(unique->stack);
	    FREE(unique);
	    return(NULL);
	}
	for (j = 0; (unsigned) j < slots; j++) {
	    nodelist[j] = NULL;
	}
	unique->permZ[i] = i;
	unique->invpermZ[i] = i;
    }
    unique->constants.slots = slots;
    unique->constants.shift = shift;
    unique->constants.keys = 0;
    unique->constants.dead = 0;
    unique->constants.maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
    nodelist = unique->constants.nodelist = ALLOC(DdNodePtr,slots);
    if (nodelist == NULL) {
	for (j = 0; (unsigned) j < numVars; j++) {
	    FREE(unique->subtables[j].nodelist);
	}
	FREE(unique->subtables);
	for (j = 0; (unsigned) j < numVarsZ; j++) {
	    FREE(unique->subtableZ[j].nodelist);
	}
	FREE(unique->subtableZ);
	FREE(unique->perm);
	FREE(unique->invperm);
	FREE(unique->permZ);
	FREE(unique->invpermZ);
	FREE(unique->stack);
	FREE(unique);
	return(NULL);
    }
    for (j = 0; (unsigned) j < slots; j++) {
	nodelist[j] = NULL;
    }

    unique->memoryList = NULL;
    unique->nextFree = NULL;

    unique->memused = sizeof(DdManager) + (unique->maxSize + unique->maxSizeZ)
	* (sizeof(DdSubtable) + 2 * sizeof(int)) + (numVars + 1) *
	slots * sizeof(DdNodePtr) +
	(ddMax(unique->maxSize,unique->maxSizeZ) + 1) * sizeof(DdNodePtr);
#ifndef DD_NO_DEATH_ROW
    unique->memused += unique->deathRowDepth * sizeof(DdNodePtr);
#endif

    /* Initialize fields concerned with automatic dynamic reordering. */
    unique->reordered = 0;
    unique->reorderings = 0;
    unique->maxReorderings = ~0;
    unique->siftMaxVar = DD_SIFT_MAX_VAR;
    unique->siftMaxSwap = DD_SIFT_MAX_SWAPS;
    unique->maxGrowth = DD_MAX_REORDER_GROWTH;
    unique->maxGrowthAlt = 2.0 * DD_MAX_REORDER_GROWTH;
    unique->reordCycle = 0;	/* do not use alternate threshold */
    unique->autoDyn = 0;	/* initially disabled */
    unique->autoDynZ = 0;	/* initially disabled */
    unique->autoMethod = CUDD_REORDER_SIFT;
    unique->autoMethodZ = CUDD_REORDER_SIFT;
    unique->realign = 0;	/* initially disabled */
    unique->realignZ = 0;	/* initially disabled */
    unique->nextDyn = DD_FIRST_REORDER;
    unique->countDead = ~0;
    unique->tree = NULL;
    unique->treeZ = NULL;
    unique->groupcheck = CUDD_GROUP_CHECK7;
    unique->recomb = DD_DEFAULT_RECOMB;
    unique->symmviolation = 0;
    unique->arcviolation = 0;
    unique->populationSize = 0;
    unique->numberXovers = 0;
    unique->randomizeOrder = 0;
    unique->linear = NULL;
    unique->linearSize = 0;

    /* Initialize ZDD universe. */
    unique->univ = (DdNodePtr *)NULL;

    /* Initialize auxiliary fields. */
    unique->localCaches = NULL;
    unique->preGCHook = NULL;
    unique->postGCHook = NULL;
    unique->preReorderingHook = NULL;
    unique->postReorderingHook = NULL;
    unique->out = stdout;
    unique->err = stderr;
    unique->errorCode = CUDD_NO_ERROR;
    unique->startTime = util_cpu_time();
    unique->timeLimit = ~0UL;

    /* Initialize statistical counters. */
    unique->maxmemhard = ~ 0UL;
    unique->garbageCollections = 0;
    unique->GCTime = 0;
    unique->reordTime = 0;
#ifdef DD_STATS
    unique->nodesDropped = 0;
    unique->nodesFreed = 0;
#endif
    unique->peakLiveNodes = 0;
#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLookUps = 0;
    unique->uniqueLinks = 0;
#endif
#ifdef DD_COUNT
    unique->recursiveCalls = 0;
    unique->swapSteps = 0;
#ifdef DD_STATS
    unique->nextSample = 250000;
#endif
#endif

    return(unique);

} /* end of cuddInitTable */


/**Function********************************************************************

  Synopsis    [Frees the resources associated to a unique table.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddInitTable]

******************************************************************************/
void
cuddFreeTable(
  DdManager * unique)
{
    DdNodePtr *next;
    DdNodePtr *memlist = unique->memoryList;
    int i;

    if (unique->univ != NULL) cuddZddFreeUniv(unique);
    while (memlist != NULL) {
	next = (DdNodePtr *) memlist[0];	/* link to next block */
	FREE(memlist);
	memlist = next;
    }
    unique->nextFree = NULL;
    unique->memoryList = NULL;

    for (i = 0; i < unique->size; i++) {
	FREE(unique->subtables[i].nodelist);
    }
    for (i = 0; i < unique->sizeZ; i++) {
	FREE(unique->subtableZ[i].nodelist);
    }
    FREE(unique->constants.nodelist);
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique->acache);
    FREE(unique->perm);
    FREE(unique->permZ);
    FREE(unique->invperm);
    FREE(unique->invpermZ);
    FREE(unique->vars);
    if (unique->map != NULL) FREE(unique->map);
    FREE(unique->stack);
#ifndef DD_NO_DEATH_ROW
    FREE(unique->deathRow);
#endif
    if (unique->tree != NULL) Mtr_FreeTree(unique->tree);
    if (unique->treeZ != NULL) Mtr_FreeTree(unique->treeZ);
    if (unique->linear != NULL) FREE(unique->linear);
    while (unique->preGCHook != NULL)
	Cudd_RemoveHook(unique,unique->preGCHook->f,CUDD_PRE_GC_HOOK);
    while (unique->postGCHook != NULL)
	Cudd_RemoveHook(unique,unique->postGCHook->f,CUDD_POST_GC_HOOK);
    while (unique->preReorderingHook != NULL)
	Cudd_RemoveHook(unique,unique->preReorderingHook->f,
			CUDD_PRE_REORDERING_HOOK);
    while (unique->postReorderingHook != NULL)
	Cudd_RemoveHook(unique,unique->postReorderingHook->f,
			CUDD_POST_REORDERING_HOOK);
    FREE(unique);

} /* end of cuddFreeTable */


/**Function********************************************************************

  Synopsis    [Performs garbage collection on the unique tables.]

  Description [Performs garbage collection on the BDD and ZDD unique tables.
  If clearCache is 0, the cache is not cleared. This should only be
  specified if the cache has been cleared right before calling
  cuddGarbageCollect. (As in the case of dynamic reordering.)
  Returns the total number of deleted nodes.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddGarbageCollect(
  DdManager * unique,
  int  clearCache)
{
    DdHook	*hook;
    DdCache	*cache = unique->cache;
    DdNode	*sentinel = &(unique->sentinel);
    DdNodePtr	*nodelist;
    int		i, j, deleted, totalDeleted, totalDeletedZ;
    DdCache	*c;
    DdNode	*node,*next;
    DdNodePtr	*lastP;
    int		slots;
    unsigned long localTime;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
    DdNodePtr	tree;
#else
    DdNodePtr *memListTrav, *nxtNode;
    DdNode *downTrav, *sentry;
    int k;
#endif
#endif

#ifndef DD_NO_DEATH_ROW
    cuddClearDeathRow(unique);
#endif

    hook = unique->preGCHook;
    while (hook != NULL) {
	int res = (hook->f)(unique,"DD",NULL);
	if (res == 0) return(0);
	hook = hook->next;
    }

    if (unique->dead + unique->deadZ == 0) {
	hook = unique->postGCHook;
	while (hook != NULL) {
	    int res = (hook->f)(unique,"DD",NULL);
	    if (res == 0) return(0);
	    hook = hook->next;
	}
	return(0);
    }

    /* If many nodes are being reclaimed, we want to resize the tables
    ** more aggressively, to reduce the frequency of garbage collection.
    */
    if (clearCache && unique->gcFrac == DD_GC_FRAC_LO &&
	unique->slots <= unique->looseUpTo && unique->stash != NULL) {
	unique->minDead = (unsigned) (DD_GC_FRAC_HI * (double) unique->slots);
#ifdef DD_VERBOSE
	(void) fprintf(unique->err,"GC fraction = %.2f\t", DD_GC_FRAC_HI);
	(void) fprintf(unique->err,"minDead = %d\n", unique->minDead);
#endif
	unique->gcFrac = DD_GC_FRAC_HI;
	return(0);
    }

    localTime = util_cpu_time();

    unique->garbageCollections++;
#ifdef DD_VERBOSE
    (void) fprintf(unique->err,
		   "garbage collecting (%d dead BDD nodes out of %d, min %d)...",
		   unique->dead, unique->keys, unique->minDead);
    (void) fprintf(unique->err,
		   "                   (%d dead ZDD nodes out of %d)...",
		   unique->deadZ, unique->keysZ);
#endif

    /* Remove references to garbage collected nodes from the cache. */
    if (clearCache) {
	slots = unique->cacheSlots;
	for (i = 0; i < slots; i++) {
	    c = &cache[i];
	    if (c->data != NULL) {
		if (cuddClean(c->f)->ref == 0 ||
		cuddClean(c->g)->ref == 0 ||
		(((ptruint)c->f & 0x2) && Cudd_Regular(c->h)->ref == 0) ||
		(c->data != DD_NON_CONSTANT &&
		Cudd_Regular(c->data)->ref == 0)) {
		    c->data = NULL;
		    unique->cachedeletions++;
		}
	    }
	}
	cuddLocalCacheClearDead(unique);
    }

    /* Now return dead nodes to free list. Count them for sanity check. */
    totalDeleted = 0;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
    tree = NULL;
#endif
#endif

    for (i = 0; i < unique->size; i++) {
	if (unique->subtables[i].dead == 0) continue;
	nodelist = unique->subtables[i].nodelist;

	deleted = 0;
	slots = unique->subtables[i].slots;
	for (j = 0; j < slots; j++) {
	    lastP = &(nodelist[j]);
	    node = *lastP;
	    while (node != sentinel) {
		next = node->next;
		if (node->ref == 0) {
		    deleted++;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
		    cuddOrderedInsert(&tree,node);
#endif
#else
		    cuddDeallocNode(unique,node);
#endif
		} else {
		    *lastP = node;
		    lastP = &(node->next);
		}
		node = next;
	    }
	    *lastP = sentinel;
	}
	if ((unsigned) deleted != unique->subtables[i].dead) {
	    ddReportRefMess(unique, i, "cuddGarbageCollect");
	}
	totalDeleted += deleted;
	unique->subtables[i].keys -= deleted;
	unique->subtables[i].dead = 0;
    }
    if (unique->constants.dead != 0) {
	nodelist = unique->constants.nodelist;
	deleted = 0;
	slots = unique->constants.slots;
	for (j = 0; j < slots; j++) {
	    lastP = &(nodelist[j]);
	    node = *lastP;
	    while (node != NULL) {
		next = node->next;
		if (node->ref == 0) {
		    deleted++;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
		    cuddOrderedInsert(&tree,node);
#endif
#else
		    cuddDeallocNode(unique,node);
#endif
		} else {
		    *lastP = node;
		    lastP = &(node->next);
		}
		node = next;
	    }
	    *lastP = NULL;
	}
	if ((unsigned) deleted != unique->constants.dead) {
	    ddReportRefMess(unique, CUDD_CONST_INDEX, "cuddGarbageCollect");
	}
	totalDeleted += deleted;
	unique->constants.keys -= deleted;
	unique->constants.dead = 0;
    }
    if ((unsigned) totalDeleted != unique->dead) {
	ddReportRefMess(unique, -1, "cuddGarbageCollect");
    }
    unique->keys -= totalDeleted;
    unique->dead = 0;
#ifdef DD_STATS
    unique->nodesFreed += (double) totalDeleted;
#endif

    totalDeletedZ = 0;

    for (i = 0; i < unique->sizeZ; i++) {
	if (unique->subtableZ[i].dead == 0) continue;
	nodelist = unique->subtableZ[i].nodelist;

	deleted = 0;
	slots = unique->subtableZ[i].slots;
	for (j = 0; j < slots; j++) {
	    lastP = &(nodelist[j]);
	    node = *lastP;
	    while (node != NULL) {
		next = node->next;
		if (node->ref == 0) {
		    deleted++;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
		    cuddOrderedInsert(&tree,node);
#endif
#else
		    cuddDeallocNode(unique,node);
#endif
		} else {
		    *lastP = node;
		    lastP = &(node->next);
		}
		node = next;
	    }
	    *lastP = NULL;
	}
	if ((unsigned) deleted != unique->subtableZ[i].dead) {
	    ddReportRefMess(unique, i, "cuddGarbageCollect");
	}
	totalDeletedZ += deleted;
	unique->subtableZ[i].keys -= deleted;
	unique->subtableZ[i].dead = 0;
    }

    /* No need to examine the constant table for ZDDs.
    ** If we did we should be careful not to count whatever dead
    ** nodes we found there among the dead ZDD nodes. */
    if ((unsigned) totalDeletedZ != unique->deadZ) {
	ddReportRefMess(unique, -1, "cuddGarbageCollect");
    }
    unique->keysZ -= totalDeletedZ;
    unique->deadZ = 0;
#ifdef DD_STATS
    unique->nodesFreed += (double) totalDeletedZ;
#endif


#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
    unique->nextFree = cuddOrderedThread(tree,unique->nextFree);
#else
    memListTrav = unique->memoryList;
    sentry = NULL;
    while (memListTrav != NULL) {
	ptruint offset;
	nxtNode = (DdNodePtr *)memListTrav[0];
	offset = (ptruint) memListTrav & (sizeof(DdNode) - 1);
	memListTrav += (sizeof(DdNode) - offset) / sizeof(DdNodePtr);
	downTrav = (DdNode *)memListTrav;
	k = 0;
	do {
	    if (downTrav[k].ref == 0) {
		if (sentry == NULL) {
		    unique->nextFree = sentry = &downTrav[k];
		} else {
		    /* First hook sentry->next to the dead node and then
		    ** reassign sentry to the dead node. */
		    sentry = (sentry->next = &downTrav[k]);
		}
	    }
	} while (++k < DD_MEM_CHUNK);
	memListTrav = nxtNode;
    }
    sentry->next = NULL;
#endif
#endif

    unique->GCTime += util_cpu_time() - localTime;

    hook = unique->postGCHook;
    while (hook != NULL) {
	int res = (hook->f)(unique,"DD",NULL);
	if (res == 0) return(0);
	hook = hook->next;
    }

#ifdef DD_VERBOSE
    (void) fprintf(unique->err," done\n");
#endif

    return(totalDeleted+totalDeletedZ);

} /* end of cuddGarbageCollect */


/**Function********************************************************************

  Synopsis [Wrapper for cuddUniqueInterZdd.]

  Description [Wrapper for cuddUniqueInterZdd, which applies the ZDD
  reduction rule. Returns a pointer to the result node under normal
  conditions; NULL if reordering occurred or memory was exhausted.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInterZdd]

******************************************************************************/
DdNode *
cuddZddGetNode(
  DdManager * zdd,
  int  id,
  DdNode * T,
  DdNode * E)
{
    DdNode	*node;

    if (T == DD_ZERO(zdd))
	return(E);
    node = cuddUniqueInterZdd(zdd, id, T, E);
    return(node);

} /* end of cuddZddGetNode */


/**Function********************************************************************

  Synopsis [Wrapper for cuddUniqueInterZdd that is independent of variable
  ordering.]

  Description [Wrapper for cuddUniqueInterZdd that is independent of
  variable ordering (IVO). This function does not require parameter
  index to precede the indices of the top nodes of g and h in the
  variable order.  Returns a pointer to the result node under normal
  conditions; NULL if reordering occurred or memory was exhausted.]

  SideEffects [None]

  SeeAlso     [cuddZddGetNode cuddZddIsop]

******************************************************************************/
DdNode *
cuddZddGetNodeIVO(
  DdManager * dd,
  int  index,
  DdNode * g,
  DdNode * h)
{
    DdNode	*f, *r, *t;
    DdNode	*zdd_one = DD_ONE(dd);
    DdNode	*zdd_zero = DD_ZERO(dd);

    f = cuddUniqueInterZdd(dd, index, zdd_one, zdd_zero);
    if (f == NULL) {
	return(NULL);
    }
    cuddRef(f);
    t = cuddZddProduct(dd, f, g);
    if (t == NULL) {
	Cudd_RecursiveDerefZdd(dd, f);
	return(NULL);
    }
    cuddRef(t);
    Cudd_RecursiveDerefZdd(dd, f);
    r = cuddZddUnion(dd, t, h);
    if (r == NULL) {
	Cudd_RecursiveDerefZdd(dd, t);
	return(NULL);
    }
    cuddRef(r);
    Cudd_RecursiveDerefZdd(dd, t);

    cuddDeref(r);
    return(r);

} /* end of cuddZddGetNodeIVO */


/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of an internal node.]

  Description [Checks the unique table for the existence of an internal
  node. If it does not exist, it creates a new one.  Does not
  modify the reference count of whatever is returned.  A newly created
  internal node comes back with a reference count 0.  For a newly
  created node, increments the reference counts of what T and E point
  to.  Returns a pointer to the new node if successful; NULL if memory
  is exhausted or if reordering took place.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInterZdd]

******************************************************************************/
DdNode *
cuddUniqueInter(
  DdManager * unique,
  int  index,
  DdNode * T,
  DdNode * E)
{
    int pos;
    unsigned int level;
    int retval;
    DdNodePtr *nodelist;
    DdNode *looking;
    DdNodePtr *previousP;
    DdSubtable *subtable;
    int gcNumber;

#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLookUps++;
#endif

    if ((0x1ffffUL & (unsigned long) unique->cacheMisses) == 0) {
        if (util_cpu_time() - unique->startTime > unique->timeLimit) {
            unique->errorCode = CUDD_TIMEOUT_EXPIRED;
            return(NULL);
        }
    }
    if (index >= unique->size) {
        int amount = ddMax(DD_DEFAULT_RESIZE,unique->size/20);
        if (!ddResizeTable(unique,index,amount)) return(NULL);
    }

    level = unique->perm[index];
    subtable = &(unique->subtables[level]);

#ifdef DD_DEBUG
    assert(level < (unsigned) cuddI(unique,T->index));
    assert(level < (unsigned) cuddI(unique,Cudd_Regular(E)->index));
#endif

    pos = ddHash(T, E, subtable->shift);
    nodelist = subtable->nodelist;
    previousP = &(nodelist[pos]);
    looking = *previousP;

    while (T < cuddT(looking)) {
	previousP = &(looking->next);
	looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
	unique->uniqueLinks++;
#endif
    }
    while (T == cuddT(looking) && E < cuddE(looking)) {
	previousP = &(looking->next);
	looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
	unique->uniqueLinks++;
#endif
    }
    if (T == cuddT(looking) && E == cuddE(looking)) {
	if (looking->ref == 0) {
	    cuddReclaim(unique,looking);
	}
	return(looking);
    }

    /* countDead is 0 if deads should be counted and ~0 if they should not. */
    if (unique->autoDyn &&
        unique->keys - (unique->dead & unique->countDead) >= unique->nextDyn &&
        unique->maxReorderings > 0) {
        unsigned long cpuTime;
#ifdef DD_DEBUG
	retval = Cudd_DebugCheck(unique);
	if (retval != 0) return(NULL);
	retval = Cudd_CheckKeys(unique);
	if (retval != 0) return(NULL);
#endif
	retval = Cudd_ReduceHeap(unique,unique->autoMethod,10); /* 10 = whatever */
        unique->maxReorderings--;
	if (retval == 0) {
            unique->reordered = 2;
        } else if ((cpuTime = util_cpu_time()) - unique->startTime > unique->timeLimit) {
            unique->errorCode = CUDD_TIMEOUT_EXPIRED;
            unique->reordered = 0;
        } else if (unique->timeLimit - (cpuTime - unique->startTime)
                   < unique->reordTime) {
            unique->autoDyn = 0;
        }
#ifdef DD_DEBUG
	retval = Cudd_DebugCheck(unique);
	if (retval != 0) unique->reordered = 2;
	retval = Cudd_CheckKeys(unique);
	if (retval != 0) unique->reordered = 2;
#endif
	return(NULL);
    }

    if (subtable->keys > subtable->maxKeys) {
	if (unique->gcEnabled &&
	    ((unique->dead > unique->minDead) ||
	    ((unique->dead > unique->minDead / 2) &&
	    (subtable->dead > subtable->keys * 0.95)))) { /* too many dead */
            if (util_cpu_time() - unique->startTime > unique->timeLimit) {
                unique->errorCode = CUDD_TIMEOUT_EXPIRED;
                return(NULL);
            }
	    (void) cuddGarbageCollect(unique,1);
	} else {
	    cuddRehash(unique,(int)level);
	}
	/* Update pointer to insertion point. In the case of rehashing,
	** the slot may have changed. In the case of garbage collection,
	** the predecessor may have been dead. */
	pos = ddHash(T, E, subtable->shift);
	nodelist = subtable->nodelist;
	previousP = &(nodelist[pos]);
	looking = *previousP;

	while (T < cuddT(looking)) {
	    previousP = &(looking->next);
	    looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
	    unique->uniqueLinks++;
#endif
	}
	while (T == cuddT(looking) && E < cuddE(looking)) {
	    previousP = &(looking->next);
	    looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
	    unique->uniqueLinks++;
#endif
	}
    }

    gcNumber = unique->garbageCollections;
    looking = cuddAllocNode(unique);
    if (looking == NULL) {
	return(NULL);
    }
    unique->keys++;
    subtable->keys++;

    if (gcNumber != unique->garbageCollections) {
	DdNode *looking2;
	pos = ddHash(T, E, subtable->shift);
	nodelist = subtable->nodelist;
	previousP = &(nodelist[pos]);
	looking2 = *previousP;

	while (T < cuddT(looking2)) {
	    previousP = &(looking2->next);
	    looking2 = *previousP;
#ifdef DD_UNIQUE_PROFILE
	    unique->uniqueLinks++;
#endif
	}
	while (T == cuddT(looking2) && E < cuddE(looking2)) {
	    previousP = &(looking2->next);
	    looking2 = *previousP;
#ifdef DD_UNIQUE_PROFILE
	    unique->uniqueLinks++;
#endif
	}
    }
    looking->index = index;
    cuddT(looking) = T;
    cuddE(looking) = E;
    looking->next = *previousP;
    *previousP = looking;
    cuddSatInc(T->ref);		/* we know T is a regular pointer */
    cuddRef(E);

#ifdef DD_DEBUG
    cuddCheckCollisionOrdering(unique,level,pos);
#endif

    return(looking);

} /* end of cuddUniqueInter */


/**Function********************************************************************

  Synopsis [Wrapper for cuddUniqueInter that is independent of variable
  ordering.]

  Description [Wrapper for cuddUniqueInter that is independent of
  variable ordering (IVO). This function does not require parameter
  index to precede the indices of the top nodes of T and E in the
  variable order.  Returns a pointer to the result node under normal
  conditions; NULL if reordering occurred or memory was exhausted.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInter Cudd_MakeBddFromZddCover]

******************************************************************************/
DdNode *
cuddUniqueInterIVO(
  DdManager * unique,
  int  index,
  DdNode * T,
  DdNode * E)
{
    DdNode *result;
    DdNode *v;

    v = cuddUniqueInter(unique, index, DD_ONE(unique),
			Cudd_Not(DD_ONE(unique)));
    if (v == NULL)
	return(NULL);
    /* Since v is a projection function, we can skip the call to cuddRef. */
    result = cuddBddIteRecur(unique, v, T, E);
    return(result);

} /* end of cuddUniqueInterIVO */


/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of an internal
  ZDD node.]

  Description [Checks the unique table for the existence of an internal
  ZDD node. If it does not exist, it creates a new one.  Does not
  modify the reference count of whatever is returned.  A newly created
  internal node comes back with a reference count 0.  For a newly
  created node, increments the reference counts of what T and E point
  to.  Returns a pointer to the new node if successful; NULL if memory
  is exhausted or if reordering took place.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInter]

******************************************************************************/
DdNode *
cuddUniqueInterZdd(
  DdManager * unique,
  int  index,
  DdNode * T,
  DdNode * E)
{
    int pos;
    unsigned int level;
    int retval;
    DdNodePtr *nodelist;
    DdNode *looking;
    DdSubtable *subtable;

#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLookUps++;
#endif

    if (index >= unique->sizeZ) {
	if (!cuddResizeTableZdd(unique,index)) return(NULL);
    }

    level = unique->permZ[index];
    subtable = &(unique->subtableZ[level]);

#ifdef DD_DEBUG
    assert(level < (unsigned) cuddIZ(unique,T->index));
    assert(level < (unsigned) cuddIZ(unique,Cudd_Regular(E)->index));
#endif

    if (subtable->keys > subtable->maxKeys) {
	if (unique->gcEnabled && ((unique->deadZ > unique->minDead) ||
	(10 * subtable->dead > 9 * subtable->keys))) {	/* too many dead */
	    (void) cuddGarbageCollect(unique,1);
	} else {
	    ddRehashZdd(unique,(int)level);
	}
    }

    pos = ddHash(T, E, subtable->shift);
    nodelist = subtable->nodelist;
    looking = nodelist[pos];

    while (looking != NULL) {
	if (cuddT(looking) == T && cuddE(looking) == E) {
	    if (looking->ref == 0) {
		cuddReclaimZdd(unique,looking);
	    }
	    return(looking);
	}
	looking = looking->next;
#ifdef DD_UNIQUE_PROFILE
	unique->uniqueLinks++;
#endif
    }

    /* countDead is 0 if deads should be counted and ~0 if they should not. */
    if (unique->autoDynZ &&
    unique->keysZ - (unique->deadZ & unique->countDead) >= unique->nextDyn) {
#ifdef DD_DEBUG
	retval = Cudd_DebugCheck(unique);
	if (retval != 0) return(NULL);
	retval = Cudd_CheckKeys(unique);
	if (retval != 0) return(NULL);
#endif
	retval = Cudd_zddReduceHeap(unique,unique->autoMethodZ,10); /* 10 = whatever */
	if (retval == 0) unique->reordered = 2;
#ifdef DD_DEBUG
	retval = Cudd_DebugCheck(unique);
	if (retval != 0) unique->reordered = 2;
	retval = Cudd_CheckKeys(unique);
	if (retval != 0) unique->reordered = 2;
#endif
	return(NULL);
    }

    unique->keysZ++;
    subtable->keys++;

    looking = cuddAllocNode(unique);
    if (looking == NULL) return(NULL);
    looking->index = index;
    cuddT(looking) = T;
    cuddE(looking) = E;
    looking->next = nodelist[pos];
    nodelist[pos] = looking;
    cuddRef(T);
    cuddRef(E);

    return(looking);

} /* end of cuddUniqueInterZdd */


/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of a constant node.]

  Description [Checks the unique table for the existence of a constant node.
  If it does not exist, it creates a new one.  Does not
  modify the reference count of whatever is returned.  A newly created
  internal node comes back with a reference count 0.  Returns a
  pointer to the new node.]

  SideEffects [None]

******************************************************************************/
DdNode *
cuddUniqueConst(
  DdManager * unique,
  CUDD_VALUE_TYPE  value)
{
    int pos;
    DdNodePtr *nodelist;
    DdNode *looking;
    hack split;

#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLookUps++;
#endif

    if (unique->constants.keys > unique->constants.maxKeys) {
	if (unique->gcEnabled && ((unique->dead > unique->minDead) ||
	(10 * unique->constants.dead > 9 * unique->constants.keys))) {	/* too many dead */
	    (void) cuddGarbageCollect(unique,1);
	} else {
	    cuddRehash(unique,CUDD_CONST_INDEX);
	}
    }

    cuddAdjust(value); /* for the case of crippled infinities */

    if (ddAbs(value) < unique->epsilon) {
	value = 0.0;
    }
    split.value = value;

    pos = ddHash(split.bits[0], split.bits[1], unique->constants.shift);
    nodelist = unique->constants.nodelist;
    looking = nodelist[pos];

    /* Here we compare values both for equality and for difference less
     * than epsilon. The first comparison is required when values are
     * infinite, since Infinity - Infinity is NaN and NaN < X is 0 for
     * every X.
     */
    while (looking != NULL) {
	if (looking->type.value == value ||
	ddEqualVal(looking->type.value,value,unique->epsilon)) {
	    if (looking->ref == 0) {
		cuddReclaim(unique,looking);
	    }
	    return(looking);
	}
	looking = looking->next;
#ifdef DD_UNIQUE_PROFILE
	unique->uniqueLinks++;
#endif
    }

    unique->keys++;
    unique->constants.keys++;

    looking = cuddAllocNode(unique);
    if (looking == NULL) return(NULL);
    looking->index = CUDD_CONST_INDEX;
    looking->type.value = value;
    looking->next = nodelist[pos];
    nodelist[pos] = looking;

    return(looking);

} /* end of cuddUniqueConst */


/**Function********************************************************************

  Synopsis    [Rehashes a unique subtable.]

  Description [Doubles the size of a unique subtable and rehashes its
  contents.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddRehash(
  DdManager * unique,
  int i)
{
    unsigned int slots, oldslots;
    int shift, oldshift;
    int j, pos;
    DdNodePtr *nodelist, *oldnodelist;
    DdNode *node, *next;
    DdNode *sentinel = &(unique->sentinel);
    hack split;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    if (unique->gcFrac == DD_GC_FRAC_HI && unique->slots > unique->looseUpTo) {
	unique->gcFrac = DD_GC_FRAC_LO;
	unique->minDead = (unsigned) (DD_GC_FRAC_LO * (double) unique->slots);
#ifdef DD_VERBOSE
	(void) fprintf(unique->err,"GC fraction = %.2f\t", DD_GC_FRAC_LO);
	(void) fprintf(unique->err,"minDead = %d\n", unique->minDead);
#endif
    }

    if (unique->gcFrac != DD_GC_FRAC_MIN && unique->memused > unique->maxmem) {
	unique->gcFrac = DD_GC_FRAC_MIN;
	unique->minDead = (unsigned) (DD_GC_FRAC_MIN * (double) unique->slots);
#ifdef DD_VERBOSE
	(void) fprintf(unique->err,"GC fraction = %.2f\t", DD_GC_FRAC_MIN);
	(void) fprintf(unique->err,"minDead = %d\n", unique->minDead);
#endif
	cuddShrinkDeathRow(unique);
	if (cuddGarbageCollect(unique,1) > 0) return;
    }

    if (i != CUDD_CONST_INDEX) {
	oldslots = unique->subtables[i].slots;
	oldshift = unique->subtables[i].shift;
	oldnodelist = unique->subtables[i].nodelist;

	/* Compute the new size of the subtable. */
	slots = oldslots << 1;
	shift = oldshift - 1;

	saveHandler = MMoutOfMemory;
	MMoutOfMemory = Cudd_OutOfMem;
	nodelist = ALLOC(DdNodePtr, slots);
	MMoutOfMemory = saveHandler;
	if (nodelist == NULL) {
	    (void) fprintf(unique->err,
			   "Unable to resize subtable %d for lack of memory\n",
			   i);
	    /* Prevent frequent resizing attempts. */
	    (void) cuddGarbageCollect(unique,1);
	    if (unique->stash != NULL) {
		FREE(unique->stash);
		unique->stash = NULL;
		/* Inhibit resizing of tables. */
		cuddSlowTableGrowth(unique);
	    }
	    return;
	}
	unique->subtables[i].nodelist = nodelist;
	unique->subtables[i].slots = slots;
	unique->subtables[i].shift = shift;
	unique->subtables[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;

	/* Move the nodes from the old table to the new table.
	** This code depends on the type of hash function.
	** It assumes that the effect of doubling the size of the table
	** is to retain one more bit of the 32-bit hash value.
	** The additional bit is the LSB. */
	for (j = 0; (unsigned) j < oldslots; j++) {
	    DdNodePtr *evenP, *oddP;
	    node = oldnodelist[j];
	    evenP = &(nodelist[j<<1]);
	    oddP = &(nodelist[(j<<1)+1]);
	    while (node != sentinel) {
		next = node->next;
		pos = ddHash(cuddT(node), cuddE(node), shift);
		if (pos & 1) {
		    *oddP = node;
		    oddP = &(node->next);
		} else {
		    *evenP = node;
		    evenP = &(node->next);
		}
		node = next;
	    }
	    *evenP = *oddP = sentinel;
	}
	FREE(oldnodelist);

#ifdef DD_VERBOSE
	(void) fprintf(unique->err,
		       "rehashing layer %d: keys %d dead %d new size %d\n",
		       i, unique->subtables[i].keys,
		       unique->subtables[i].dead, slots);
#endif
    } else {
	oldslots = unique->constants.slots;
	oldshift = unique->constants.shift;
	oldnodelist = unique->constants.nodelist;

	/* The constant subtable is never subjected to reordering.
	** Therefore, when it is resized, it is because it has just
	** reached the maximum load. We can safely just double the size,
	** with no need for the loop we use for the other tables.
	*/
	slots = oldslots << 1;
	shift = oldshift - 1;
	saveHandler = MMoutOfMemory;
	MMoutOfMemory = Cudd_OutOfMem;
	nodelist = ALLOC(DdNodePtr, slots);
	MMoutOfMemory = saveHandler;
	if (nodelist == NULL) {
	    (void) fprintf(unique->err,
			   "Unable to resize constant subtable for lack of memory\n");
	    (void) cuddGarbageCollect(unique,1);
	    for (j = 0; j < unique->size; j++) {
		unique->subtables[j].maxKeys <<= 1;
	    }
	    unique->constants.maxKeys <<= 1;
	    return;
	}
	unique->constants.slots = slots;
	unique->constants.shift = shift;
	unique->constants.maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
	unique->constants.nodelist = nodelist;
	for (j = 0; (unsigned) j < slots; j++) {
	    nodelist[j] = NULL;
	}
	for (j = 0; (unsigned) j < oldslots; j++) {
	    node = oldnodelist[j];
	    while (node != NULL) {
		next = node->next;
		split.value = cuddV(node);
		pos = ddHash(split.bits[0], split.bits[1], shift);
		node->next = nodelist[pos];
		nodelist[pos] = node;
		node = next;
	    }
	}
	FREE(oldnodelist);

#ifdef DD_VERBOSE
	(void) fprintf(unique->err,
		       "rehashing constants: keys %d dead %d new size %d\n",
		       unique->constants.keys,unique->constants.dead,slots);
#endif
    }

    /* Update global data */

    unique->memused += (slots - oldslots) * sizeof(DdNodePtr);
    unique->slots += (slots - oldslots);
    ddFixLimits(unique);

} /* end of cuddRehash */


/**Function********************************************************************

  Synopsis    [Shrinks a subtable.]

  Description [Shrinks a subtable.]

  SideEffects [None]

  SeeAlso     [cuddRehash]

******************************************************************************/
void
cuddShrinkSubtable(
  DdManager *unique,
  int i)
{
    int j;
    int shift, posn;
    DdNodePtr *nodelist, *oldnodelist;
    DdNode *node, *next;
    DdNode *sentinel = &(unique->sentinel);
    unsigned int slots, oldslots;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    oldnodelist = unique->subtables[i].nodelist;
    oldslots = unique->subtables[i].slots;
    slots = oldslots >> 1;
    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    nodelist = ALLOC(DdNodePtr, slots);
    MMoutOfMemory = saveHandler;
    if (nodelist == NULL) {
	return;
    }
    unique->subtables[i].nodelist = nodelist;
    unique->subtables[i].slots = slots;
    unique->subtables[i].shift++;
    unique->subtables[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
#ifdef DD_VERBOSE
    (void) fprintf(unique->err,
		   "shrunk layer %d (%d keys) from %d to %d slots\n",
		   i, unique->subtables[i].keys, oldslots, slots);
#endif

    for (j = 0; (unsigned) j < slots; j++) {
	nodelist[j] = sentinel;
    }
    shift = unique->subtables[i].shift;
    for (j = 0; (unsigned) j < oldslots; j++) {
	node = oldnodelist[j];
	while (node != sentinel) {
	    DdNode *looking, *T, *E;
	    DdNodePtr *previousP;
	    next = node->next;
	    posn = ddHash(cuddT(node), cuddE(node), shift);
	    previousP = &(nodelist[posn]);
	    looking = *previousP;
	    T = cuddT(node);
	    E = cuddE(node);
	    while (T < cuddT(looking)) {
		previousP = &(looking->next);
		looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
		unique->uniqueLinks++;
#endif
	    }
	    while (T == cuddT(looking) && E < cuddE(looking)) {
		previousP = &(looking->next);
		looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
		unique->uniqueLinks++;
#endif
	    }
	    node->next = *previousP;
	    *previousP = node;
	    node = next;
	}
    }
    FREE(oldnodelist);

    unique->memused += ((long) slots - (long) oldslots) * sizeof(DdNode *);
    unique->slots += slots - oldslots;
    unique->minDead = (unsigned) (unique->gcFrac * (double) unique->slots);
    unique->cacheSlack = (int)
	ddMin(unique->maxCacheHard,DD_MAX_CACHE_TO_SLOTS_RATIO * unique->slots)
	- 2 * (int) unique->cacheSlots;

} /* end of cuddShrinkSubtable */


/**Function********************************************************************

  Synopsis [Inserts n new subtables in a unique table at level.]

  Description [Inserts n new subtables in a unique table at level.
  The number n should be positive, and level should be an existing level.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddDestroySubtables]

******************************************************************************/
int
cuddInsertSubtables(
  DdManager * unique,
  int  n,
  int  level)
{
    DdSubtable *newsubtables;
    DdNodePtr *newnodelist;
    DdNodePtr *newvars;
    DdNode *sentinel = &(unique->sentinel);
    int oldsize,newsize;
    int i,j,index,reorderSave;
    unsigned int numSlots = unique->initSlots;
    int *newperm, *newinvperm, *newmap;
    DdNode *one, *zero;

#ifdef DD_DEBUG
    assert(n > 0 && level < unique->size);
#endif

    oldsize = unique->size;
    /* Easy case: there is still room in the current table. */
    if (oldsize + n <= unique->maxSize) {
	/* Shift the tables at and below level. */
	for (i = oldsize - 1; i >= level; i--) {
	    unique->subtables[i+n].slots    = unique->subtables[i].slots;
	    unique->subtables[i+n].shift    = unique->subtables[i].shift;
	    unique->subtables[i+n].keys     = unique->subtables[i].keys;
	    unique->subtables[i+n].maxKeys  = unique->subtables[i].maxKeys;
	    unique->subtables[i+n].dead     = unique->subtables[i].dead;
	    unique->subtables[i+n].nodelist = unique->subtables[i].nodelist;
	    unique->subtables[i+n].bindVar  = unique->subtables[i].bindVar;
	    unique->subtables[i+n].varType  = unique->subtables[i].varType;
	    unique->subtables[i+n].pairIndex  = unique->subtables[i].pairIndex;
	    unique->subtables[i+n].varHandled = unique->subtables[i].varHandled;
	    unique->subtables[i+n].varToBeGrouped =
		unique->subtables[i].varToBeGrouped;

	    index                           = unique->invperm[i];
	    unique->invperm[i+n]            = index;
	    unique->perm[index]            += n;
	}
	/* Create new subtables. */
	for (i = 0; i < n; i++) {
	    unique->subtables[level+i].slots = numSlots;
	    unique->subtables[level+i].shift = sizeof(int) * 8 -
		cuddComputeFloorLog2(numSlots);
	    unique->subtables[level+i].keys = 0;
	    unique->subtables[level+i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
	    unique->subtables[level+i].dead = 0;
	    unique->subtables[level+i].bindVar = 0;
	    unique->subtables[level+i].varType = CUDD_VAR_PRIMARY_INPUT;
	    unique->subtables[level+i].pairIndex = 0;
	    unique->subtables[level+i].varHandled = 0;
	    unique->subtables[level+i].varToBeGrouped = CUDD_LAZY_NONE;

	    unique->perm[oldsize+i] = level + i;
	    unique->invperm[level+i] = oldsize + i;
	    newnodelist = unique->subtables[level+i].nodelist =
		ALLOC(DdNodePtr, numSlots);
	    if (newnodelist == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    for (j = 0; (unsigned) j < numSlots; j++) {
		newnodelist[j] = sentinel;
	    }
	}
	if (unique->map != NULL) {
	    for (i = 0; i < n; i++) {
		unique->map[oldsize+i] = oldsize + i;
	    }
	}
    } else {
	/* The current table is too small: we need to allocate a new,
	** larger one; move all old subtables, and initialize the new
	** subtables.
	*/
	newsize = oldsize + n + DD_DEFAULT_RESIZE;
#ifdef DD_VERBOSE
	(void) fprintf(unique->err,
		       "Increasing the table size from %d to %d\n",
	    unique->maxSize, newsize);
#endif
	/* Allocate memory for new arrays (except nodelists). */
	newsubtables = ALLOC(DdSubtable,newsize);
	if (newsubtables == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	newvars = ALLOC(DdNodePtr,newsize);
	if (newvars == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    FREE(newsubtables);
	    return(0);
	}
	newperm = ALLOC(int,newsize);
	if (newperm == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    FREE(newsubtables);
	    FREE(newvars);
	    return(0);
	}
	newinvperm = ALLOC(int,newsize);
	if (newinvperm == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    FREE(newsubtables);
	    FREE(newvars);
	    FREE(newperm);
	    return(0);
	}
	if (unique->map != NULL) {
	    newmap = ALLOC(int,newsize);
	    if (newmap == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		FREE(newsubtables);
		FREE(newvars);
		FREE(newperm);
		FREE(newinvperm);
		return(0);
	    }
	    unique->memused += (newsize - unique->maxSize) * sizeof(int);
	}
	unique->memused += (newsize - unique->maxSize) * ((numSlots+1) *
	    sizeof(DdNode *) + 2 * sizeof(int) + sizeof(DdSubtable));
	/* Copy levels before insertion points from old tables. */
	for (i = 0; i < level; i++) {
	    newsubtables[i].slots = unique->subtables[i].slots;
	    newsubtables[i].shift = unique->subtables[i].shift;
	    newsubtables[i].keys = unique->subtables[i].keys;
	    newsubtables[i].maxKeys = unique->subtables[i].maxKeys;
	    newsubtables[i].dead = unique->subtables[i].dead;
	    newsubtables[i].nodelist = unique->subtables[i].nodelist;
	    newsubtables[i].bindVar = unique->subtables[i].bindVar;
	    newsubtables[i].varType = unique->subtables[i].varType;
	    newsubtables[i].pairIndex = unique->subtables[i].pairIndex;
	    newsubtables[i].varHandled = unique->subtables[i].varHandled;
	    newsubtables[i].varToBeGrouped = unique->subtables[i].varToBeGrouped;

	    newvars[i] = unique->vars[i];
	    newperm[i] = unique->perm[i];
	    newinvperm[i] = unique->invperm[i];
	}
	/* Finish initializing permutation for new table to old one. */
	for (i = level; i < oldsize; i++) {
	    newperm[i] = unique->perm[i];
	}
	/* Initialize new levels. */
	for (i = level; i < level + n; i++) {
	    newsubtables[i].slots = numSlots;
	    newsubtables[i].shift = sizeof(int) * 8 -
		cuddComputeFloorLog2(numSlots);
	    newsubtables[i].keys = 0;
	    newsubtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
	    newsubtables[i].dead = 0;
	    newsubtables[i].bindVar = 0;
	    newsubtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
	    newsubtables[i].pairIndex = 0;
	    newsubtables[i].varHandled = 0;
	    newsubtables[i].varToBeGrouped = CUDD_LAZY_NONE;

	    newperm[oldsize + i - level] = i;
	    newinvperm[i] = oldsize + i - level;
	    newnodelist = newsubtables[i].nodelist = ALLOC(DdNodePtr, numSlots);
	    if (newnodelist == NULL) {
		/* We are going to leak some memory.  We should clean up. */
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    for (j = 0; (unsigned) j < numSlots; j++) {
		newnodelist[j] = sentinel;
	    }
	}
	/* Copy the old tables for levels past the insertion point. */
	for (i = level; i < oldsize; i++) {
	    newsubtables[i+n].slots    = unique->subtables[i].slots;
	    newsubtables[i+n].shift    = unique->subtables[i].shift;
	    newsubtables[i+n].keys     = unique->subtables[i].keys;
	    newsubtables[i+n].maxKeys  = unique->subtables[i].maxKeys;
	    newsubtables[i+n].dead     = unique->subtables[i].dead;
	    newsubtables[i+n].nodelist = unique->subtables[i].nodelist;
	    newsubtables[i+n].bindVar  = unique->subtables[i].bindVar;
	    newsubtables[i+n].varType  = unique->subtables[i].varType;
	    newsubtables[i+n].pairIndex  = unique->subtables[i].pairIndex;
	    newsubtables[i+n].varHandled  = unique->subtables[i].varHandled;
	    newsubtables[i+n].varToBeGrouped  =
		unique->subtables[i].varToBeGrouped;

	    newvars[i]                 = unique->vars[i];
	    index                      = unique->invperm[i];
	    newinvperm[i+n]            = index;
	    newperm[index]            += n;
	}
	/* Update the map. */
	if (unique->map != NULL) {
	    for (i = 0; i < oldsize; i++) {
		newmap[i] = unique->map[i];
	    }
	    for (i = oldsize; i < oldsize + n; i++) {
		newmap[i] = i;
	    }
	    FREE(unique->map);
	    unique->map = newmap;
	}
	/* Install the new tables and free the old ones. */
	FREE(unique->subtables);
	unique->subtables = newsubtables;
	unique->maxSize = newsize;
	FREE(unique->vars);
	unique->vars = newvars;
	FREE(unique->perm);
	unique->perm = newperm;
	FREE(unique->invperm);
	unique->invperm = newinvperm;
	/* Update the stack for iterative procedures. */
	if (newsize > unique->maxSizeZ) {
	    FREE(unique->stack);
	    unique->stack = ALLOC(DdNodePtr,newsize + 1);
	    if (unique->stack == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    unique->stack[0] = NULL; /* to suppress harmless UMR */
	    unique->memused +=
		(newsize - ddMax(unique->maxSize,unique->maxSizeZ))
		* sizeof(DdNode *);
	}
    }
    /* Update manager parameters to account for the new subtables. */
    unique->slots += n * numSlots;
    ddFixLimits(unique);
    unique->size += n;

    /* Now that the table is in a coherent state, create the new
    ** projection functions. We need to temporarily disable reordering,
    ** because we cannot reorder without projection functions in place.
    **/
    one = unique->one;
    zero = Cudd_Not(one);

    reorderSave = unique->autoDyn;
    unique->autoDyn = 0;
    for (i = oldsize; i < oldsize + n; i++) {
	unique->vars[i] = cuddUniqueInter(unique,i,one,zero);
	if (unique->vars[i] == NULL) {
	    unique->autoDyn = reorderSave;
	    /* Shift everything back so table remains coherent. */
	    for (j = oldsize; j < i; j++) {
		Cudd_IterDerefBdd(unique,unique->vars[j]);
		cuddDeallocNode(unique,unique->vars[j]);
		unique->vars[j] = NULL;
	    }
	    for (j = level; j < oldsize; j++) {
		unique->subtables[j].slots    = unique->subtables[j+n].slots;
		unique->subtables[j].slots    = unique->subtables[j+n].slots;
		unique->subtables[j].shift    = unique->subtables[j+n].shift;
		unique->subtables[j].keys     = unique->subtables[j+n].keys;
		unique->subtables[j].maxKeys  =
		    unique->subtables[j+n].maxKeys;
		unique->subtables[j].dead     = unique->subtables[j+n].dead;
		FREE(unique->subtables[j].nodelist);
		unique->subtables[j].nodelist =
		    unique->subtables[j+n].nodelist;
		unique->subtables[j+n].nodelist = NULL;
		unique->subtables[j].bindVar  =
		    unique->subtables[j+n].bindVar;
		unique->subtables[j].varType  =
		    unique->subtables[j+n].varType;
		unique->subtables[j].pairIndex =
		    unique->subtables[j+n].pairIndex;
		unique->subtables[j].varHandled =
		    unique->subtables[j+n].varHandled;
		unique->subtables[j].varToBeGrouped =
		    unique->subtables[j+n].varToBeGrouped;
		index                         = unique->invperm[j+n];
		unique->invperm[j]            = index;
		unique->perm[index]          -= n;
	    }
	    unique->size = oldsize;
	    unique->slots -= n * numSlots;
	    ddFixLimits(unique);
	    (void) Cudd_DebugCheck(unique);
	    return(0);
	}
	cuddRef(unique->vars[i]);
    }
    if (unique->tree != NULL) {
	unique->tree->size += n;
	unique->tree->index = unique->invperm[0];
	ddPatchTree(unique,unique->tree);
    }
    unique->autoDyn = reorderSave;

    return(1);

} /* end of cuddInsertSubtables */


/**Function********************************************************************

  Synopsis [Destroys the n most recently created subtables in a unique table.]

  Description [Destroys the n most recently created subtables in a unique
  table.  n should be positive. The subtables should not contain any live
  nodes, except the (isolated) projection function. The projection
  functions are freed.  Returns 1 if successful; 0 otherwise.]

  SideEffects [The variable map used for fast variable substitution is
  destroyed if it exists. In this case the cache is also cleared.]

  SeeAlso     [cuddInsertSubtables Cudd_SetVarMap]

******************************************************************************/
int
cuddDestroySubtables(
  DdManager * unique,
  int  n)
{
    DdSubtable *subtables;
    DdNodePtr *nodelist;
    DdNodePtr *vars;
    int firstIndex, lastIndex;
    int index, level, newlevel;
    int lowestLevel;
    int shift;
    int found;

    /* Sanity check and set up. */
    if (n <= 0) return(0);
    if (n > unique->size) n = unique->size;

    subtables = unique->subtables;
    vars = unique->vars;
    firstIndex = unique->size - n;
    lastIndex  = unique->size;

    /* Check for nodes labeled by the variables being destroyed
    ** that may still be in use.  It is allowed to destroy a variable
    ** only if there are no such nodes. Also, find the lowest level
    ** among the variables being destroyed. This will make further
    ** processing more efficient.
    */
    lowestLevel = unique->size;
    for (index = firstIndex; index < lastIndex; index++) {
	level = unique->perm[index];
	if (level < lowestLevel) lowestLevel = level;
	nodelist = subtables[level].nodelist;
	if (subtables[level].keys - subtables[level].dead != 1) return(0);
	/* The projection function should be isolated. If the ref count
	** is 1, everything is OK. If the ref count is saturated, then
	** we need to make sure that there are no nodes pointing to it.
	** As for the external references, we assume the application is
	** responsible for them.
	*/
	if (vars[index]->ref != 1) {
	    if (vars[index]->ref != DD_MAXREF) return(0);
	    found = cuddFindParent(unique,vars[index]);
	    if (found) {
		return(0);
	    } else {
		vars[index]->ref = 1;
	    }
	}
	Cudd_RecursiveDeref(unique,vars[index]);
    }

    /* Collect garbage, because we cannot afford having dead nodes pointing
    ** to the dead nodes in the subtables being destroyed.
    */
    (void) cuddGarbageCollect(unique,1);

    /* Here we know we can destroy our subtables. */
    for (index = firstIndex; index < lastIndex; index++) {
	level = unique->perm[index];
	nodelist = subtables[level].nodelist;
#ifdef DD_DEBUG
	assert(subtables[level].keys == 0);
#endif
	FREE(nodelist);
	unique->memused -= sizeof(DdNodePtr) * subtables[level].slots;
	unique->slots -= subtables[level].slots;
	unique->dead -= subtables[level].dead;
    }

    /* Here all subtables to be destroyed have their keys field == 0 and
    ** their hash tables have been freed.
    ** We now scan the subtables from level lowestLevel + 1 to level size - 1,
    ** shifting the subtables as required. We keep a running count of
    ** how many subtables have been moved, so that we know by how many
    ** positions each subtable should be shifted.
    */
    shift = 1;
    for (level = lowestLevel + 1; level < unique->size; level++) {
	if (subtables[level].keys == 0) {
	    shift++;
	    continue;
	}
	newlevel = level - shift;
	subtables[newlevel].slots = subtables[level].slots;
	subtables[newlevel].shift = subtables[level].shift;
	subtables[newlevel].keys = subtables[level].keys;
	subtables[newlevel].maxKeys = subtables[level].maxKeys;
	subtables[newlevel].dead = subtables[level].dead;
	subtables[newlevel].nodelist = subtables[level].nodelist;
	index = unique->invperm[level];
	unique->perm[index] = newlevel;
	unique->invperm[newlevel]  = index;
	subtables[newlevel].bindVar = subtables[level].bindVar;
	subtables[newlevel].varType = subtables[level].varType;
	subtables[newlevel].pairIndex = subtables[level].pairIndex;
	subtables[newlevel].varHandled = subtables[level].varHandled;
	subtables[newlevel].varToBeGrouped = subtables[level].varToBeGrouped;
    }
    /* Destroy the map. If a surviving variable is
    ** mapped to a dying variable, and the map were used again,
    ** an out-of-bounds access to unique->vars would result. */
    if (unique->map != NULL) {
	cuddCacheFlush(unique);
	FREE(unique->map);
	unique->map = NULL;
    }

    unique->minDead = (unsigned) (unique->gcFrac * (double) unique->slots);
    unique->size -= n;

    return(1);

} /* end of cuddDestroySubtables */


/**Function********************************************************************

  Synopsis [Increases the number of ZDD subtables in a unique table so
  that it meets or exceeds index.]

  Description [Increases the number of ZDD subtables in a unique table so
  that it meets or exceeds index.  When new ZDD variables are created, it
  is possible to preserve the functions unchanged, or it is possible to
  preserve the covers unchanged, but not both. cuddResizeTableZdd preserves
  the covers.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [ddResizeTable]

******************************************************************************/
int
cuddResizeTableZdd(
  DdManager * unique,
  int  index)
{
    DdSubtable *newsubtables;
    DdNodePtr *newnodelist;
    int oldsize,newsize;
    int i,j,reorderSave;
    unsigned int numSlots = unique->initSlots;
    int *newperm, *newinvperm;

    oldsize = unique->sizeZ;
    /* Easy case: there is still room in the current table. */
    if (index < unique->maxSizeZ) {
	for (i = oldsize; i <= index; i++) {
	    unique->subtableZ[i].slots = numSlots;
	    unique->subtableZ[i].shift = sizeof(int) * 8 -
		cuddComputeFloorLog2(numSlots);
	    unique->subtableZ[i].keys = 0;
	    unique->subtableZ[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
	    unique->subtableZ[i].dead = 0;
	    unique->permZ[i] = i;
	    unique->invpermZ[i] = i;
	    newnodelist = unique->subtableZ[i].nodelist =
		ALLOC(DdNodePtr, numSlots);
	    if (newnodelist == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    for (j = 0; (unsigned) j < numSlots; j++) {
		newnodelist[j] = NULL;
	    }
	}
    } else {
	/* The current table is too small: we need to allocate a new,
	** larger one; move all old subtables, and initialize the new
	** subtables up to index included.
	*/
	newsize = index + DD_DEFAULT_RESIZE;
#ifdef DD_VERBOSE
	(void) fprintf(unique->err,
		       "Increasing the ZDD table size from %d to %d\n",
	    unique->maxSizeZ, newsize);
#endif
	newsubtables = ALLOC(DdSubtable,newsize);
	if (newsubtables == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	newperm = ALLOC(int,newsize);
	if (newperm == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	newinvperm = ALLOC(int,newsize);
	if (newinvperm == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	unique->memused += (newsize - unique->maxSizeZ) * ((numSlots+1) *
	    sizeof(DdNode *) + 2 * sizeof(int) + sizeof(DdSubtable));
	if (newsize > unique->maxSize) {
	    FREE(unique->stack);
	    unique->stack = ALLOC(DdNodePtr,newsize + 1);
	    if (unique->stack == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    unique->stack[0] = NULL; /* to suppress harmless UMR */
	    unique->memused +=
		(newsize - ddMax(unique->maxSize,unique->maxSizeZ))
		* sizeof(DdNode *);
	}
	for (i = 0; i < oldsize; i++) {
	    newsubtables[i].slots = unique->subtableZ[i].slots;
	    newsubtables[i].shift = unique->subtableZ[i].shift;
	    newsubtables[i].keys = unique->subtableZ[i].keys;
	    newsubtables[i].maxKeys = unique->subtableZ[i].maxKeys;
	    newsubtables[i].dead = unique->subtableZ[i].dead;
	    newsubtables[i].nodelist = unique->subtableZ[i].nodelist;
	    newperm[i] = unique->permZ[i];
	    newinvperm[i] = unique->invpermZ[i];
	}
	for (i = oldsize; i <= index; i++) {
	    newsubtables[i].slots = numSlots;
	    newsubtables[i].shift = sizeof(int) * 8 -
		cuddComputeFloorLog2(numSlots);
	    newsubtables[i].keys = 0;
	    newsubtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
	    newsubtables[i].dead = 0;
	    newperm[i] = i;
	    newinvperm[i] = i;
	    newnodelist = newsubtables[i].nodelist = ALLOC(DdNodePtr, numSlots);
	    if (newnodelist == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    for (j = 0; (unsigned) j < numSlots; j++) {
		newnodelist[j] = NULL;
	    }
	}
	FREE(unique->subtableZ);
	unique->subtableZ = newsubtables;
	unique->maxSizeZ = newsize;
	FREE(unique->permZ);
	unique->permZ = newperm;
	FREE(unique->invpermZ);
	unique->invpermZ = newinvperm;
    }
    unique->slots += (index + 1 - unique->sizeZ) * numSlots;
    ddFixLimits(unique);
    unique->sizeZ = index + 1;

    /* Now that the table is in a coherent state, update the ZDD
    ** universe. We need to temporarily disable reordering,
    ** because we cannot reorder without universe in place.
    */

    reorderSave = unique->autoDynZ;
    unique->autoDynZ = 0;
    cuddZddFreeUniv(unique);
    if (!cuddZddInitUniv(unique)) {
	unique->autoDynZ = reorderSave;
	return(0);
    }
    unique->autoDynZ = reorderSave;

    return(1);

} /* end of cuddResizeTableZdd */


/**Function********************************************************************

  Synopsis    [Adjusts parameters of a table to slow down its growth.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddSlowTableGrowth(
  DdManager *unique)
{
    int i;

    unique->maxCacheHard = unique->cacheSlots - 1;
    unique->cacheSlack = - (int) (unique->cacheSlots + 1);
    for (i = 0; i < unique->size; i++) {
	unique->subtables[i].maxKeys <<= 2;
    }
    unique->gcFrac = DD_GC_FRAC_MIN;
    unique->minDead = (unsigned) (DD_GC_FRAC_MIN * (double) unique->slots);
    cuddShrinkDeathRow(unique);
    (void) fprintf(unique->err,"Slowing down table growth: ");
    (void) fprintf(unique->err,"GC fraction = %.2f\t", unique->gcFrac);
    (void) fprintf(unique->err,"minDead = %u\n", unique->minDead);

} /* end of cuddSlowTableGrowth */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Rehashes a ZDD unique subtable.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddRehash]

******************************************************************************/
static void
ddRehashZdd(
  DdManager * unique,
  int  i)
{
    unsigned int slots, oldslots;
    int shift, oldshift;
    int j, pos;
    DdNodePtr *nodelist, *oldnodelist;
    DdNode *node, *next;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    if (unique->slots > unique->looseUpTo) {
	unique->minDead = (unsigned) (DD_GC_FRAC_LO * (double) unique->slots);
#ifdef DD_VERBOSE
	if (unique->gcFrac == DD_GC_FRAC_HI) {
	    (void) fprintf(unique->err,"GC fraction = %.2f\t",
			   DD_GC_FRAC_LO);
	    (void) fprintf(unique->err,"minDead = %d\n", unique->minDead);
	}
#endif
	unique->gcFrac = DD_GC_FRAC_LO;
    }

    assert(i != CUDD_MAXINDEX);
    oldslots = unique->subtableZ[i].slots;
    oldshift = unique->subtableZ[i].shift;
    oldnodelist = unique->subtableZ[i].nodelist;

    /* Compute the new size of the subtable. Normally, we just
    ** double.  However, after reordering, a table may be severely
    ** overloaded. Therefore, we iterate. */
    slots = oldslots;
    shift = oldshift;
    do {
	slots <<= 1;
	shift--;
    } while (slots * DD_MAX_SUBTABLE_DENSITY < unique->subtableZ[i].keys);

    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    nodelist = ALLOC(DdNodePtr, slots);
    MMoutOfMemory = saveHandler;
    if (nodelist == NULL) {
	(void) fprintf(unique->err,
		       "Unable to resize ZDD subtable %d for lack of memory.\n",
		       i);
	(void) cuddGarbageCollect(unique,1);
	for (j = 0; j < unique->sizeZ; j++) {
	    unique->subtableZ[j].maxKeys <<= 1;
	}
	return;
    }
    unique->subtableZ[i].nodelist = nodelist;
    unique->subtableZ[i].slots = slots;
    unique->subtableZ[i].shift = shift;
    unique->subtableZ[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
    for (j = 0; (unsigned) j < slots; j++) {
	nodelist[j] = NULL;
    }
    for (j = 0; (unsigned) j < oldslots; j++) {
	node = oldnodelist[j];
	while (node != NULL) {
	    next = node->next;
	    pos = ddHash(cuddT(node), cuddE(node), shift);
	    node->next = nodelist[pos];
	    nodelist[pos] = node;
	    node = next;
	}
    }
    FREE(oldnodelist);

#ifdef DD_VERBOSE
    (void) fprintf(unique->err,
		   "rehashing layer %d: keys %d dead %d new size %d\n",
		   i, unique->subtableZ[i].keys,
		   unique->subtableZ[i].dead, slots);
#endif

    /* Update global data. */
    unique->memused += (slots - oldslots) * sizeof(DdNode *);
    unique->slots += (slots - oldslots);
    ddFixLimits(unique);

} /* end of ddRehashZdd */


/**Function********************************************************************

  Synopsis [Increases the number of subtables in a unique table so
  that it meets or exceeds index.]

  Description [Increases the number of subtables in a unique table so
  that it meets or exceeds index.  The parameter amount determines how
  much spare space is allocated to prevent too frequent resizing.  If
  index is negative, the table is resized, but no new variables are
  created.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Reserve cuddResizeTableZdd]

******************************************************************************/
static int
ddResizeTable(
  DdManager * unique,
  int index,
  int amount)
{
    DdSubtable *newsubtables;
    DdNodePtr *newnodelist;
    DdNodePtr *newvars;
    DdNode *sentinel = &(unique->sentinel);
    int oldsize,newsize;
    int i,j,reorderSave;
    int numSlots = unique->initSlots;
    int *newperm, *newinvperm, *newmap;
    DdNode *one, *zero;

    oldsize = unique->size;
    /* Easy case: there is still room in the current table. */
    if (index >= 0 && index < unique->maxSize) {
	for (i = oldsize; i <= index; i++) {
	    unique->subtables[i].slots = numSlots;
	    unique->subtables[i].shift = sizeof(int) * 8 -
		cuddComputeFloorLog2(numSlots);
	    unique->subtables[i].keys = 0;
	    unique->subtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
	    unique->subtables[i].dead = 0;
	    unique->subtables[i].bindVar = 0;
	    unique->subtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
	    unique->subtables[i].pairIndex = 0;
	    unique->subtables[i].varHandled = 0;
	    unique->subtables[i].varToBeGrouped = CUDD_LAZY_NONE;

	    unique->perm[i] = i;
	    unique->invperm[i] = i;
	    newnodelist = unique->subtables[i].nodelist =
		ALLOC(DdNodePtr, numSlots);
	    if (newnodelist == NULL) {
		for (j = oldsize; j < i; j++) {
		    FREE(unique->subtables[j].nodelist);
		}
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    for (j = 0; j < numSlots; j++) {
		newnodelist[j] = sentinel;
	    }
	}
	if (unique->map != NULL) {
	    for (i = oldsize; i <= index; i++) {
		unique->map[i] = i;
	    }
	}
    } else {
	/* The current table is too small: we need to allocate a new,
	** larger one; move all old subtables, and initialize the new
	** subtables up to index included.
	*/
	newsize = (index < 0) ? amount : index + amount;
#ifdef DD_VERBOSE
	(void) fprintf(unique->err,
		       "Increasing the table size from %d to %d\n",
		       unique->maxSize, newsize);
#endif
	newsubtables = ALLOC(DdSubtable,newsize);
	if (newsubtables == NULL) {
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	newvars = ALLOC(DdNodePtr,newsize);
	if (newvars == NULL) {
	    FREE(newsubtables);
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	newperm = ALLOC(int,newsize);
	if (newperm == NULL) {
	    FREE(newsubtables);
	    FREE(newvars);
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	newinvperm = ALLOC(int,newsize);
	if (newinvperm == NULL) {
	    FREE(newsubtables);
	    FREE(newvars);
	    FREE(newperm);
	    unique->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	if (unique->map != NULL) {
	    newmap = ALLOC(int,newsize);
	    if (newmap == NULL) {
		FREE(newsubtables);
		FREE(newvars);
		FREE(newperm);
		FREE(newinvperm);
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    unique->memused += (newsize - unique->maxSize) * sizeof(int);
	}
	unique->memused += (newsize - unique->maxSize) * ((numSlots+1) *
	    sizeof(DdNode *) + 2 * sizeof(int) + sizeof(DdSubtable));
	if (newsize > unique->maxSizeZ) {
	    FREE(unique->stack);
	    unique->stack = ALLOC(DdNodePtr,newsize + 1);
	    if (unique->stack == NULL) {
		FREE(newsubtables);
		FREE(newvars);
		FREE(newperm);
		FREE(newinvperm);
		if (unique->map != NULL) {
		    FREE(newmap);
		}
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    unique->stack[0] = NULL; /* to suppress harmless UMR */
	    unique->memused +=
		(newsize - ddMax(unique->maxSize,unique->maxSizeZ))
		* sizeof(DdNode *);
	}
	for (i = 0; i < oldsize; i++) {
	    newsubtables[i].slots = unique->subtables[i].slots;
	    newsubtables[i].shift = unique->subtables[i].shift;
	    newsubtables[i].keys = unique->subtables[i].keys;
	    newsubtables[i].maxKeys = unique->subtables[i].maxKeys;
	    newsubtables[i].dead = unique->subtables[i].dead;
	    newsubtables[i].nodelist = unique->subtables[i].nodelist;
	    newsubtables[i].bindVar = unique->subtables[i].bindVar;
	    newsubtables[i].varType = unique->subtables[i].varType;
	    newsubtables[i].pairIndex = unique->subtables[i].pairIndex;
	    newsubtables[i].varHandled = unique->subtables[i].varHandled;
	    newsubtables[i].varToBeGrouped = unique->subtables[i].varToBeGrouped;

	    newvars[i] = unique->vars[i];
	    newperm[i] = unique->perm[i];
	    newinvperm[i] = unique->invperm[i];
	}
	for (i = oldsize; i <= index; i++) {
	    newsubtables[i].slots = numSlots;
	    newsubtables[i].shift = sizeof(int) * 8 -
		cuddComputeFloorLog2(numSlots);
	    newsubtables[i].keys = 0;
	    newsubtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
	    newsubtables[i].dead = 0;
	    newsubtables[i].bindVar = 0;
	    newsubtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
	    newsubtables[i].pairIndex = 0;
	    newsubtables[i].varHandled = 0;
	    newsubtables[i].varToBeGrouped = CUDD_LAZY_NONE;

	    newperm[i] = i;
	    newinvperm[i] = i;
	    newnodelist = newsubtables[i].nodelist = ALLOC(DdNodePtr, numSlots);
	    if (newnodelist == NULL) {
		unique->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    for (j = 0; j < numSlots; j++) {
		newnodelist[j] = sentinel;
	    }
	}
	if (unique->map != NULL) {
	    for (i = 0; i < oldsize; i++) {
		newmap[i] = unique->map[i];
	    }
	    for (i = oldsize; i <= index; i++) {
		newmap[i] = i;
	    }
	    FREE(unique->map);
	    unique->map = newmap;
	}
	FREE(unique->subtables);
	unique->subtables = newsubtables;
	unique->maxSize = newsize;
	FREE(unique->vars);
	unique->vars = newvars;
	FREE(unique->perm);
	unique->perm = newperm;
	FREE(unique->invperm);
	unique->invperm = newinvperm;
    }

    /* Now that the table is in a coherent state, create the new
    ** projection functions. We need to temporarily disable reordering,
    ** because we cannot reorder without projection functions in place.
    **/
    if (index >= 0) {
        one = unique->one;
        zero = Cudd_Not(one);

        unique->size = index + 1;
        if (unique->tree != NULL) {
            unique->tree->size = ddMax(unique->tree->size, unique->size);
        }
        unique->slots += (index + 1 - oldsize) * numSlots;
        ddFixLimits(unique);

        reorderSave = unique->autoDyn;
        unique->autoDyn = 0;
        for (i = oldsize; i <= index; i++) {
            unique->vars[i] = cuddUniqueInter(unique,i,one,zero);
            if (unique->vars[i] == NULL) {
                unique->autoDyn = reorderSave;
                for (j = oldsize; j < i; j++) {
                    Cudd_IterDerefBdd(unique,unique->vars[j]);
                    cuddDeallocNode(unique,unique->vars[j]);
                    unique->vars[j] = NULL;
                }
                for (j = oldsize; j <= index; j++) {
                    FREE(unique->subtables[j].nodelist);
                    unique->subtables[j].nodelist = NULL;
                }
                unique->size = oldsize;
                unique->slots -= (index + 1 - oldsize) * numSlots;
                ddFixLimits(unique);
                return(0);
            }
            cuddRef(unique->vars[i]);
        }
        unique->autoDyn = reorderSave;
    }

    return(1);

} /* end of ddResizeTable */


/**Function********************************************************************

  Synopsis    [Searches the subtables above node for a parent.]

  Description [Searches the subtables above node for a parent. Returns 1
  as soon as one parent is found. Returns 0 is the search is fruitless.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddFindParent(
  DdManager * table,
  DdNode * node)
{
    int         i,j;
    int		slots;
    DdNodePtr	*nodelist;
    DdNode	*f;

    for (i = cuddI(table,node->index) - 1; i >= 0; i--) {
	nodelist = table->subtables[i].nodelist;
	slots = table->subtables[i].slots;

	for (j = 0; j < slots; j++) {
	    f = nodelist[j];
	    while (cuddT(f) > node) {
		f = f->next;
	    }
	    while (cuddT(f) == node && Cudd_Regular(cuddE(f)) > node) {
		f = f->next;
	    }
	    if (cuddT(f) == node && Cudd_Regular(cuddE(f)) == node) {
		return(1);
	    }
	}
    }

    return(0);

} /* end of cuddFindParent */


/**Function********************************************************************

  Synopsis    [Adjusts the values of table limits.]

  Description [Adjusts the values of table fields controlling the.
  sizes of subtables and computed table. If the computed table is too small
  according to the new values, it is resized.]

  SideEffects [Modifies manager fields. May resize computed table.]

  SeeAlso     []

******************************************************************************/
DD_INLINE
static void
ddFixLimits(
  DdManager *unique)
{
    unique->minDead = (unsigned) (unique->gcFrac * (double) unique->slots);
    unique->cacheSlack = (int) ddMin(unique->maxCacheHard,
	DD_MAX_CACHE_TO_SLOTS_RATIO * unique->slots) -
	2 * (int) unique->cacheSlots;
    if (unique->cacheSlots < unique->slots/2 && unique->cacheSlack >= 0)
	cuddCacheResize(unique);
    return;

} /* end of ddFixLimits */


#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
/**Function********************************************************************

  Synopsis    [Inserts a DdNode in a red/black search tree.]

  Description [Inserts a DdNode in a red/black search tree. Nodes from
  the same "page" (defined by DD_PAGE_MASK) are linked in a LIFO list.]

  SideEffects [None]

  SeeAlso     [cuddOrderedThread]

******************************************************************************/
static void
cuddOrderedInsert(
  DdNodePtr * root,
  DdNodePtr node)
{
    DdNode *scan;
    DdNodePtr *scanP;
    DdNodePtr *stack[DD_STACK_SIZE];
    int stackN = 0;

    scanP = root;
    while ((scan = *scanP) != NULL) {
	stack[stackN++] = scanP;
	if (DD_INSERT_COMPARE(node, scan) == 0) { /* add to page list */
	    DD_NEXT(node) = DD_NEXT(scan);
	    DD_NEXT(scan) = node;
	    return;
	}
	scanP = (node < scan) ? &DD_LEFT(scan) : &DD_RIGHT(scan);
    }
    DD_RIGHT(node) = DD_LEFT(node) = DD_NEXT(node) = NULL;
    DD_COLOR(node) = DD_RED;
    *scanP = node;
    stack[stackN] = &node;
    cuddDoRebalance(stack,stackN);

} /* end of cuddOrderedInsert */


/**Function********************************************************************

  Synopsis    [Threads all the nodes of a search tree into a linear list.]

  Description [Threads all the nodes of a search tree into a linear
  list. For each node of the search tree, the "left" child, if non-null, has
  a lower address than its parent, and the "right" child, if non-null, has a
  higher address than its parent.
  The list is sorted in order of increasing addresses. The search
  tree is destroyed as a result of this operation. The last element of
  the linear list is made to point to the address passed in list. Each
  node if the search tree is a linearly-linked list of nodes from the
  same memory page (as defined in DD_PAGE_MASK). When a node is added to
  the linear list, all the elements of the linked list are added.]

  SideEffects [The search tree is destroyed as a result of this operation.]

  SeeAlso     [cuddOrderedInsert]

******************************************************************************/
static DdNode *
cuddOrderedThread(
  DdNode * root,
  DdNode * list)
{
    DdNode *current, *next, *prev, *end;

    current = root;
    /* The first word in the node is used to implement a stack that holds
    ** the nodes from the root of the tree to the current node. Here we
    ** put the root of the tree at the bottom of the stack.
    */
    *((DdNodePtr *) current) = NULL;

    while (current != NULL) {
	if (DD_RIGHT(current) != NULL) {
	    /* If possible, we follow the "right" link. Eventually we'll
	    ** find the node with the largest address in the current tree.
	    ** In this phase we use the first word of a node to implemen
	    ** a stack of the nodes on the path from the root to "current".
	    ** Also, we disconnect the "right" pointers to indicate that
	    ** we have already followed them.
	    */
	    next = DD_RIGHT(current);
	    DD_RIGHT(current) = NULL;
	    *((DdNodePtr *)next) = current;
	    current = next;
	} else {
	    /* We can't proceed along the "right" links any further.
	    ** Hence "current" is the largest element in the current tree.
	    ** We make this node the new head of "list". (Repeating this
	    ** operation until the tree is empty yields the desired linear
	    ** threading of all nodes.)
	    */
	    prev = *((DdNodePtr *) current); /* save prev node on stack in prev */
	    /* Traverse the linked list of current until the end. */
	    for (end = current; DD_NEXT(end) != NULL; end = DD_NEXT(end));
	    DD_NEXT(end) = list; /* attach "list" at end and make */
	    list = current;   /* "current" the new head of "list" */
	    /* Now, if current has a "left" child, we push it on the stack.
	    ** Otherwise, we just continue with the parent of "current".
	    */
	    if (DD_LEFT(current) != NULL) {
		next = DD_LEFT(current);
		*((DdNodePtr *) next) = prev;
		current = next;
	    } else {
		current = prev;
	    }
	}
    }

    return(list);

} /* end of cuddOrderedThread */


/**Function********************************************************************

  Synopsis    [Performs the left rotation for red/black trees.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddRotateRight]

******************************************************************************/
DD_INLINE
static void
cuddRotateLeft(
  DdNodePtr * nodeP)
{
    DdNode *newRoot;
    DdNode *oldRoot = *nodeP;

    *nodeP = newRoot = DD_RIGHT(oldRoot);
    DD_RIGHT(oldRoot) = DD_LEFT(newRoot);
    DD_LEFT(newRoot) = oldRoot;

} /* end of cuddRotateLeft */


/**Function********************************************************************

  Synopsis    [Performs the right rotation for red/black trees.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddRotateLeft]

******************************************************************************/
DD_INLINE
static void
cuddRotateRight(
  DdNodePtr * nodeP)
{
    DdNode *newRoot;
    DdNode *oldRoot = *nodeP;

    *nodeP = newRoot = DD_LEFT(oldRoot);
    DD_LEFT(oldRoot) = DD_RIGHT(newRoot);
    DD_RIGHT(newRoot) = oldRoot;

} /* end of cuddRotateRight */


/**Function********************************************************************

  Synopsis    [Rebalances a red/black tree.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
cuddDoRebalance(
  DdNodePtr ** stack,
  int  stackN)
{
    DdNodePtr *xP, *parentP, *grandpaP;
    DdNode *x, *y, *parent, *grandpa;

    xP = stack[stackN];
    x = *xP;
    /* Work our way back up, re-balancing the tree. */
    while (--stackN >= 0) {
	parentP = stack[stackN];
	parent = *parentP;
	if (DD_IS_BLACK(parent)) break;
	/* Since the root is black, here a non-null grandparent exists. */
	grandpaP = stack[stackN-1];
	grandpa = *grandpaP;
	if (parent == DD_LEFT(grandpa)) {
	    y = DD_RIGHT(grandpa);
	    if (y != NULL && DD_IS_RED(y)) {
		DD_COLOR(parent) = DD_BLACK;
		DD_COLOR(y) = DD_BLACK;
		DD_COLOR(grandpa) = DD_RED;
		x = grandpa;
		stackN--;
	    } else {
		if (x == DD_RIGHT(parent)) {
		    cuddRotateLeft(parentP);
		    DD_COLOR(x) = DD_BLACK;
		} else {
		    DD_COLOR(parent) = DD_BLACK;
		}
		DD_COLOR(grandpa) = DD_RED;
		cuddRotateRight(grandpaP);
		break;
	    }
	} else {
	    y = DD_LEFT(grandpa);
	    if (y != NULL && DD_IS_RED(y)) {
		DD_COLOR(parent) = DD_BLACK;
		DD_COLOR(y) = DD_BLACK;
		DD_COLOR(grandpa) = DD_RED;
		x = grandpa;
		stackN--;
	    } else {
		if (x == DD_LEFT(parent)) {
		    cuddRotateRight(parentP);
		    DD_COLOR(x) = DD_BLACK;
		} else {
		    DD_COLOR(parent) = DD_BLACK;
		}
		DD_COLOR(grandpa) = DD_RED;
		cuddRotateLeft(grandpaP);
	    }
	}
    }
    DD_COLOR(*(stack[0])) = DD_BLACK;

} /* end of cuddDoRebalance */
#endif
#endif


/**Function********************************************************************

  Synopsis    [Fixes a variable tree after the insertion of new subtables.]

  Description [Fixes a variable tree after the insertion of new subtables.
  After such an insertion, the low fields of the tree below the insertion
  point are inconsistent.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
ddPatchTree(
  DdManager *dd,
  MtrNode *treenode)
{
    MtrNode *auxnode = treenode;

    while (auxnode != NULL) {
	auxnode->low = dd->perm[auxnode->index];
	if (auxnode->child != NULL) {
	    ddPatchTree(dd, auxnode->child);
	}
	auxnode = auxnode->younger;
    }

    return;

} /* end of ddPatchTree */


#ifdef DD_DEBUG
/**Function********************************************************************

  Synopsis    [Checks whether a collision list is ordered.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddCheckCollisionOrdering(
  DdManager *unique,
  int i,
  int j)
{
    int slots;
    DdNode *node, *next;
    DdNodePtr *nodelist;
    DdNode *sentinel = &(unique->sentinel);

    nodelist = unique->subtables[i].nodelist;
    slots = unique->subtables[i].slots;
    node = nodelist[j];
    if (node == sentinel) return(1);
    next = node->next;
    while (next != sentinel) {
	if (cuddT(node) < cuddT(next) ||
	    (cuddT(node) == cuddT(next) && cuddE(node) < cuddE(next))) {
	    (void) fprintf(unique->err,
			   "Unordered list: index %u, position %d\n", i, j);
	    return(0);
	}
	node = next;
	next = node->next;
    }
    return(1);

} /* end of cuddCheckCollisionOrdering */
#endif




/**Function********************************************************************

  Synopsis    [Reports problem in garbage collection.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddGarbageCollect cuddGarbageCollectZdd]

******************************************************************************/
static void
ddReportRefMess(
  DdManager *unique /* manager */,
  int i /* table in which the problem occurred */,
  const char *caller /* procedure that detected the problem */)
{
    if (i == CUDD_CONST_INDEX) {
	(void) fprintf(unique->err,
			   "%s: problem in constants\n", caller);
    } else if (i != -1) {
	(void) fprintf(unique->err,
			   "%s: problem in table %d\n", caller, i);
    }
    (void) fprintf(unique->err, "  dead count != deleted\n");
    (void) fprintf(unique->err, "  This problem is often due to a missing \
call to Cudd_Ref\n  or to an extra call to Cudd_RecursiveDeref.\n  \
See the CUDD Programmer's Guide for additional details.");
    abort();

} /* end of ddReportRefMess */
