/**CFile***********************************************************************

  FileName    [cuddSubsetHB.c]

  PackageName [cudd]

  Synopsis    [Procedure to subset the given BDD by choosing the heavier
	       branches.]


  Description [External procedures provided by this module:
		<ul>
		<li> Cudd_SubsetHeavyBranch()
		<li> Cudd_SupersetHeavyBranch()
		</ul>
	       Internal procedures included in this module:
		<ul>
		<li> cuddSubsetHeavyBranch()
		</ul>
	       Static procedures included in this module:
		<ul>
		<li> ResizeCountMintermPages();
		<li> ResizeNodeDataPages()
		<li> ResizeCountNodePages()
		<li> SubsetCountMintermAux()
		<li> SubsetCountMinterm()
		<li> SubsetCountNodesAux()
		<li> SubsetCountNodes()
		<li> BuildSubsetBdd()
		</ul>
		]

  SeeAlso     [cuddSubsetSP.c]

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

#ifdef __STDC__
#include <float.h>
#else
#define DBL_MAX_EXP 1024
#endif
#include "CUDD/util.h"
#include "CUDD/cuddInt.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define	DEFAULT_PAGE_SIZE 2048
#define	DEFAULT_NODE_DATA_PAGE_SIZE 1024
#define INITIAL_PAGES 128


/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/* data structure to store the information on each node. It keeps
 * the number of minterms represented by the DAG rooted at this node
 * in terms of the number of variables specified by the user, number
 * of nodes in this DAG and the number of nodes of its child with
 * lesser number of minterms that are not shared by the child with
 * more minterms
 */
struct NodeData {
    double *mintermPointer;
    int *nodesPointer;
    int *lightChildNodesPointer;
};

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

typedef struct NodeData NodeData_t;

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddSubsetHB.c,v 1.39 2012/02/05 01:07:19 fabio Exp $";
#endif

static int memOut;
#ifdef DEBUG
static	int		num_calls;
#endif

static	DdNode	        *zero, *one; /* constant functions */
static	double		**mintermPages;	/* pointers to the pages */
static	int		**nodePages; /* pointers to the pages */
static	int		**lightNodePages; /* pointers to the pages */
static	double		*currentMintermPage; /* pointer to the current
						   page */
static  double		max; /* to store the 2^n value of the number
			      * of variables */

static	int		*currentNodePage; /* pointer to the current
						   page */
static	int		*currentLightNodePage; /* pointer to the
						*  current page */
static	int		pageIndex; /* index to next element */
static	int		page; /* index to current page */
static	int		pageSize = DEFAULT_PAGE_SIZE; /* page size */
static  int             maxPages; /* number of page pointers */

static	NodeData_t	*currentNodeDataPage; /* pointer to the current
						 page */
static	int		nodeDataPage; /* index to next element */
static	int		nodeDataPageIndex; /* index to next element */
static	NodeData_t	**nodeDataPages; /* index to current page */
static	int		nodeDataPageSize = DEFAULT_NODE_DATA_PAGE_SIZE;
						     /* page size */
static  int             maxNodeDataPages; /* number of page pointers */


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void ResizeNodeDataPages (void);
static void ResizeCountMintermPages (void);
static void ResizeCountNodePages (void);
static double SubsetCountMintermAux (DdNode *node, double max, st_table *table);
static st_table * SubsetCountMinterm (DdNode *node, int nvars);
static int SubsetCountNodesAux (DdNode *node, st_table *table, double max);
static int SubsetCountNodes (DdNode *node, st_table *table, int nvars);
static void StoreNodes (st_table *storeTable, DdManager *dd, DdNode *node);
static DdNode * BuildSubsetBdd (DdManager *dd, DdNode *node, int *size, st_table *visitedTable, int threshold, st_table *storeTable, st_table *approxTable);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Extracts a dense subset from a BDD with the heavy branch
  heuristic.]

  Description [Extracts a dense subset from a BDD. This procedure
  builds a subset by throwing away one of the children of each node,
  starting from the root, until the result is small enough. The child
  that is eliminated from the result is the one that contributes the
  fewer minterms.  Returns a pointer to the BDD of the subset if
  successful. NULL if the procedure runs out of memory. The parameter
  numVars is the maximum number of variables to be used in minterm
  calculation and node count calculation.  The optimal number should
  be as close as possible to the size of the support of f.  However,
  it is safe to pass the value returned by Cudd_ReadSize for numVars
  when the number of variables is under 1023.  If numVars is larger
  than 1023, it will overflow. If a 0 parameter is passed then the
  procedure will compute a value which will avoid overflow but will
  cause underflow with 2046 variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetShortPaths Cudd_SupersetHeavyBranch Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_SubsetHeavyBranch(
  DdManager * dd /* manager */,
  DdNode * f /* function to be subset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* maximum number of nodes in the subset */)
{
    DdNode *subset;

    memOut = 0;
    do {
	dd->reordered = 0;
	subset = cuddSubsetHeavyBranch(dd, f, numVars, threshold);
    } while ((dd->reordered == 1) && (!memOut));

    return(subset);

} /* end of Cudd_SubsetHeavyBranch */


/**Function********************************************************************

  Synopsis    [Extracts a dense superset from a BDD with the heavy branch
  heuristic.]

  Description [Extracts a dense superset from a BDD. The procedure is
  identical to the subset procedure except for the fact that it
  receives the complement of the given function. Extracting the subset
  of the complement function is equivalent to extracting the superset
  of the function. This procedure builds a superset by throwing away
  one of the children of each node starting from the root of the
  complement function, until the result is small enough. The child
  that is eliminated from the result is the one that contributes the
  fewer minterms.
  Returns a pointer to the BDD of the superset if successful. NULL if
  intermediate result causes the procedure to run out of memory. The
  parameter numVars is the maximum number of variables to be used in
  minterm calculation and node count calculation.  The optimal number
  should be as close as possible to the size of the support of f.
  However, it is safe to pass the value returned by Cudd_ReadSize for
  numVars when the number of variables is under 1023.  If numVars is
  larger than 1023, it will overflow. If a 0 parameter is passed then
  the procedure will compute a value which will avoid overflow but
  will cause underflow with 2046 variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetHeavyBranch Cudd_SupersetShortPaths Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_SupersetHeavyBranch(
  DdManager * dd /* manager */,
  DdNode * f /* function to be superset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* maximum number of nodes in the superset */)
{
    DdNode *subset, *g;

    g = Cudd_Not(f);
    memOut = 0;
    do {
	dd->reordered = 0;
	subset = cuddSubsetHeavyBranch(dd, g, numVars, threshold);
    } while ((dd->reordered == 1) && (!memOut));

    return(Cudd_NotCond(subset, (subset != NULL)));

} /* end of Cudd_SupersetHeavyBranch */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [The main procedure that returns a subset by choosing the heavier
  branch in the BDD.]

  Description [Here a subset BDD is built by throwing away one of the
  children. Starting at root, annotate each node with the number of
  minterms (in terms of the total number of variables specified -
  numVars), number of nodes taken by the DAG rooted at this node and
  number of additional nodes taken by the child that has the lesser
  minterms. The child with the lower number of minterms is thrown away
  and a dyanmic count of the nodes of the subset is kept. Once the
  threshold is reached the subset is returned to the calling
  procedure.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetHeavyBranch]

******************************************************************************/
DdNode *
cuddSubsetHeavyBranch(
  DdManager * dd /* DD manager */,
  DdNode * f /* current DD */,
  int  numVars /* maximum number of variables */,
  int  threshold /* threshold size for the subset */)
{

    int i, *size;
    st_table *visitedTable;
    int numNodes;
    NodeData_t *currNodeQual;
    DdNode *subset;
    st_table *storeTable, *approxTable;
    DdNode *key, *value;
    st_generator *stGen;

    if (f == NULL) {
	fprintf(dd->err, "Cannot subset, nil object\n");
	dd->errorCode = CUDD_INVALID_ARG;
	return(NULL);
    }

    one	 = Cudd_ReadOne(dd);
    zero = Cudd_Not(one);

    /* If user does not know numVars value, set it to the maximum
     * exponent that the pow function can take. The -1 is due to the
     * discrepancy in the value that pow takes and the value that
     * log gives.
     */
    if (numVars == 0) {
	/* set default value */
	numVars = DBL_MAX_EXP - 1;
    }

    if (Cudd_IsConstant(f)) {
	return(f);
    }

    max = pow(2.0, (double)numVars);

    /* Create visited table where structures for node data are allocated and
       stored in a st_table */
    visitedTable = SubsetCountMinterm(f, numVars);
    if ((visitedTable == NULL) || memOut) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	dd->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    numNodes = SubsetCountNodes(f, visitedTable, numVars);
    if (memOut) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	dd->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }

    if (st_lookup(visitedTable, f, &currNodeQual) == 0) {
	fprintf(dd->err,
		"Something is wrong, ought to be node quality table\n");
	dd->errorCode = CUDD_INTERNAL_ERROR;
    }

    size = ALLOC(int, 1);
    if (size == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    *size = numNodes;

#ifdef DEBUG
    num_calls = 0;
#endif
    /* table to store nodes being created. */
    storeTable = st_init_table(st_ptrcmp, st_ptrhash);
    /* insert the constant */
    cuddRef(one);
    if (st_insert(storeTable, Cudd_ReadOne(dd), NULL) ==
	ST_OUT_OF_MEM) {
	fprintf(dd->out, "Something wrong, st_table insert failed\n");
    }
    /* table to store approximations of nodes */
    approxTable = st_init_table(st_ptrcmp, st_ptrhash);
    subset = (DdNode *)BuildSubsetBdd(dd, f, size, visitedTable, threshold,
				      storeTable, approxTable);
    if (subset != NULL) {
	cuddRef(subset);
    }

    stGen = st_init_gen(approxTable);
    if (stGen == NULL) {
	st_free_table(approxTable);
	return(NULL);
    }
    while(st_gen(stGen, &key, &value)) {
	Cudd_RecursiveDeref(dd, value);
    }
    st_free_gen(stGen); stGen = NULL;
    st_free_table(approxTable);

    stGen = st_init_gen(storeTable);
    if (stGen == NULL) {
	st_free_table(storeTable);
	return(NULL);
    }
    while(st_gen(stGen, &key, &value)) {
	Cudd_RecursiveDeref(dd, key);
    }
    st_free_gen(stGen); stGen = NULL;
    st_free_table(storeTable);

    for (i = 0; i <= page; i++) {
	FREE(mintermPages[i]);
    }
    FREE(mintermPages);
    for (i = 0; i <= page; i++) {
	FREE(nodePages[i]);
    }
    FREE(nodePages);
    for (i = 0; i <= page; i++) {
	FREE(lightNodePages[i]);
    }
    FREE(lightNodePages);
    for (i = 0; i <= nodeDataPage; i++) {
	FREE(nodeDataPages[i]);
    }
    FREE(nodeDataPages);
    st_free_table(visitedTable);
    FREE(size);
#if 0
    (void) Cudd_DebugCheck(dd);
    (void) Cudd_CheckKeys(dd);
#endif

    if (subset != NULL) {
#ifdef DD_DEBUG
      if (!Cudd_bddLeq(dd, subset, f)) {
	    fprintf(dd->err, "Wrong subset\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
      }
#endif
	cuddDeref(subset);
	return(subset);
    } else {
	return(NULL);
    }
} /* end of cuddSubsetHeavyBranch */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Resize the number of pages allocated to store the node data.]

  Description [Resize the number of pages allocated to store the node data
  The procedure  moves the counter to the next page when the end of
  the page is reached and allocates new pages when necessary.]

  SideEffects [Changes the size of pages, page, page index, maximum
  number of pages freeing stuff in case of memory out. ]

  SeeAlso     []

******************************************************************************/
static void
ResizeNodeDataPages(void)
{
    int i;
    NodeData_t **newNodeDataPages;

    nodeDataPage++;
    /* If the current page index is larger than the number of pages
     * allocated, allocate a new page array. Page numbers are incremented by
     * INITIAL_PAGES
     */
    if (nodeDataPage == maxNodeDataPages) {
	newNodeDataPages = ALLOC(NodeData_t *,maxNodeDataPages + INITIAL_PAGES);
	if (newNodeDataPages == NULL) {
	    for (i = 0; i < nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    memOut = 1;
	    return;
	} else {
	    for (i = 0; i < maxNodeDataPages; i++) {
		newNodeDataPages[i] = nodeDataPages[i];
	    }
	    /* Increase total page count */
	    maxNodeDataPages += INITIAL_PAGES;
	    FREE(nodeDataPages);
	    nodeDataPages = newNodeDataPages;
	}
    }
    /* Allocate a new page */
    currentNodeDataPage = nodeDataPages[nodeDataPage] =
	ALLOC(NodeData_t ,nodeDataPageSize);
    if (currentNodeDataPage == NULL) {
	for (i = 0; i < nodeDataPage; i++) FREE(nodeDataPages[i]);
	FREE(nodeDataPages);
	memOut = 1;
	return;
    }
    /* reset page index */
    nodeDataPageIndex = 0;
    return;

} /* end of ResizeNodeDataPages */


/**Function********************************************************************

  Synopsis    [Resize the number of pages allocated to store the minterm
  counts. ]

  Description [Resize the number of pages allocated to store the minterm
  counts.  The procedure  moves the counter to the next page when the
  end of the page is reached and allocates new pages when necessary.]

  SideEffects [Changes the size of minterm pages, page, page index, maximum
  number of pages freeing stuff in case of memory out. ]

  SeeAlso     []

******************************************************************************/
static void
ResizeCountMintermPages(void)
{
    int i;
    double **newMintermPages;

    page++;
    /* If the current page index is larger than the number of pages
     * allocated, allocate a new page array. Page numbers are incremented by
     * INITIAL_PAGES
     */
    if (page == maxPages) {
	newMintermPages = ALLOC(double *,maxPages + INITIAL_PAGES);
	if (newMintermPages == NULL) {
	    for (i = 0; i < page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    memOut = 1;
	    return;
	} else {
	    for (i = 0; i < maxPages; i++) {
		newMintermPages[i] = mintermPages[i];
	    }
	    /* Increase total page count */
	    maxPages += INITIAL_PAGES;
	    FREE(mintermPages);
	    mintermPages = newMintermPages;
	}
    }
    /* Allocate a new page */
    currentMintermPage = mintermPages[page] = ALLOC(double,pageSize);
    if (currentMintermPage == NULL) {
	for (i = 0; i < page; i++) FREE(mintermPages[i]);
	FREE(mintermPages);
	memOut = 1;
	return;
    }
    /* reset page index */
    pageIndex = 0;
    return;

} /* end of ResizeCountMintermPages */


/**Function********************************************************************

  Synopsis    [Resize the number of pages allocated to store the node counts.]

  Description [Resize the number of pages allocated to store the node counts.
  The procedure  moves the counter to the next page when the end of
  the page is reached and allocates new pages when necessary.]

  SideEffects [Changes the size of pages, page, page index, maximum
  number of pages freeing stuff in case of memory out.]

  SeeAlso     []

******************************************************************************/
static void
ResizeCountNodePages(void)
{
    int i;
    int **newNodePages;

    page++;

    /* If the current page index is larger than the number of pages
     * allocated, allocate a new page array. The number of pages is incremented
     * by INITIAL_PAGES.
     */
    if (page == maxPages) {
	newNodePages = ALLOC(int *,maxPages + INITIAL_PAGES);
	if (newNodePages == NULL) {
	    for (i = 0; i < page; i++) FREE(nodePages[i]);
	    FREE(nodePages);
	    for (i = 0; i < page; i++) FREE(lightNodePages[i]);
	    FREE(lightNodePages);
	    memOut = 1;
	    return;
	} else {
	    for (i = 0; i < maxPages; i++) {
		newNodePages[i] = nodePages[i];
	    }
	    FREE(nodePages);
	    nodePages = newNodePages;
	}

	newNodePages = ALLOC(int *,maxPages + INITIAL_PAGES);
	if (newNodePages == NULL) {
	    for (i = 0; i < page; i++) FREE(nodePages[i]);
	    FREE(nodePages);
	    for (i = 0; i < page; i++) FREE(lightNodePages[i]);
	    FREE(lightNodePages);
	    memOut = 1;
	    return;
	} else {
	    for (i = 0; i < maxPages; i++) {
		newNodePages[i] = lightNodePages[i];
	    }
	    FREE(lightNodePages);
	    lightNodePages = newNodePages;
	}
	/* Increase total page count */
	maxPages += INITIAL_PAGES;
    }
    /* Allocate a new page */
    currentNodePage = nodePages[page] = ALLOC(int,pageSize);
    if (currentNodePage == NULL) {
	for (i = 0; i < page; i++) FREE(nodePages[i]);
	FREE(nodePages);
	for (i = 0; i < page; i++) FREE(lightNodePages[i]);
	FREE(lightNodePages);
	memOut = 1;
	return;
    }
    /* Allocate a new page */
    currentLightNodePage = lightNodePages[page] = ALLOC(int,pageSize);
    if (currentLightNodePage == NULL) {
	for (i = 0; i <= page; i++) FREE(nodePages[i]);
	FREE(nodePages);
	for (i = 0; i < page; i++) FREE(lightNodePages[i]);
	FREE(lightNodePages);
	memOut = 1;
	return;
    }
    /* reset page index */
    pageIndex = 0;
    return;

} /* end of ResizeCountNodePages */


/**Function********************************************************************

  Synopsis    [Recursively counts minterms of each node in the DAG.]

  Description [Recursively counts minterms of each node in the DAG.
  Similar to the cuddCountMintermAux which recursively counts the
  number of minterms for the dag rooted at each node in terms of the
  total number of variables (max). This procedure creates the node
  data structure and stores the minterm count as part of the node
  data structure. ]

  SideEffects [Creates structures of type node quality and fills the st_table]

  SeeAlso     [SubsetCountMinterm]

******************************************************************************/
static double
SubsetCountMintermAux(
  DdNode * node /* function to analyze */,
  double  max /* number of minterms of constant 1 */,
  st_table * table /* visitedTable table */)
{

    DdNode	*N,*Nv,*Nnv; /* nodes to store cofactors  */
    double	min,*pmin; /* minterm count */
    double	min1, min2; /* minterm count */
    NodeData_t *dummy;
    NodeData_t *newEntry;
    int i;

#ifdef DEBUG
    num_calls++;
#endif

    /* Constant case */
    if (Cudd_IsConstant(node)) {
	if (node == zero) {
	    return(0.0);
	} else {
	    return(max);
	}
    } else {

	/* check if entry for this node exists */
	if (st_lookup(table, node, &dummy)) {
	    min = *(dummy->mintermPointer);
	    return(min);
	}

	/* Make the node regular to extract cofactors */
	N = Cudd_Regular(node);

	/* store the cofactors */
	Nv = Cudd_T(N);
	Nnv = Cudd_E(N);

	Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
	Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

	min1 =  SubsetCountMintermAux(Nv, max,table)/2.0;
	if (memOut) return(0.0);
	min2 =  SubsetCountMintermAux(Nnv,max,table)/2.0;
	if (memOut) return(0.0);
	min = (min1+min2);

	/* if page index is at the bottom, then create a new page */
	if (pageIndex == pageSize) ResizeCountMintermPages();
	if (memOut) {
	    for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    st_free_table(table);
	    return(0.0);
	}

	/* point to the correct location in the page */
	pmin = currentMintermPage+pageIndex;
	pageIndex++;

	/* store the minterm count of this node in the page */
	*pmin = min;

	/* Note I allocate the struct here. Freeing taken care of later */
	if (nodeDataPageIndex == nodeDataPageSize) ResizeNodeDataPages();
	if (memOut) {
	    for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    st_free_table(table);
	    return(0.0);
	}

	newEntry = currentNodeDataPage + nodeDataPageIndex;
	nodeDataPageIndex++;

	/* points to the correct location in the page */
	newEntry->mintermPointer = pmin;
	/* initialize this field of the Node Quality structure */
	newEntry->nodesPointer = NULL;

	/* insert entry for the node in the table */
	if (st_insert(table,node, newEntry) == ST_OUT_OF_MEM) {
	    memOut = 1;
	    for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    st_free_table(table);
	    return(0.0);
	}
	return(min);
    }

} /* end of SubsetCountMintermAux */


/**Function********************************************************************

  Synopsis    [Counts minterms of each node in the DAG]

  Description [Counts minterms of each node in the DAG. Similar to the
  Cudd_CountMinterm procedure except this returns the minterm count for
  all the nodes in the bdd in an st_table.]

  SideEffects [none]

  SeeAlso     [SubsetCountMintermAux]

******************************************************************************/
static st_table *
SubsetCountMinterm(
  DdNode * node /* function to be analyzed */,
  int nvars /* number of variables node depends on */)
{
    st_table	*table;
    int i;


#ifdef DEBUG
    num_calls = 0;
#endif

    max = pow(2.0,(double) nvars);
    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) goto OUT_OF_MEM;
    maxPages = INITIAL_PAGES;
    mintermPages = ALLOC(double *,maxPages);
    if (mintermPages == NULL) {
	st_free_table(table);
	goto OUT_OF_MEM;
    }
    page = 0;
    currentMintermPage = ALLOC(double,pageSize);
    mintermPages[page] = currentMintermPage;
    if (currentMintermPage == NULL) {
	FREE(mintermPages);
	st_free_table(table);
	goto OUT_OF_MEM;
    }
    pageIndex = 0;
    maxNodeDataPages = INITIAL_PAGES;
    nodeDataPages = ALLOC(NodeData_t *, maxNodeDataPages);
    if (nodeDataPages == NULL) {
	for (i = 0; i <= page ; i++) FREE(mintermPages[i]);
	FREE(mintermPages);
	st_free_table(table);
	goto OUT_OF_MEM;
    }
    nodeDataPage = 0;
    currentNodeDataPage = ALLOC(NodeData_t ,nodeDataPageSize);
    nodeDataPages[nodeDataPage] = currentNodeDataPage;
    if (currentNodeDataPage == NULL) {
	for (i = 0; i <= page ; i++) FREE(mintermPages[i]);
	FREE(mintermPages);
	FREE(nodeDataPages);
	st_free_table(table);
	goto OUT_OF_MEM;
    }
    nodeDataPageIndex = 0;

    (void) SubsetCountMintermAux(node,max,table);
    if (memOut) goto OUT_OF_MEM;
    return(table);

OUT_OF_MEM:
    memOut = 1;
    return(NULL);

} /* end of SubsetCountMinterm */


/**Function********************************************************************

  Synopsis    [Recursively counts the number of nodes under the dag.
  Also counts the number of nodes under the lighter child of
  this node.]

  Description [Recursively counts the number of nodes under the dag.
  Also counts the number of nodes under the lighter child of
  this node. . Note that the same dag may be the lighter child of two
  different nodes and have different counts. As with the minterm counts,
  the node counts are stored in pages to be space efficient and the
  address for these node counts are stored in an st_table associated
  to each node. ]

  SideEffects [Updates the node data table with node counts]

  SeeAlso     [SubsetCountNodes]

******************************************************************************/
static int
SubsetCountNodesAux(
  DdNode * node /* current node */,
  st_table * table /* table to update node count, also serves as visited table. */,
  double  max /* maximum number of variables */)
{
    int tval, eval, i;
    DdNode *N, *Nv, *Nnv;
    double minNv, minNnv;
    NodeData_t *dummyN, *dummyNv, *dummyNnv, *dummyNBar;
    int *pmin, *pminBar, *val;

    if ((node == NULL) || Cudd_IsConstant(node))
	return(0);

    /* if this node has been processed do nothing */
    if (st_lookup(table, node, &dummyN) == 1) {
	val = dummyN->nodesPointer;
	if (val != NULL)
	    return(0);
    } else {
	return(0);
    }

    N  = Cudd_Regular(node);
    Nv = Cudd_T(N);
    Nnv = Cudd_E(N);

    Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
    Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

    /* find the minterm counts for the THEN and ELSE branches */
    if (Cudd_IsConstant(Nv)) {
	if (Nv == zero) {
	    minNv = 0.0;
	} else {
	    minNv = max;
	}
    } else {
	if (st_lookup(table, Nv, &dummyNv) == 1)
	    minNv = *(dummyNv->mintermPointer);
	else {
	    return(0);
	}
    }
    if (Cudd_IsConstant(Nnv)) {
	if (Nnv == zero) {
	    minNnv = 0.0;
	} else {
	    minNnv = max;
	}
    } else {
	if (st_lookup(table, Nnv, &dummyNnv) == 1) {
	    minNnv = *(dummyNnv->mintermPointer);
	}
	else {
	    return(0);
	}
    }


    /* recur based on which has larger minterm, */
    if (minNv >= minNnv) {
	tval = SubsetCountNodesAux(Nv, table, max);
	if (memOut) return(0);
	eval = SubsetCountNodesAux(Nnv, table, max);
	if (memOut) return(0);

	/* store the node count of the lighter child. */
	if (pageIndex == pageSize) ResizeCountNodePages();
	if (memOut) {
	    for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    st_free_table(table);
	    return(0);
	}
	pmin = currentLightNodePage + pageIndex;
	*pmin = eval; /* Here the ELSE child is lighter */
	dummyN->lightChildNodesPointer = pmin;

    } else {
	eval = SubsetCountNodesAux(Nnv, table, max);
	if (memOut) return(0);
	tval = SubsetCountNodesAux(Nv, table, max);
	if (memOut) return(0);

	/* store the node count of the lighter child. */
	if (pageIndex == pageSize) ResizeCountNodePages();
	if (memOut) {
	    for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    st_free_table(table);
	    return(0);
	}
	pmin = currentLightNodePage + pageIndex;
	*pmin = tval; /* Here the THEN child is lighter */
	dummyN->lightChildNodesPointer = pmin;

    }
    /* updating the page index for node count storage. */
    pmin = currentNodePage + pageIndex;
    *pmin = tval + eval + 1;
    dummyN->nodesPointer = pmin;

    /* pageIndex is parallel page index for count_nodes and count_lightNodes */
    pageIndex++;

    /* if this node has been reached first, it belongs to a heavier
       branch. Its complement will be reached later on a lighter branch.
       Hence the complement has zero node count. */

    if (st_lookup(table, Cudd_Not(node), &dummyNBar) == 1)  {
	if (pageIndex == pageSize) ResizeCountNodePages();
	if (memOut) {
	    for (i = 0; i < page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    for (i = 0; i < nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    st_free_table(table);
	    return(0);
	}
	pminBar = currentLightNodePage + pageIndex;
	*pminBar = 0;
	dummyNBar->lightChildNodesPointer = pminBar;
	/* The lighter child has less nodes than the parent.
	 * So if parent 0 then lighter child zero
	 */
	if (pageIndex == pageSize) ResizeCountNodePages();
	if (memOut) {
	    for (i = 0; i < page; i++) FREE(mintermPages[i]);
	    FREE(mintermPages);
	    for (i = 0; i < nodeDataPage; i++) FREE(nodeDataPages[i]);
	    FREE(nodeDataPages);
	    st_free_table(table);
	    return(0);
	}
	pminBar = currentNodePage + pageIndex;
	*pminBar = 0;
	dummyNBar->nodesPointer = pminBar ; /* maybe should point to zero */

	pageIndex++;
    }
    return(*pmin);
} /*end of SubsetCountNodesAux */


/**Function********************************************************************

  Synopsis    [Counts the nodes under the current node and its lighter child]

  Description [Counts the nodes under the current node and its lighter
  child. Calls a recursive procedure to count the number of nodes of
  a DAG rooted at a particular node and the number of nodes taken by its
  lighter child.]

  SideEffects [None]

  SeeAlso     [SubsetCountNodesAux]

******************************************************************************/
static int
SubsetCountNodes(
  DdNode * node /* function to be analyzed */,
  st_table * table /* node quality table */,
  int  nvars /* number of variables node depends on */)
{
    int	num;
    int i;

#ifdef DEBUG
    num_calls = 0;
#endif

    max = pow(2.0,(double) nvars);
    maxPages = INITIAL_PAGES;
    nodePages = ALLOC(int *,maxPages);
    if (nodePages == NULL)  {
	goto OUT_OF_MEM;
    }

    lightNodePages = ALLOC(int *,maxPages);
    if (lightNodePages == NULL) {
	for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	FREE(mintermPages);
	for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	FREE(nodeDataPages);
	FREE(nodePages);
	goto OUT_OF_MEM;
    }

    page = 0;
    currentNodePage = nodePages[page] = ALLOC(int,pageSize);
    if (currentNodePage == NULL) {
	for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	FREE(mintermPages);
	for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	FREE(nodeDataPages);
	FREE(lightNodePages);
	FREE(nodePages);
	goto OUT_OF_MEM;
    }

    currentLightNodePage = lightNodePages[page] = ALLOC(int,pageSize);
    if (currentLightNodePage == NULL) {
	for (i = 0; i <= page; i++) FREE(mintermPages[i]);
	FREE(mintermPages);
	for (i = 0; i <= nodeDataPage; i++) FREE(nodeDataPages[i]);
	FREE(nodeDataPages);
	FREE(currentNodePage);
	FREE(lightNodePages);
	FREE(nodePages);
	goto OUT_OF_MEM;
    }

    pageIndex = 0;
    num = SubsetCountNodesAux(node,table,max);
    if (memOut) goto OUT_OF_MEM;
    return(num);

OUT_OF_MEM:
    memOut = 1;
    return(0);

} /* end of SubsetCountNodes */


/**Function********************************************************************

  Synopsis    [Procedure to recursively store nodes that are retained in the subset.]

  Description [rocedure to recursively store nodes that are retained in the subset.]

  SideEffects [None]

  SeeAlso     [StoreNodes]

******************************************************************************/
static void
StoreNodes(
  st_table * storeTable,
  DdManager * dd,
  DdNode * node)
{
    DdNode *N, *Nt, *Ne;
    if (Cudd_IsConstant(dd)) {
	return;
    }
    N = Cudd_Regular(node);
    if (st_lookup(storeTable, N, NULL)) {
	return;
    }
    cuddRef(N);
    if (st_insert(storeTable, N, NULL) == ST_OUT_OF_MEM) {
	fprintf(dd->err,"Something wrong, st_table insert failed\n");
    }

    Nt = Cudd_T(N);
    Ne = Cudd_E(N);

    StoreNodes(storeTable, dd, Nt);
    StoreNodes(storeTable, dd, Ne);
    return;

}


/**Function********************************************************************

  Synopsis    [Builds the subset BDD using the heavy branch method.]

  Description [The procedure carries out the building of the subset BDD
  starting at the root. Using the three different counts labelling each node,
  the procedure chooses the heavier branch starting from the root and keeps
  track of the number of nodes it discards at each step, thus keeping count
  of the size of the subset BDD dynamically. Once the threshold is satisfied,
  the procedure then calls ITE to build the BDD.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
BuildSubsetBdd(
  DdManager * dd /* DD manager */,
  DdNode * node /* current node */,
  int * size /* current size of the subset */,
  st_table * visitedTable /* visited table storing all node data */,
  int  threshold,
  st_table * storeTable,
  st_table * approxTable)
{

    DdNode *Nv, *Nnv, *N, *topv, *neW;
    double minNv, minNnv;
    NodeData_t *currNodeQual;
    NodeData_t *currNodeQualT;
    NodeData_t *currNodeQualE;
    DdNode *ThenBranch, *ElseBranch;
    unsigned int topid;
    char *dummy;

#ifdef DEBUG
    num_calls++;
#endif
    /*If the size of the subset is below the threshold, dont do
      anything. */
    if ((*size) <= threshold) {
      /* store nodes below this, so we can recombine if possible */
      StoreNodes(storeTable, dd, node);
      return(node);
    }

    if (Cudd_IsConstant(node))
	return(node);

    /* Look up minterm count for this node. */
    if (!st_lookup(visitedTable, node, &currNodeQual)) {
	fprintf(dd->err,
		"Something is wrong, ought to be in node quality table\n");
    }

    /* Get children. */
    N = Cudd_Regular(node);
    Nv = Cudd_T(N);
    Nnv = Cudd_E(N);

    /* complement if necessary */
    Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
    Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

    if (!Cudd_IsConstant(Nv)) {
	/* find out minterms and nodes contributed by then child */
	if (!st_lookup(visitedTable, Nv, &currNodeQualT)) {
		fprintf(dd->out,"Something wrong, couldnt find nodes in node quality table\n");
		dd->errorCode = CUDD_INTERNAL_ERROR;
		return(NULL);
	    }
	else {
	    minNv = *(((NodeData_t *)currNodeQualT)->mintermPointer);
	}
    } else {
	if (Nv == zero) {
	    minNv = 0;
	} else  {
	    minNv = max;
	}
    }
    if (!Cudd_IsConstant(Nnv)) {
	/* find out minterms and nodes contributed by else child */
	if (!st_lookup(visitedTable, Nnv, &currNodeQualE)) {
	    fprintf(dd->out,"Something wrong, couldnt find nodes in node quality table\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
	} else {
	    minNnv = *(((NodeData_t *)currNodeQualE)->mintermPointer);
	}
    } else {
	if (Nnv == zero) {
	    minNnv = 0;
	} else {
	    minNnv = max;
	}
    }

    /* keep track of size of subset by subtracting the number of
     * differential nodes contributed by lighter child
     */
    *size = (*(size)) - (int)*(currNodeQual->lightChildNodesPointer);
    if (minNv >= minNnv) { /*SubsetCountNodesAux procedure takes
			     the Then branch in case of a tie */

	/* recur with the Then branch */
	ThenBranch = (DdNode *)BuildSubsetBdd(dd, Nv, size,
	      visitedTable, threshold, storeTable, approxTable);
	if (ThenBranch == NULL) {
	    return(NULL);
	}
	cuddRef(ThenBranch);
	/* The Else branch is either a node that already exists in the
	 * subset, or one whose approximation has been computed, or
	 * Zero.
	 */
	if (st_lookup(storeTable, Cudd_Regular(Nnv), &dummy)) {
	  ElseBranch = Nnv;
	  cuddRef(ElseBranch);
	} else {
	  if (st_lookup(approxTable, Nnv, &dummy)) {
	    ElseBranch = (DdNode *)dummy;
	    cuddRef(ElseBranch);
	  } else {
	    ElseBranch = zero;
	    cuddRef(ElseBranch);
	  }
	}

    }
    else {
	/* recur with the Else branch */
	ElseBranch = (DdNode *)BuildSubsetBdd(dd, Nnv, size,
		      visitedTable, threshold, storeTable, approxTable);
	if (ElseBranch == NULL) {
	    return(NULL);
	}
	cuddRef(ElseBranch);
	/* The Then branch is either a node that already exists in the
	 * subset, or one whose approximation has been computed, or
	 * Zero.
	 */
	if (st_lookup(storeTable, Cudd_Regular(Nv), &dummy)) {
	  ThenBranch = Nv;
	  cuddRef(ThenBranch);
	} else {
	  if (st_lookup(approxTable, Nv, &dummy)) {
	    ThenBranch = (DdNode *)dummy;
	    cuddRef(ThenBranch);
	  } else {
	    ThenBranch = zero;
	    cuddRef(ThenBranch);
	  }
	}
    }

    /* construct the Bdd with the top variable and the two children */
    topid = Cudd_NodeReadIndex(N);
    topv = Cudd_ReadVars(dd, topid);
    cuddRef(topv);
    neW =  cuddBddIteRecur(dd, topv, ThenBranch, ElseBranch);
    if (neW != NULL) {
      cuddRef(neW);
    }
    Cudd_RecursiveDeref(dd, topv);
    Cudd_RecursiveDeref(dd, ThenBranch);
    Cudd_RecursiveDeref(dd, ElseBranch);


    if (neW == NULL)
	return(NULL);
    else {
	/* store this node in the store table */
	if (!st_lookup(storeTable, Cudd_Regular(neW), &dummy)) {
	  cuddRef(neW);
	  if (st_insert(storeTable, Cudd_Regular(neW), NULL) ==
              ST_OUT_OF_MEM)
              return (NULL);
	}
	/* store the approximation for this node */
	if (N !=  Cudd_Regular(neW)) {
	    if (st_lookup(approxTable, node, &dummy)) {
		fprintf(dd->err, "This node should not be in the approximated table\n");
	    } else {
		cuddRef(neW);
		if (st_insert(approxTable, node, neW) ==
                    ST_OUT_OF_MEM)
		    return(NULL);
	    }
	}
	cuddDeref(neW);
	return(neW);
    }
} /* end of BuildSubsetBdd */
