/**CFile***********************************************************************

  FileName    [cuddSubsetSP.c]

  PackageName [cudd]

  Synopsis [Procedure to subset the given BDD choosing the shortest paths
	    (largest cubes) in the BDD.]


  Description  [External procedures included in this module:
		<ul>
		<li> Cudd_SubsetShortPaths()
		<li> Cudd_SupersetShortPaths()
		</ul>
		Internal procedures included in this module:
		<ul>
		<li> cuddSubsetShortPaths()
		</ul>
		Static procedures included in this module:
		<ul>
		<li> BuildSubsetBdd()
		<li> CreatePathTable()
		<li> AssessPathLength()
		<li> CreateTopDist()
		<li> CreateBotDist()
		<li> ResizeNodeDistPages()
		<li> ResizeQueuePages()
		<li> stPathTableDdFree()
		</ul>
		]

  SeeAlso     [cuddSubsetHB.c]

  Author      [Kavita Ravi]

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

#define DEFAULT_PAGE_SIZE 2048 /* page size to store the BFS queue element type */
#define DEFAULT_NODE_DIST_PAGE_SIZE 2048 /*  page size to store NodeDist_t type */
#define MAXSHORTINT	((DdHalfWord) ~0) /* constant defined to store
					   * maximum distance of a node
					   * from the root or the constant
					   */
#define INITIAL_PAGES 128 /* number of initial pages for the
			   * queue/NodeDist_t type */

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/* structure created to store subset results for each node and distances with
 * odd and even parity of the node from the root and sink. Main data structure
 * in this procedure.
 */
struct NodeDist {
    DdHalfWord oddTopDist;
    DdHalfWord evenTopDist;
    DdHalfWord oddBotDist;
    DdHalfWord evenBotDist;
    DdNode *regResult;
    DdNode *compResult;
};

/* assorted information needed by the BuildSubsetBdd procedure. */
struct AssortedInfo {
    unsigned int maxpath;
    int findShortestPath;
    int thresholdReached;
    st_table *maxpathTable;
    int threshold;
};

struct GlobalInfo {
    struct NodeDist **nodeDistPages; /* pointers to the pages */
    int		nodeDistPageIndex; /* index to next element */
    int		nodeDistPage; /* index to current page */
    int		nodeDistPageSize; /* page size */
    int		maxNodeDistPages; /* number of page pointers */
    struct NodeDist *currentNodeDistPage; /* current page */
    DdNode      ***queuePages; /* pointers to the pages */
    int		queuePageIndex;	/* index to next element */
    int		queuePage; /* index to current page */
    int		queuePageSize; /* page size */
    int		maxQueuePages; /* number of page pointers */
    DdNode      **currentQueuePage; /* current page */
#ifdef DD_DEBUG
    int         numCalls;
    int         hits;
    int         thishit;
#endif
};

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

typedef struct NodeDist NodeDist_t;
typedef struct GlobalInfo GlobalInfo_t;

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddSubsetSP.c,v 1.36 2012/02/05 01:07:19 fabio Exp $";
#endif


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void ResizeNodeDistPages (DdManager *dd, GlobalInfo_t *gInfo);
static void ResizeQueuePages (DdManager *dd, GlobalInfo_t *gInfo);
static void CreateTopDist (DdManager *dd, GlobalInfo_t *gInfo, st_table *pathTable, int parentPage, int parentQueueIndex, int topLen, DdNode **childPage, int childQueueIndex, int numParents, FILE *fp);
static int CreateBotDist (DdNode *node, st_table *pathTable, unsigned int *pathLengthArray, FILE *fp);
static st_table * CreatePathTable (DdManager *dd, GlobalInfo_t *gInfo, DdNode *node, unsigned int *pathLengthArray, FILE *fp);
static unsigned int AssessPathLength (unsigned int *pathLengthArray, int threshold, int numVars, unsigned int *excess, FILE *fp);
static DdNode * BuildSubsetBdd (DdManager *dd, GlobalInfo_t *gInfo, st_table *pathTable, DdNode *node, struct AssortedInfo *info, st_table *subsetNodeTable);
static enum st_retval stPathTableDdFree (char *key, char *value, char *arg);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of Exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Extracts a dense subset from a BDD with the shortest paths
  heuristic.]

  Description [Extracts a dense subset from a BDD.  This procedure
  tries to preserve the shortest paths of the input BDD, because they
  give many minterms and contribute few nodes.  This procedure may
  increase the number of nodes in trying to create the subset or
  reduce the number of nodes due to recombination as compared to the
  original BDD. Hence the threshold may not be strictly adhered to. In
  practice, recombination overshadows the increase in the number of
  nodes and results in small BDDs as compared to the threshold. The
  hardlimit specifies whether threshold needs to be strictly adhered
  to. If it is set to 1, the procedure ensures that result is never
  larger than the specified limit but may be considerably less than
  the threshold.  Returns a pointer to the BDD for the subset if
  successful; NULL otherwise.  The value for numVars should be as
  close as possible to the size of the support of f for better
  efficiency. However, it is safe to pass the value returned by
  Cudd_ReadSize for numVars. If 0 is passed, then the value returned
  by Cudd_ReadSize is used.]

  SideEffects [None]

  SeeAlso     [Cudd_SupersetShortPaths Cudd_SubsetHeavyBranch Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_SubsetShortPaths(
  DdManager * dd /* manager */,
  DdNode * f /* function to be subset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* maximum number of nodes in the subset */,
  int  hardlimit /* flag: 1 if threshold is a hard limit */)
{
    DdNode *subset;

    do {
	dd->reordered = 0;
	subset = cuddSubsetShortPaths(dd, f, numVars, threshold, hardlimit);
    } while(dd->reordered == 1);

    return(subset);

} /* end of Cudd_SubsetShortPaths */


/**Function********************************************************************

  Synopsis    [Extracts a dense superset from a BDD with the shortest paths
  heuristic.]

  Description [Extracts a dense superset from a BDD.  The procedure is
  identical to the subset procedure except for the fact that it
  receives the complement of the given function. Extracting the subset
  of the complement function is equivalent to extracting the superset
  of the function.  This procedure tries to preserve the shortest
  paths of the complement BDD, because they give many minterms and
  contribute few nodes.  This procedure may increase the number of
  nodes in trying to create the superset or reduce the number of nodes
  due to recombination as compared to the original BDD. Hence the
  threshold may not be strictly adhered to. In practice, recombination
  overshadows the increase in the number of nodes and results in small
  BDDs as compared to the threshold.  The hardlimit specifies whether
  threshold needs to be strictly adhered to. If it is set to 1, the
  procedure ensures that result is never larger than the specified
  limit but may be considerably less than the threshold. Returns a
  pointer to the BDD for the superset if successful; NULL
  otherwise. The value for numVars should be as close as possible to
  the size of the support of f for better efficiency.  However, it is
  safe to pass the value returned by Cudd_ReadSize for numVar.  If 0
  is passed, then the value returned by Cudd_ReadSize is used.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetShortPaths Cudd_SupersetHeavyBranch Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_SupersetShortPaths(
  DdManager * dd /* manager */,
  DdNode * f /* function to be superset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* maximum number of nodes in the subset */,
  int  hardlimit /* flag: 1 if threshold is a hard limit */)
{
    DdNode *subset, *g;

    g = Cudd_Not(f);
    do {
	dd->reordered = 0;
	subset = cuddSubsetShortPaths(dd, g, numVars, threshold, hardlimit);
    } while(dd->reordered == 1);

    return(Cudd_NotCond(subset, (subset != NULL)));

} /* end of Cudd_SupersetShortPaths */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [The outermost procedure to return a subset of the given BDD
  with the shortest path lengths.]

  Description [The outermost procedure to return a subset of the given
  BDD with the largest cubes. The path lengths are calculated, the maximum
  allowable path length is determined and the number of nodes of this
  path length that can be used to build a subset. If the threshold is
  larger than the size of the original BDD, the original BDD is
  returned. ]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetShortPaths]

******************************************************************************/
DdNode *
cuddSubsetShortPaths(
  DdManager * dd /* DD manager */,
  DdNode * f /* function to be subset */,
  int  numVars /* total number of variables in consideration */,
  int  threshold /* maximum number of nodes allowed in the subset */,
  int  hardlimit /* flag determining whether threshold should be respected strictly */)
{
    GlobalInfo_t gInfo;
    st_table *pathTable;
    DdNode *N, *subset;

    unsigned int  *pathLengthArray;
    unsigned int maxpath, oddLen, evenLen, pathLength, *excess;
    int i;
    NodeDist_t	*nodeStat;
    struct AssortedInfo *info;
    st_table *subsetNodeTable;

    gInfo.nodeDistPageSize = DEFAULT_NODE_DIST_PAGE_SIZE;
    gInfo.queuePageSize = DEFAULT_PAGE_SIZE;

    if (numVars == 0) {
      /* set default value */
      numVars = Cudd_ReadSize(dd);
    }

    if (threshold > numVars) {
	threshold = threshold - numVars;
    }
    if (f == NULL) {
	fprintf(dd->err, "Cannot partition, nil object\n");
	dd->errorCode = CUDD_INVALID_ARG;
	return(NULL);
    }
    if (Cudd_IsConstant(f))
	return (f);

    pathLengthArray = ALLOC(unsigned int, numVars+1);
    for (i = 0; i < numVars+1; i++) pathLengthArray[i] = 0;


#ifdef DD_DEBUG
    gInfo.numCalls = 0;
#endif

    pathTable = CreatePathTable(dd, &gInfo, f, pathLengthArray, dd->err);

    if ((pathTable == NULL) || (dd->errorCode == CUDD_MEMORY_OUT)) {
	if (pathTable != NULL)
	    st_free_table(pathTable);
	FREE(pathLengthArray);
	return (NIL(DdNode));
    }

    excess = ALLOC(unsigned int, 1);
    *excess = 0;
    maxpath = AssessPathLength(pathLengthArray, threshold, numVars, excess,
			       dd->err);

    if (maxpath != (unsigned) (numVars + 1)) {

	info = ALLOC(struct AssortedInfo, 1);
	info->maxpath = maxpath;
	info->findShortestPath = 0;
	info->thresholdReached = *excess;
	info->maxpathTable = st_init_table(st_ptrcmp, st_ptrhash);
	info->threshold = threshold;

#ifdef DD_DEBUG
	(void) fprintf(dd->out, "Path length array\n");
	for (i = 0; i < (numVars+1); i++) {
	    if (pathLengthArray[i])
		(void) fprintf(dd->out, "%d ",i);
	}
	(void) fprintf(dd->out, "\n");
	for (i = 0; i < (numVars+1); i++) {
	    if (pathLengthArray[i])
		(void) fprintf(dd->out, "%d ",pathLengthArray[i]);
	}
	(void) fprintf(dd->out, "\n");
	(void) fprintf(dd->out, "Maxpath  = %d, Thresholdreached = %d\n",
		       maxpath, info->thresholdReached);
#endif

	N = Cudd_Regular(f);
	if (!st_lookup(pathTable, N, &nodeStat)) {
	    fprintf(dd->err, "Something wrong, root node must be in table\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    FREE(excess);
	    FREE(info);
	    return(NULL);
	} else {
	    if ((nodeStat->oddTopDist != MAXSHORTINT) &&
		(nodeStat->oddBotDist != MAXSHORTINT))
		oddLen = (nodeStat->oddTopDist + nodeStat->oddBotDist);
	    else
		oddLen = MAXSHORTINT;

	    if ((nodeStat->evenTopDist != MAXSHORTINT) &&
		(nodeStat->evenBotDist != MAXSHORTINT))
		evenLen = (nodeStat->evenTopDist +nodeStat->evenBotDist);
	    else
		evenLen = MAXSHORTINT;

	    pathLength = (oddLen <= evenLen) ? oddLen : evenLen;
	    if (pathLength > maxpath) {
		(void) fprintf(dd->err, "All computations are bogus, since root has path length greater than max path length within threshold %u, %u\n", maxpath, pathLength);
		dd->errorCode = CUDD_INTERNAL_ERROR;
		return(NULL);
	    }
	}

#ifdef DD_DEBUG
	gInfo.numCalls = 0;
	gInfo.hits = 0;
	gInfo.thishit = 0;
#endif
	/* initialize a table to store computed nodes */
	if (hardlimit) {
	    subsetNodeTable = st_init_table(st_ptrcmp, st_ptrhash);
	} else {
	    subsetNodeTable = NIL(st_table);
	}
	subset = BuildSubsetBdd(dd, &gInfo, pathTable, f, info, subsetNodeTable);
	if (subset != NULL) {
	    cuddRef(subset);
	}
	/* record the number of times a computed result for a node is hit */

#ifdef DD_DEBUG
	(void) fprintf(dd->out, "Hits = %d, New==Node = %d, NumCalls = %d\n",
		gInfo.hits, gInfo.thishit, gInfo.numCalls);
#endif

	if (subsetNodeTable != NIL(st_table)) {
	    st_free_table(subsetNodeTable);
	}
	st_free_table(info->maxpathTable);
	st_foreach(pathTable, stPathTableDdFree, (char *)dd);

	FREE(info);

    } else {/* if threshold larger than size of dd */
	subset = f;
	cuddRef(subset);
    }
    FREE(excess);
    st_free_table(pathTable);
    FREE(pathLengthArray);
    for (i = 0; i <= gInfo.nodeDistPage; i++) FREE(gInfo.nodeDistPages[i]);
    FREE(gInfo.nodeDistPages);

#ifdef DD_DEBUG
    /* check containment of subset in f */
    if (subset != NULL) {
	if (!Cudd_bddLeq(dd, subset, f)) {
	    (void) fprintf(dd->err, "Wrong partition\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
	}
    }
#endif

    if (subset != NULL) {
	cuddDeref(subset);
	return(subset);
    } else {
	return(NULL);
    }

} /* end of cuddSubsetShortPaths */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Resize the number of pages allocated to store the distances
  related to each node.]

  Description [Resize the number of pages allocated to store the distances
  related to each node. The procedure  moves the counter to the
  next page when the end of the page is reached and allocates new
  pages when necessary. ]

  SideEffects [Changes the size of  pages, page, page index, maximum
  number of pages freeing stuff in case of memory out. ]

  SeeAlso     []

******************************************************************************/
static void
ResizeNodeDistPages(
  DdManager *dd /* DD manager */,
  GlobalInfo_t *gInfo /* global information */)
{
    int i;
    NodeDist_t **newNodeDistPages;

    /* move to next page */
    gInfo->nodeDistPage++;

    /* If the current page index is larger than the number of pages
     * allocated, allocate a new page array. Page numbers are incremented by
     * INITIAL_PAGES
     */
    if (gInfo->nodeDistPage == gInfo->maxNodeDistPages) {
	newNodeDistPages = ALLOC(NodeDist_t *,gInfo->maxNodeDistPages + INITIAL_PAGES);
	if (newNodeDistPages == NULL) {
	    for (i = 0; i < gInfo->nodeDistPage; i++) FREE(gInfo->nodeDistPages[i]);
	    FREE(gInfo->nodeDistPages);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return;
	} else {
	    for (i = 0; i < gInfo->maxNodeDistPages; i++) {
		newNodeDistPages[i] = gInfo->nodeDistPages[i];
	    }
	    /* Increase total page count */
	    gInfo->maxNodeDistPages += INITIAL_PAGES;
	    FREE(gInfo->nodeDistPages);
	    gInfo->nodeDistPages = newNodeDistPages;
	}
    }
    /* Allocate a new page */
    gInfo->currentNodeDistPage = gInfo->nodeDistPages[gInfo->nodeDistPage] =
        ALLOC(NodeDist_t, gInfo->nodeDistPageSize);
    if (gInfo->currentNodeDistPage == NULL) {
	for (i = 0; i < gInfo->nodeDistPage; i++) FREE(gInfo->nodeDistPages[i]);
	FREE(gInfo->nodeDistPages);
	dd->errorCode = CUDD_MEMORY_OUT;
	return;
    }
    /* reset page index */
    gInfo->nodeDistPageIndex = 0;
    return;

} /* end of ResizeNodeDistPages */


/**Function********************************************************************

  Synopsis    [Resize the number of pages allocated to store nodes in the BFS
  traversal of the Bdd  .]

  Description [Resize the number of pages allocated to store nodes in the BFS
  traversal of the Bdd. The procedure  moves the counter to the
  next page when the end of the page is reached and allocates new
  pages when necessary.]

  SideEffects [Changes the size of pages, page, page index, maximum
  number of pages freeing stuff in case of memory out. ]

  SeeAlso     []

******************************************************************************/
static void
ResizeQueuePages(
  DdManager *dd /* DD manager */,
  GlobalInfo_t *gInfo /* global information */)
{
    int i;
    DdNode ***newQueuePages;

    gInfo->queuePage++;
    /* If the current page index is larger than the number of pages
     * allocated, allocate a new page array. Page numbers are incremented by
     * INITIAL_PAGES
     */
    if (gInfo->queuePage == gInfo->maxQueuePages) {
	newQueuePages = ALLOC(DdNode **,gInfo->maxQueuePages + INITIAL_PAGES);
	if (newQueuePages == NULL) {
	    for (i = 0; i < gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
	    FREE(gInfo->queuePages);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return;
	} else {
	    for (i = 0; i < gInfo->maxQueuePages; i++) {
		newQueuePages[i] = gInfo->queuePages[i];
	    }
	    /* Increase total page count */
	    gInfo->maxQueuePages += INITIAL_PAGES;
	    FREE(gInfo->queuePages);
	    gInfo->queuePages = newQueuePages;
	}
    }
    /* Allocate a new page */
    gInfo->currentQueuePage = gInfo->queuePages[gInfo->queuePage] =
        ALLOC(DdNode *,gInfo->queuePageSize);
    if (gInfo->currentQueuePage == NULL) {
	for (i = 0; i < gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
	FREE(gInfo->queuePages);
	dd->errorCode = CUDD_MEMORY_OUT;
	return;
    }
    /* reset page index */
    gInfo->queuePageIndex = 0;
    return;

} /* end of ResizeQueuePages */


/**Function********************************************************************

  Synopsis    [ Labels each node with its shortest distance from the root]

  Description [ Labels each node with its shortest distance from the root.
  This is done in a BFS search of the BDD. The nodes are processed
  in a queue implemented as pages(array) to reduce memory fragmentation.
  An entry is created for each node visited. The distance from the root
  to the node with the corresponding  parity is updated. The procedure
  is called recursively each recusion level handling nodes at a given
  level from the root.]


  SideEffects [Creates entries in the pathTable]

  SeeAlso     [CreatePathTable CreateBotDist]

******************************************************************************/
static void
CreateTopDist(
  DdManager *dd /* DD manager */,
  GlobalInfo_t *gInfo /* global information */,
  st_table * pathTable /* hast table to store path lengths */,
  int  parentPage /* the pointer to the page on which the first parent in the queue is to be found. */,
  int  parentQueueIndex /* pointer to the first parent on the page */,
  int  topLen /* current distance from the root */,
  DdNode ** childPage /* pointer to the page on which the first child is to be added. */,
  int  childQueueIndex /* pointer to the first child */,
  int  numParents /* number of parents to process in this recursive call */,
  FILE *fp /* where to write messages */)
{
    NodeDist_t *nodeStat;
    DdNode *N, *Nv, *Nnv, *node, *child, *regChild;
    int  i;
    int processingDone, childrenCount;

#ifdef DD_DEBUG
    gInfo->numCalls++;

    /* assume this procedure comes in with only the root node*/
    /* set queue index to the next available entry for addition */
    /* set queue page to page of addition */
    if ((gInfo->queuePages[parentPage] == childPage) && (parentQueueIndex ==
						  childQueueIndex)) {
	fprintf(fp, "Should not happen that they are equal\n");
    }
    assert(gInfo->queuePageIndex == childQueueIndex);
    assert(gInfo->currentQueuePage == childPage);
#endif
    /* number children added to queue is initialized , needed for
     * numParents in the next call
     */
    childrenCount = 0;
    /* process all the nodes in this level */
    while (numParents) {
	numParents--;
	if (parentQueueIndex == gInfo->queuePageSize) {
	    parentPage++;
	    parentQueueIndex = 0;
	}
	/* a parent to process */
	node = *(gInfo->queuePages[parentPage] + parentQueueIndex);
	parentQueueIndex++;
	/* get its children */
	N = Cudd_Regular(node);
	Nv = Cudd_T(N);
	Nnv = Cudd_E(N);

	Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
	Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

	processingDone = 2;
	while (processingDone) {
	    /* processing the THEN and the ELSE children, the THEN
	     * child first
	     */
	    if (processingDone == 2) {
		child = Nv;
	    } else {
		child = Nnv;
	    }

	    regChild = Cudd_Regular(child);
	    /* dont process if the child is a constant */
	    if (!Cudd_IsConstant(child)) {
		/* check is already visited, if not add a new entry in
		 * the path Table
		 */
		if (!st_lookup(pathTable, regChild, &nodeStat)) {
		    /* if not in table, has never been visited */
		    /* create entry for table */
		    if (gInfo->nodeDistPageIndex == gInfo->nodeDistPageSize)
			ResizeNodeDistPages(dd, gInfo);
		    if (dd->errorCode == CUDD_MEMORY_OUT) {
			for (i = 0; i <= gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
			FREE(gInfo->queuePages);
			st_free_table(pathTable);
			return;
		    }
		    /* New entry for child in path Table is created here */
		    nodeStat = gInfo->currentNodeDistPage + gInfo->nodeDistPageIndex;
		    gInfo->nodeDistPageIndex++;

		    /* Initialize fields of the node data */
		    nodeStat->oddTopDist = MAXSHORTINT;
		    nodeStat->evenTopDist = MAXSHORTINT;
		    nodeStat->evenBotDist = MAXSHORTINT;
		    nodeStat->oddBotDist = MAXSHORTINT;
		    nodeStat->regResult = NULL;
		    nodeStat->compResult = NULL;
		    /* update the table entry element, the distance keeps
		     * track of the parity of the path from the root
		     */
		    if (Cudd_IsComplement(child)) {
			nodeStat->oddTopDist = (DdHalfWord) topLen + 1;
		    } else {
			nodeStat->evenTopDist = (DdHalfWord) topLen + 1;
		    }

		    /* insert entry element for child in the table */
		    if (st_insert(pathTable, regChild,
				  nodeStat) == ST_OUT_OF_MEM) {
			dd->errorCode = CUDD_MEMORY_OUT;
			for (i = 0; i <= gInfo->nodeDistPage; i++)
			    FREE(gInfo->nodeDistPages[i]);
			FREE(gInfo->nodeDistPages);
			for (i = 0; i <= gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
			FREE(gInfo->queuePages);
			st_free_table(pathTable);
			return;
		    }

		    /* Create list element for this child to process its children.
		     * If this node has been processed already, then it appears
		     * in the path table and hence is never added to the list
		     * again.
		     */

		    if (gInfo->queuePageIndex == gInfo->queuePageSize) ResizeQueuePages(dd, gInfo);
		    if (dd->errorCode == CUDD_MEMORY_OUT) {
			for (i = 0; i <= gInfo->nodeDistPage; i++)
			    FREE(gInfo->nodeDistPages[i]);
			FREE(gInfo->nodeDistPages);
			st_free_table(pathTable);
			return;
		    }
		    *(gInfo->currentQueuePage + gInfo->queuePageIndex) = child;
		    gInfo->queuePageIndex++;

		    childrenCount++;
		} else {
		    /* if not been met in a path with this parity before */
		    /* put in list */
		    if (((Cudd_IsComplement(child)) && (nodeStat->oddTopDist ==
			  MAXSHORTINT)) || ((!Cudd_IsComplement(child)) &&
				  (nodeStat->evenTopDist == MAXSHORTINT))) {

			if (gInfo->queuePageIndex == gInfo->queuePageSize) ResizeQueuePages(dd, gInfo);
			if (dd->errorCode == CUDD_MEMORY_OUT) {
			    for (i = 0; i <= gInfo->nodeDistPage; i++)
				FREE(gInfo->nodeDistPages[i]);
			    FREE(gInfo->nodeDistPages);
			    st_free_table(pathTable);
			    return;

			}
			*(gInfo->currentQueuePage + gInfo->queuePageIndex) = child;
			gInfo->queuePageIndex++;

			/* update the distance with the appropriate parity */
			if (Cudd_IsComplement(child)) {
			    nodeStat->oddTopDist = (DdHalfWord) topLen + 1;
			} else {
			    nodeStat->evenTopDist = (DdHalfWord) topLen + 1;
			}
			childrenCount++;
		    }

		} /* end of else (not found in st_table) */
	    } /*end of if Not constant child */
	    processingDone--;
	} /*end of while processing Nv, Nnv */
    }  /*end of while numParents */

#ifdef DD_DEBUG
    assert(gInfo->queuePages[parentPage] == childPage);
    assert(parentQueueIndex == childQueueIndex);
#endif

    if (childrenCount != 0) {
	topLen++;
	childPage = gInfo->currentQueuePage;
	childQueueIndex = gInfo->queuePageIndex;
	CreateTopDist(dd, gInfo, pathTable, parentPage, parentQueueIndex, topLen,
		      childPage, childQueueIndex, childrenCount, fp);
    }

    return;

} /* end of CreateTopDist */


/**Function********************************************************************

  Synopsis    [ Labels each node with the shortest distance from the constant.]

  Description [Labels each node with the shortest distance from the constant.
  This is done in a DFS search of the BDD. Each node has an odd
  and even parity distance from the sink (since there exists paths to both
  zero and one) which is less than MAXSHORTINT. At each node these distances
  are updated using the minimum distance of its children from the constant.
  SInce now both the length from the root and child is known, the minimum path
  length(length of the shortest path between the root and the constant that
  this node lies on) of this node can be calculated and used to update the
  pathLengthArray]

  SideEffects [Updates Path Table and path length array]

  SeeAlso     [CreatePathTable CreateTopDist AssessPathLength]

******************************************************************************/
static int
CreateBotDist(
  DdNode * node /* current node */,
  st_table * pathTable /* path table with path lengths */,
  unsigned int * pathLengthArray /* array that stores number of nodes belonging to a particular path length. */,
  FILE *fp /* where to write messages */)
{
    DdNode *N, *Nv, *Nnv;
    DdNode *realChild;
    DdNode *child, *regChild;
    NodeDist_t *nodeStat, *nodeStatChild;
    unsigned int  oddLen, evenLen, pathLength;
    DdHalfWord botDist;
    int processingDone;

    if (Cudd_IsConstant(node))
	return(1);
    N = Cudd_Regular(node);
    /* each node has one table entry */
    /* update as you go down the min dist of each node from
       the root in each (odd and even) parity */
    if (!st_lookup(pathTable, N, &nodeStat)) {
	fprintf(fp, "Something wrong, the entry doesn't exist\n");
	return(0);
    }

    /* compute length of odd parity distances */
    if ((nodeStat->oddTopDist != MAXSHORTINT) &&
	(nodeStat->oddBotDist != MAXSHORTINT))
	oddLen = (nodeStat->oddTopDist + nodeStat->oddBotDist);
    else
	oddLen = MAXSHORTINT;

    /* compute length of even parity distances */
    if (!((nodeStat->evenTopDist == MAXSHORTINT) ||
	  (nodeStat->evenBotDist == MAXSHORTINT)))
	evenLen = (nodeStat->evenTopDist +nodeStat->evenBotDist);
    else
	evenLen = MAXSHORTINT;

    /* assign pathlength to minimum of the two */
    pathLength = (oddLen <= evenLen) ? oddLen : evenLen;

    Nv = Cudd_T(N);
    Nnv = Cudd_E(N);

    /* process each child */
    processingDone = 0;
    while (processingDone != 2) {
	if (!processingDone) {
	    child = Nv;
	} else {
	    child = Nnv;
	}

	realChild = Cudd_NotCond(child, Cudd_IsComplement(node));
	regChild = Cudd_Regular(child);
	if (Cudd_IsConstant(realChild)) {
	    /* Found a minterm; count parity and shortest distance
	    ** from the constant.
	    */
	    if (Cudd_IsComplement(child))
		nodeStat->oddBotDist = 1;
	    else
		nodeStat->evenBotDist = 1;
	} else {
	    /* If node not in table, recur. */
	    if (!st_lookup(pathTable, regChild, &nodeStatChild)) {
		fprintf(fp, "Something wrong, node in table should have been created in top dist proc.\n");
		return(0);
	    }

	    if (nodeStatChild->oddBotDist == MAXSHORTINT) {
		if (nodeStatChild->evenBotDist == MAXSHORTINT) {
		    if (!CreateBotDist(realChild, pathTable, pathLengthArray, fp))
			return(0);
		} else {
		    fprintf(fp, "Something wrong, both bot nodeStats should be there\n");
		    return(0);
		}
	    }

	    /* Update shortest distance from the constant depending on
	    **  parity. */

	    if (Cudd_IsComplement(child)) {
		/* If parity on the edge then add 1 to even distance
		** of child to get odd parity distance and add 1 to
		** odd distance of child to get even parity
		** distance. Change distance of current node only if
		** the calculated distance is less than existing
		** distance. */
		if (nodeStatChild->oddBotDist != MAXSHORTINT)
		    botDist = nodeStatChild->oddBotDist + 1;
		else
		    botDist = MAXSHORTINT;
		if (nodeStat->evenBotDist > botDist )
		    nodeStat->evenBotDist = botDist;

		if (nodeStatChild->evenBotDist != MAXSHORTINT)
		    botDist = nodeStatChild->evenBotDist + 1;
		else
		    botDist = MAXSHORTINT;
		if (nodeStat->oddBotDist > botDist)
		    nodeStat->oddBotDist = botDist;

	    } else {
		/* If parity on the edge then add 1 to even distance
		** of child to get even parity distance and add 1 to
		** odd distance of child to get odd parity distance.
		** Change distance of current node only if the
		** calculated distance is lesser than existing
		** distance. */
		if (nodeStatChild->evenBotDist != MAXSHORTINT)
		    botDist = nodeStatChild->evenBotDist + 1;
		else
		    botDist = MAXSHORTINT;
		if (nodeStat->evenBotDist > botDist)
		    nodeStat->evenBotDist = botDist;

		if (nodeStatChild->oddBotDist != MAXSHORTINT)
		    botDist = nodeStatChild->oddBotDist + 1;
		else
		    botDist = MAXSHORTINT;
		if (nodeStat->oddBotDist > botDist)
		    nodeStat->oddBotDist = botDist;
	    }
	} /* end of else (if not constant child ) */
	processingDone++;
    } /* end of while processing Nv, Nnv */

    /* Compute shortest path length on the fly. */
    if ((nodeStat->oddTopDist != MAXSHORTINT) &&
	(nodeStat->oddBotDist != MAXSHORTINT))
	oddLen = (nodeStat->oddTopDist + nodeStat->oddBotDist);
    else
	oddLen = MAXSHORTINT;

    if ((nodeStat->evenTopDist != MAXSHORTINT) &&
	(nodeStat->evenBotDist != MAXSHORTINT))
	evenLen = (nodeStat->evenTopDist +nodeStat->evenBotDist);
    else
	evenLen = MAXSHORTINT;

    /* Update path length array that has number of nodes of a particular
    ** path length. */
    if (oddLen < pathLength ) {
	if (pathLength != MAXSHORTINT)
	    pathLengthArray[pathLength]--;
	if (oddLen != MAXSHORTINT)
	    pathLengthArray[oddLen]++;
	pathLength = oddLen;
    }
    if (evenLen < pathLength ) {
	if (pathLength != MAXSHORTINT)
	    pathLengthArray[pathLength]--;
	if (evenLen != MAXSHORTINT)
	    pathLengthArray[evenLen]++;
    }

    return(1);

} /*end of CreateBotDist */


/**Function********************************************************************

  Synopsis    [ The outer procedure to label each node with its shortest
  distance from the root and constant]

  Description [ The outer procedure to label each node with its shortest
  distance from the root and constant. Calls CreateTopDist and CreateBotDist.
  The basis for computing the distance between root and constant is that
  the distance may be the sum of even distances from the node to the root
  and constant or the sum of odd distances from the node to the root and
  constant.  Both CreateTopDist and CreateBotDist create the odd and
  even parity distances from the root and constant respectively.]

  SideEffects [None]

  SeeAlso     [CreateTopDist CreateBotDist]

******************************************************************************/
static st_table *
CreatePathTable(
  DdManager *dd /* DD manager */,
  GlobalInfo_t *gInfo /* global information */,
  DdNode * node /* root of function */,
  unsigned int * pathLengthArray /* array of path lengths to store nodes labeled with the various path lengths */,
  FILE *fp /* where to write messages */)
{

    st_table *pathTable;
    NodeDist_t *nodeStat;
    DdHalfWord topLen;
    DdNode *N;
    int i, numParents;
    int insertValue;
    DdNode **childPage;
    int parentPage;
    int childQueueIndex, parentQueueIndex;

    /* Creating path Table for storing data about nodes */
    pathTable = st_init_table(st_ptrcmp,st_ptrhash);

    /* initializing pages for info about each node */
    gInfo->maxNodeDistPages = INITIAL_PAGES;
    gInfo->nodeDistPages = ALLOC(NodeDist_t *, gInfo->maxNodeDistPages);
    if (gInfo->nodeDistPages == NULL) {
	goto OUT_OF_MEM;
    }
    gInfo->nodeDistPage = 0;
    gInfo->currentNodeDistPage = gInfo->nodeDistPages[gInfo->nodeDistPage] =
	ALLOC(NodeDist_t, gInfo->nodeDistPageSize);
    if (gInfo->currentNodeDistPage == NULL) {
	for (i = 0; i <= gInfo->nodeDistPage; i++) FREE(gInfo->nodeDistPages[i]);
	FREE(gInfo->nodeDistPages);
	goto OUT_OF_MEM;
    }
    gInfo->nodeDistPageIndex = 0;

    /* Initializing pages for the BFS search queue, implemented as an array. */
    gInfo->maxQueuePages = INITIAL_PAGES;
    gInfo->queuePages = ALLOC(DdNode **, gInfo->maxQueuePages);
    if (gInfo->queuePages == NULL) {
	goto OUT_OF_MEM;
    }
    gInfo->queuePage = 0;
    gInfo->currentQueuePage  = gInfo->queuePages[gInfo->queuePage] =
        ALLOC(DdNode *, gInfo->queuePageSize);
    if (gInfo->currentQueuePage == NULL) {
	for (i = 0; i <= gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
	FREE(gInfo->queuePages);
	goto OUT_OF_MEM;
    }
    gInfo->queuePageIndex = 0;

    /* Enter the root node into the queue to start with. */
    parentPage = gInfo->queuePage;
    parentQueueIndex = gInfo->queuePageIndex;
    topLen = 0;
    *(gInfo->currentQueuePage + gInfo->queuePageIndex) = node;
    gInfo->queuePageIndex++;
    childPage = gInfo->currentQueuePage;
    childQueueIndex = gInfo->queuePageIndex;

    N = Cudd_Regular(node);

    if (gInfo->nodeDistPageIndex == gInfo->nodeDistPageSize) ResizeNodeDistPages(dd, gInfo);
    if (dd->errorCode == CUDD_MEMORY_OUT) {
	for (i = 0; i <= gInfo->nodeDistPage; i++) FREE(gInfo->nodeDistPages[i]);
	FREE(gInfo->nodeDistPages);
	for (i = 0; i <= gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
	FREE(gInfo->queuePages);
	st_free_table(pathTable);
	goto OUT_OF_MEM;
    }

    nodeStat = gInfo->currentNodeDistPage + gInfo->nodeDistPageIndex;
    gInfo->nodeDistPageIndex++;

    nodeStat->oddTopDist = MAXSHORTINT;
    nodeStat->evenTopDist = MAXSHORTINT;
    nodeStat->evenBotDist = MAXSHORTINT;
    nodeStat->oddBotDist = MAXSHORTINT;
    nodeStat->regResult = NULL;
    nodeStat->compResult = NULL;

    insertValue = st_insert(pathTable, N, nodeStat);
    if (insertValue == ST_OUT_OF_MEM) {
	dd->errorCode = CUDD_MEMORY_OUT;
	for (i = 0; i <= gInfo->nodeDistPage; i++) FREE(gInfo->nodeDistPages[i]);
	FREE(gInfo->nodeDistPages);
	for (i = 0; i <= gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
	FREE(gInfo->queuePages);
	st_free_table(pathTable);
	goto OUT_OF_MEM;
    } else if (insertValue == 1) {
	fprintf(fp, "Something wrong, the entry exists but didnt show up in st_lookup\n");
	return(NULL);
    }

    if (Cudd_IsComplement(node)) {
	nodeStat->oddTopDist = 0;
    } else {
	nodeStat->evenTopDist = 0;
    }
    numParents = 1;
    /* call the function that counts the distance of each node from the
     * root
     */
#ifdef DD_DEBUG
    gInfo->numCalls = 0;
#endif
    CreateTopDist(dd, gInfo, pathTable, parentPage, parentQueueIndex, (int) topLen,
		  childPage, childQueueIndex, numParents, fp);
    if (dd->errorCode == CUDD_MEMORY_OUT) {
	fprintf(fp, "Out of Memory and cant count path lengths\n");
	goto OUT_OF_MEM;
    }

#ifdef DD_DEBUG
    gInfo->numCalls = 0;
#endif
    /* call the function that counts the distance of each node from the
     * constant
     */
    if (!CreateBotDist(node, pathTable, pathLengthArray, fp)) return(NULL);

    /* free BFS queue pages as no longer required */
    for (i = 0; i <= gInfo->queuePage; i++) FREE(gInfo->queuePages[i]);
    FREE(gInfo->queuePages);
    return(pathTable);

OUT_OF_MEM:
    (void) fprintf(fp, "Out of Memory, cannot allocate pages\n");
    dd->errorCode = CUDD_MEMORY_OUT;
    return(NULL);

} /*end of CreatePathTable */


/**Function********************************************************************

  Synopsis    [Chooses the maximum allowable path length of nodes under the
  threshold.]

  Description [Chooses the maximum allowable path length under each node.
  The corner cases are when the threshold is larger than the number
  of nodes in the BDD iself, in which case 'numVars + 1' is returned.
  If all nodes of a particular path length are needed, then the
  maxpath returned is the next one with excess nodes = 0;]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static unsigned int
AssessPathLength(
  unsigned int * pathLengthArray /* array determining number of nodes belonging to the different path lengths */,
  int  threshold /* threshold to determine maximum allowable nodes in the subset */,
  int  numVars /* maximum number of variables */,
  unsigned int * excess /* number of nodes labeled maxpath required in the subset */,
  FILE *fp /* where to write messages */)
{
    unsigned int i, maxpath;
    int temp;

    temp = threshold;
    i = 0;
    maxpath = 0;
    /* quit loop if i reaches max number of variables or if temp reaches
     * below zero
     */
    while ((i < (unsigned) numVars+1) && (temp > 0)) {
	if (pathLengthArray[i] > 0) {
	    maxpath = i;
	    temp = temp - pathLengthArray[i];
	}
	i++;
    }
    /* if all nodes of max path are needed */
    if (temp >= 0) {
	maxpath++; /* now maxpath  becomes the next maxppath or max number
		      of variables */
	*excess = 0;
    } else { /* normal case when subset required is less than size of
		original BDD */
	*excess = temp + pathLengthArray[maxpath];
    }

    if (maxpath == 0) {
	fprintf(fp, "Path Length array seems to be all zeroes, check\n");
    }
    return(maxpath);

} /* end of AssessPathLength */


/**Function********************************************************************

  Synopsis    [Builds the BDD with nodes labeled with path length less than or equal to maxpath]

  Description [Builds the BDD with nodes labeled with path length
  under maxpath and as many nodes labeled maxpath as determined by the
  threshold. The procedure uses the path table to determine which nodes
  in the original bdd need to be retained. This procedure picks a
  shortest path (tie break decided by taking the child with the shortest
  distance to the constant) and recurs down the path till it reaches the
  constant. the procedure then starts building the subset upward from
  the constant. All nodes labeled by path lengths less than the given
  maxpath are used to build the subset.  However, in the case of nodes
  that have label equal to maxpath, as many are chosen as required by
  the threshold. This number is stored in the info structure in the
  field thresholdReached. This field is decremented whenever a node
  labeled maxpath is encountered and the nodes labeled maxpath are
  aggregated in a maxpath table. As soon as the thresholdReached count
  goes to 0, the shortest path from this node to the constant is found.
  The extraction of nodes with the above labeling is based on the fact
  that each node, labeled with a path length, P, has at least one child
  labeled P or less. So extracting all nodes labeled a given path length
  P ensures complete paths between the root and the constant. Extraction
  of a partial number of nodes with a given path length may result in
  incomplete paths and hence the additional number of nodes are grabbed
  to complete the path. Since the Bdd is built bottom-up, other nodes
  labeled maxpath do lie on complete paths.  The procedure may cause the
  subset to have a larger or smaller number of nodes than the specified
  threshold. The increase in the number of nodes is caused by the
  building of a subset and the reduction by recombination. However in
  most cases, the recombination overshadows the increase and the
  procedure returns a result with lower number of nodes than specified.
  The subsetNodeTable is NIL when there is no hard limit on the number
  of nodes. Further efforts towards keeping the subset closer to the
  threshold number were abandoned in favour of keeping the procedure
  simple and fast.]

  SideEffects [SubsetNodeTable is changed if it is not NIL.]

  SeeAlso     []

******************************************************************************/
static DdNode *
BuildSubsetBdd(
  DdManager * dd /* DD manager */,
  GlobalInfo_t *gInfo /* global information */,
  st_table * pathTable /* path table with path lengths and computed results */,
  DdNode * node /* current node */,
  struct AssortedInfo * info /* assorted information structure */,
  st_table * subsetNodeTable /* table storing computed results */)
{
    DdNode *N, *Nv, *Nnv;
    DdNode *ThenBranch, *ElseBranch, *childBranch;
    DdNode *child, *regChild, *regNnv, *regNv;
    NodeDist_t *nodeStatNv, *nodeStat, *nodeStatNnv;
    DdNode *neW, *topv, *regNew;
    char *entry;
    unsigned int topid;
    unsigned int childPathLength, oddLen, evenLen, NnvPathLength, NvPathLength;
    unsigned int NvBotDist, NnvBotDist;
    int tiebreakChild;
    int  processingDone, thenDone, elseDone;

    DdNode *zero = Cudd_Not(DD_ONE(dd));
#ifdef DD_DEBUG
    gInfo->numCalls++;
#endif
    if (Cudd_IsConstant(node))
	return(node);

    N = Cudd_Regular(node);
    /* Find node in table. */
    if (!st_lookup(pathTable, N, &nodeStat)) {
	(void) fprintf(dd->err, "Something wrong, node must be in table \n");
	dd->errorCode = CUDD_INTERNAL_ERROR;
	return(NULL);
    }
    /* If the node in the table has been visited, then return the corresponding
    ** Dd. Since a node can become a subset of itself, its
    ** complement (that is te same node reached by a different parity) will
    ** become a superset of the original node and result in some minterms
    ** that were not in the original set. Hence two different results are
    ** maintained, corresponding to the odd and even parities.
    */

    /* If this node is reached with an odd parity, get odd parity results. */
    if (Cudd_IsComplement(node)) {
	if  (nodeStat->compResult != NULL) {
#ifdef DD_DEBUG
	    gInfo->hits++;
#endif
	    return(nodeStat->compResult);
	}
    } else {
	/* if this node is reached with an even parity, get even parity
	 * results
	 */
	if (nodeStat->regResult != NULL) {
#ifdef DD_DEBUG
	    gInfo->hits++;
#endif
	    return(nodeStat->regResult);
	}
    }


    /* get children */
    Nv = Cudd_T(N);
    Nnv = Cudd_E(N);

    Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
    Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

    /* no child processed */
    processingDone = 0;
    /* then child not processed */
    thenDone = 0;
    ThenBranch = NULL;
    /* else child not processed */
    elseDone = 0;
    ElseBranch = NULL;
    /* if then child constant, branch is the child */
    if (Cudd_IsConstant(Nv)) {
	/*shortest path found */
	if ((Nv == DD_ONE(dd)) && (info->findShortestPath)) {
	    info->findShortestPath = 0;
	}

	ThenBranch = Nv;
	cuddRef(ThenBranch);
	if (ThenBranch == NULL) {
	    return(NULL);
	}

	thenDone++;
	processingDone++;
	NvBotDist = MAXSHORTINT;
    } else {
	/* Derive regular child for table lookup. */
	regNv = Cudd_Regular(Nv);
	/* Get node data for shortest path length. */
	if (!st_lookup(pathTable, regNv, &nodeStatNv) ) {
	    (void) fprintf(dd->err, "Something wrong, node must be in table\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
	}
	/* Derive shortest path length for child. */
	if ((nodeStatNv->oddTopDist != MAXSHORTINT) &&
	    (nodeStatNv->oddBotDist != MAXSHORTINT)) {
	    oddLen = (nodeStatNv->oddTopDist + nodeStatNv->oddBotDist);
	} else {
	    oddLen = MAXSHORTINT;
	}

	if ((nodeStatNv->evenTopDist != MAXSHORTINT) &&
	    (nodeStatNv->evenBotDist != MAXSHORTINT)) {
	    evenLen = (nodeStatNv->evenTopDist +nodeStatNv->evenBotDist);
	} else {
	    evenLen = MAXSHORTINT;
	}

	NvPathLength = (oddLen <= evenLen) ? oddLen : evenLen;
	NvBotDist = (oddLen <= evenLen) ? nodeStatNv->oddBotDist:
						   nodeStatNv->evenBotDist;
    }
    /* if else child constant, branch is the child */
    if (Cudd_IsConstant(Nnv)) {
	/*shortest path found */
	if ((Nnv == DD_ONE(dd)) && (info->findShortestPath)) {
	    info->findShortestPath = 0;
	}

	ElseBranch = Nnv;
	cuddRef(ElseBranch);
	if (ElseBranch == NULL) {
	    return(NULL);
	}

	elseDone++;
	processingDone++;
	NnvBotDist = MAXSHORTINT;
    } else {
	/* Derive regular child for table lookup. */
	regNnv = Cudd_Regular(Nnv);
	/* Get node data for shortest path length. */
	if (!st_lookup(pathTable, regNnv, &nodeStatNnv) ) {
	    (void) fprintf(dd->err, "Something wrong, node must be in table\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
	}
	/* Derive shortest path length for child. */
	if ((nodeStatNnv->oddTopDist != MAXSHORTINT) &&
	    (nodeStatNnv->oddBotDist != MAXSHORTINT)) {
	    oddLen = (nodeStatNnv->oddTopDist + nodeStatNnv->oddBotDist);
	} else {
	    oddLen = MAXSHORTINT;
	}

	if ((nodeStatNnv->evenTopDist != MAXSHORTINT) &&
	    (nodeStatNnv->evenBotDist != MAXSHORTINT)) {
	    evenLen = (nodeStatNnv->evenTopDist +nodeStatNnv->evenBotDist);
	} else {
	    evenLen = MAXSHORTINT;
	}

	NnvPathLength = (oddLen <= evenLen) ? oddLen : evenLen;
	NnvBotDist = (oddLen <= evenLen) ? nodeStatNnv->oddBotDist :
						   nodeStatNnv->evenBotDist;
    }

    tiebreakChild = (NvBotDist <= NnvBotDist) ? 1 : 0;
    /* while both children not processed */
    while (processingDone != 2) {
	if (!processingDone) {
	    /* if no child processed */
	    /* pick the child with shortest path length and record which one
	     * picked
	     */
	    if ((NvPathLength < NnvPathLength) ||
		((NvPathLength == NnvPathLength) && (tiebreakChild == 1))) {
		child = Nv;
		regChild = regNv;
		thenDone = 1;
		childPathLength = NvPathLength;
	    } else {
		child = Nnv;
		regChild = regNnv;
		elseDone = 1;
		childPathLength = NnvPathLength;
	    } /* then path length less than else path length */
	} else {
	    /* if one child processed, process the other */
	    if (thenDone) {
		child = Nnv;
		regChild = regNnv;
		elseDone = 1;
		childPathLength = NnvPathLength;
	    } else {
		child = Nv;
		regChild = regNv;
		thenDone = 1;
		childPathLength = NvPathLength;
	    } /* end of else pick the Then child if ELSE child processed */
	} /* end of else one child has been processed */

	/* ignore (replace with constant 0) all nodes which lie on paths larger
	 * than the maximum length of the path required
	 */
	if (childPathLength > info->maxpath) {
	    /* record nodes visited */
	    childBranch = zero;
	} else {
	    if (childPathLength < info->maxpath) {
		if (info->findShortestPath) {
		    info->findShortestPath = 0;
		}
		childBranch = BuildSubsetBdd(dd, gInfo, pathTable, child, info,
					     subsetNodeTable);

	    } else { /* Case: path length of node = maxpath */
		/* If the node labeled with maxpath is found in the
		** maxpathTable, use it to build the subset BDD.  */
		if (st_lookup(info->maxpathTable, regChild, &entry)) {
		    /* When a node that is already been chosen is hit,
		    ** the quest for a complete path is over.  */
		    if (info->findShortestPath) {
			info->findShortestPath = 0;
		    }
		    childBranch = BuildSubsetBdd(dd, gInfo, pathTable, child, info,
						 subsetNodeTable);
		} else {
		    /* If node is not found in the maxpathTable and
		    ** the threshold has been reached, then if the
		    ** path needs to be completed, continue. Else
		    ** replace the node with a zero.  */
		    if (info->thresholdReached <= 0) {
			if (info->findShortestPath) {
			    if (st_insert(info->maxpathTable, regChild,
					  NULL) == ST_OUT_OF_MEM) {
				dd->errorCode = CUDD_MEMORY_OUT;
				(void) fprintf(dd->err, "OUT of memory\n");
				info->thresholdReached = 0;
				childBranch = zero;
			    } else {
				info->thresholdReached--;
				childBranch = BuildSubsetBdd(dd, gInfo, pathTable,
						    child, info,subsetNodeTable);
			    }
			} else { /* not find shortest path, we dont need this
				    node */
			    childBranch = zero;
			}
		    } else { /* Threshold hasn't been reached,
			     ** need the node. */
			if (st_insert(info->maxpathTable, regChild,
				      NULL) == ST_OUT_OF_MEM) {
			    dd->errorCode = CUDD_MEMORY_OUT;
			    (void) fprintf(dd->err, "OUT of memory\n");
			    info->thresholdReached = 0;
			    childBranch = zero;
			} else {
			    info->thresholdReached--;
			    if (info->thresholdReached <= 0) {
				info->findShortestPath = 1;
			    }
			    childBranch = BuildSubsetBdd(dd, gInfo, pathTable,
						 child, info, subsetNodeTable);

			} /* end of st_insert successful */
		    } /* end of threshold hasnt been reached yet */
		} /* end of else node not found in maxpath table */
	    } /* end of if (path length of node = maxpath) */
	} /* end if !(childPathLength > maxpath) */
	if (childBranch == NULL) {
	    /* deref other stuff incase reordering has taken place */
	    if (ThenBranch != NULL) {
		Cudd_RecursiveDeref(dd, ThenBranch);
		ThenBranch = NULL;
	    }
	    if (ElseBranch != NULL) {
		Cudd_RecursiveDeref(dd, ElseBranch);
		ElseBranch = NULL;
	    }
	    return(NULL);
	}

	cuddRef(childBranch);

	if (child == Nv) {
	    ThenBranch = childBranch;
	} else {
	    ElseBranch = childBranch;
	}
	processingDone++;

    } /*end of while processing Nv, Nnv */

    info->findShortestPath = 0;
    topid = Cudd_NodeReadIndex(N);
    topv = Cudd_ReadVars(dd, topid);
    cuddRef(topv);
    neW = cuddBddIteRecur(dd, topv, ThenBranch, ElseBranch);
    if (neW != NULL) {
	cuddRef(neW);
    }
    Cudd_RecursiveDeref(dd, topv);
    Cudd_RecursiveDeref(dd, ThenBranch);
    Cudd_RecursiveDeref(dd, ElseBranch);


    /* Hard Limit of threshold has been imposed */
    if (subsetNodeTable != NIL(st_table)) {
	/* check if a new node is created */
	regNew = Cudd_Regular(neW);
	/* subset node table keeps all new nodes that have been created to keep
	 * a running count of how many nodes have been built in the subset.
	 */
	if (!st_lookup(subsetNodeTable, regNew, &entry)) {
	    if (!Cudd_IsConstant(regNew)) {
		if (st_insert(subsetNodeTable, regNew,
			      NULL) == ST_OUT_OF_MEM) {
		    (void) fprintf(dd->err, "Out of memory\n");
		    return (NULL);
		}
		if (st_count(subsetNodeTable) > info->threshold) {
		    info->thresholdReached = 0;
		}
	    }
	}
    }


    if (neW == NULL) {
	return(NULL);
    } else {
	/*store computed result in regular form*/
	if (Cudd_IsComplement(node)) {
	    nodeStat->compResult = neW;
	    cuddRef(nodeStat->compResult);
	    /* if the new node is the same as the corresponding node in the
	     * original bdd then its complement need not be computed as it
	     * cannot be larger than the node itself
	     */
	    if (neW == node) {
#ifdef DD_DEBUG
		gInfo->thishit++;
#endif
		/* if a result for the node has already been computed, then
		 * it can only be smaller than teh node itself. hence store
		 * the node result in order not to break recombination
		 */
		if (nodeStat->regResult != NULL) {
		    Cudd_RecursiveDeref(dd, nodeStat->regResult);
		}
		nodeStat->regResult = Cudd_Not(neW);
		cuddRef(nodeStat->regResult);
	    }

	} else {
	    nodeStat->regResult = neW;
	    cuddRef(nodeStat->regResult);
	    if (neW == node) {
#ifdef DD_DEBUG
		gInfo->thishit++;
#endif
		if (nodeStat->compResult != NULL) {
		    Cudd_RecursiveDeref(dd, nodeStat->compResult);
		}
		nodeStat->compResult = Cudd_Not(neW);
		cuddRef(nodeStat->compResult);
	    }
	}

	cuddDeref(neW);
	return(neW);
    } /* end of else i.e. Subset != NULL */
} /* end of BuildSubsetBdd */


/**Function********************************************************************

  Synopsis     [Procedure to free te result dds stored in the NodeDist pages.]

  Description [None]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static enum st_retval
stPathTableDdFree(
  char * key,
  char * value,
  char * arg)
{
    NodeDist_t *nodeStat;
    DdManager *dd;

    nodeStat = (NodeDist_t *)value;
    dd = (DdManager *)arg;
    if (nodeStat->regResult != NULL) {
	Cudd_RecursiveDeref(dd, nodeStat->regResult);
    }
    if (nodeStat->compResult != NULL) {
	Cudd_RecursiveDeref(dd, nodeStat->compResult);
    }
    return(ST_CONTINUE);

} /* end of stPathTableFree */
