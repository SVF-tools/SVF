/**CFile***********************************************************************

  FileName    [cuddApprox.c]

  PackageName [cudd]

  Synopsis    [Procedures to approximate a given BDD.]

  Description [External procedures provided by this module:
                <ul>
		<li> Cudd_UnderApprox()
		<li> Cudd_OverApprox()
		<li> Cudd_RemapUnderApprox()
		<li> Cudd_RemapOverApprox()
		<li> Cudd_BiasedUnderApprox()
		<li> Cudd_BiasedOverApprox()
		</ul>
	       Internal procedures included in this module:
		<ul>
		<li> cuddUnderApprox()
		<li> cuddRemapUnderApprox()
		<li> cuddBiasedUnderApprox()
		</ul>
	       Static procedures included in this module:
		<ul>
                <li> updateParity()
		<li> gatherInfoAux()
		<li> gatherInfo()
		<li> computeSavings()
		<li> updateRefs()
		<li> UAmarkNodes()
		<li> UAbuildSubset()
		<li> RAmarkNodes()
		<li> BAmarkNodes()
		<li> RAbuildSubset()
                <li> BAapplyBias()
		</ul>
		]

  SeeAlso     [cuddSubsetHB.c cuddSubsetSP.c cuddGenCof.c]

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

#define NOTHING		0
#define REPLACE_T	1
#define REPLACE_E	2
#define REPLACE_N	3
#define REPLACE_TT	4
#define REPLACE_TE	5

#define DONT_CARE	0
#define CARE		1
#define TOTAL_CARE	2
#define CARE_ERROR	3

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/* Data structure to store the information on each node. It keeps the
** number of minterms of the function rooted at this node in terms of
** the number of variables specified by the user; the number of
** minterms of the complement; the impact of the number of minterms of
** this function on the number of minterms of the root function; the
** reference count of the node from within the root function; the
** flag that says whether the node intersects the care set; the flag
** that says whether the node should be replaced and how; the results
** of subsetting in both phases. */
typedef struct NodeData {
    double mintermsP;		/* minterms for the regular node */
    double mintermsN;		/* minterms for the complemented node */
    int functionRef;		/* references from within this function */
    char care;			/* node intersects care set */
    char replace;		/* replacement decision */
    short int parity;		/* 1: even; 2: odd; 3: both */
    DdNode *resultP;		/* result for even parity */
    DdNode *resultN;		/* result for odd parity */
} NodeData;

typedef struct ApproxInfo {
    DdNode *one;		/* one constant */
    DdNode *zero;		/* BDD zero constant */
    NodeData *page;		/* per-node information */
    DdHashTable *table;		/* hash table to access the per-node info */
    int index;			/* index of the current node */
    double max;			/* max number of minterms */
    int size;			/* how many nodes are left */
    double minterms;		/* how many minterms are left */
} ApproxInfo;

/* Item of the queue used in the levelized traversal of the BDD. */
typedef struct GlobalQueueItem {
    struct GlobalQueueItem *next;
    struct GlobalQueueItem *cnext;
    DdNode *node;
    double impactP;
    double impactN;
} GlobalQueueItem;
 
typedef struct LocalQueueItem {
    struct LocalQueueItem *next;
    struct LocalQueueItem *cnext;
    DdNode *node;
    int localRef;
} LocalQueueItem;

    
/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddApprox.c,v 1.31 2012/02/05 04:38:07 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void updateParity (DdNode *node, ApproxInfo *info, int newparity);
static NodeData * gatherInfoAux (DdNode *node, ApproxInfo *info, int parity);
static ApproxInfo * gatherInfo (DdManager *dd, DdNode *node, int numVars, int parity);
static int computeSavings (DdManager *dd, DdNode *f, DdNode *skip, ApproxInfo *info, DdLevelQueue *queue);
static int updateRefs (DdManager *dd, DdNode *f, DdNode *skip, ApproxInfo *info, DdLevelQueue *queue);
static int UAmarkNodes (DdManager *dd, DdNode *f, ApproxInfo *info, int threshold, int safe, double quality);
static DdNode * UAbuildSubset (DdManager *dd, DdNode *node, ApproxInfo *info);
static int RAmarkNodes (DdManager *dd, DdNode *f, ApproxInfo *info, int threshold, double quality);
static int BAmarkNodes (DdManager *dd, DdNode *f, ApproxInfo *info, int threshold, double quality1, double quality0);
static DdNode * RAbuildSubset (DdManager *dd, DdNode *node, ApproxInfo *info);
static int BAapplyBias (DdManager *dd, DdNode *f, DdNode *b, ApproxInfo *info, DdHashTable *cache);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Extracts a dense subset from a BDD with Shiple's
  underapproximation method.]

  Description [Extracts a dense subset from a BDD. This procedure uses
  a variant of Tom Shiple's underapproximation method. The main
  difference from the original method is that density is used as cost
  function.  Returns a pointer to the BDD of the subset if
  successful. NULL if the procedure runs out of memory. The parameter
  numVars is the maximum number of variables to be used in minterm
  calculation.  The optimal number should be as close as possible to
  the size of the support of f.  However, it is safe to pass the value
  returned by Cudd_ReadSize for numVars when the number of variables
  is under 1023.  If numVars is larger than 1023, it will cause
  overflow. If a 0 parameter is passed then the procedure will compute
  a value which will avoid overflow but will cause underflow with 2046
  variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetShortPaths Cudd_SubsetHeavyBranch Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_UnderApprox(
  DdManager * dd /* manager */,
  DdNode * f /* function to be subset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* when to stop approximation */,
  int  safe /* enforce safe approximation */,
  double  quality /* minimum improvement for accepted changes */)
{
    DdNode *subset;

    do {
	dd->reordered = 0;
	subset = cuddUnderApprox(dd, f, numVars, threshold, safe, quality);
    } while (dd->reordered == 1);

    return(subset);

} /* end of Cudd_UnderApprox */


/**Function********************************************************************

  Synopsis    [Extracts a dense superset from a BDD with Shiple's
  underapproximation method.]

  Description [Extracts a dense superset from a BDD. The procedure is
  identical to the underapproximation procedure except for the fact that it
  works on the complement of the given function. Extracting the subset
  of the complement function is equivalent to extracting the superset
  of the function.
  Returns a pointer to the BDD of the superset if successful. NULL if
  intermediate result causes the procedure to run out of memory. The
  parameter numVars is the maximum number of variables to be used in
  minterm calculation.  The optimal number
  should be as close as possible to the size of the support of f.
  However, it is safe to pass the value returned by Cudd_ReadSize for
  numVars when the number of variables is under 1023.  If numVars is
  larger than 1023, it will overflow. If a 0 parameter is passed then
  the procedure will compute a value which will avoid overflow but
  will cause underflow with 2046 variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SupersetHeavyBranch Cudd_SupersetShortPaths Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_OverApprox(
  DdManager * dd /* manager */,
  DdNode * f /* function to be superset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* when to stop approximation */,
  int  safe /* enforce safe approximation */,
  double  quality /* minimum improvement for accepted changes */)
{
    DdNode *subset, *g;

    g = Cudd_Not(f);    
    do {
	dd->reordered = 0;
	subset = cuddUnderApprox(dd, g, numVars, threshold, safe, quality);
    } while (dd->reordered == 1);
    
    return(Cudd_NotCond(subset, (subset != NULL)));
    
} /* end of Cudd_OverApprox */


/**Function********************************************************************

  Synopsis [Extracts a dense subset from a BDD with the remapping
  underapproximation method.]

  Description [Extracts a dense subset from a BDD. This procedure uses
  a remapping technique and density as the cost function.
  Returns a pointer to the BDD of the subset if
  successful. NULL if the procedure runs out of memory. The parameter
  numVars is the maximum number of variables to be used in minterm
  calculation.  The optimal number should be as close as possible to
  the size of the support of f.  However, it is safe to pass the value
  returned by Cudd_ReadSize for numVars when the number of variables
  is under 1023.  If numVars is larger than 1023, it will cause
  overflow. If a 0 parameter is passed then the procedure will compute
  a value which will avoid overflow but will cause underflow with 2046
  variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetShortPaths Cudd_SubsetHeavyBranch Cudd_UnderApprox Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_RemapUnderApprox(
  DdManager * dd /* manager */,
  DdNode * f /* function to be subset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* when to stop approximation */,
  double  quality /* minimum improvement for accepted changes */)
{
    DdNode *subset;

    do {
	dd->reordered = 0;
	subset = cuddRemapUnderApprox(dd, f, numVars, threshold, quality);
    } while (dd->reordered == 1);

    return(subset);

} /* end of Cudd_RemapUnderApprox */


/**Function********************************************************************

  Synopsis    [Extracts a dense superset from a BDD with the remapping
  underapproximation method.]

  Description [Extracts a dense superset from a BDD. The procedure is
  identical to the underapproximation procedure except for the fact that it
  works on the complement of the given function. Extracting the subset
  of the complement function is equivalent to extracting the superset
  of the function.
  Returns a pointer to the BDD of the superset if successful. NULL if
  intermediate result causes the procedure to run out of memory. The
  parameter numVars is the maximum number of variables to be used in
  minterm calculation.  The optimal number
  should be as close as possible to the size of the support of f.
  However, it is safe to pass the value returned by Cudd_ReadSize for
  numVars when the number of variables is under 1023.  If numVars is
  larger than 1023, it will overflow. If a 0 parameter is passed then
  the procedure will compute a value which will avoid overflow but
  will cause underflow with 2046 variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SupersetHeavyBranch Cudd_SupersetShortPaths Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_RemapOverApprox(
  DdManager * dd /* manager */,
  DdNode * f /* function to be superset */,
  int  numVars /* number of variables in the support of f */,
  int  threshold /* when to stop approximation */,
  double  quality /* minimum improvement for accepted changes */)
{
    DdNode *subset, *g;

    g = Cudd_Not(f);    
    do {
	dd->reordered = 0;
	subset = cuddRemapUnderApprox(dd, g, numVars, threshold, quality);
    } while (dd->reordered == 1);
    
    return(Cudd_NotCond(subset, (subset != NULL)));
    
} /* end of Cudd_RemapOverApprox */


/**Function********************************************************************

  Synopsis [Extracts a dense subset from a BDD with the biased
  underapproximation method.]

  Description [Extracts a dense subset from a BDD. This procedure uses
  a biased remapping technique and density as the cost function. The bias
  is a function. This procedure tries to approximate where the bias is 0
  and preserve the given function where the bias is 1.
  Returns a pointer to the BDD of the subset if
  successful. NULL if the procedure runs out of memory. The parameter
  numVars is the maximum number of variables to be used in minterm
  calculation.  The optimal number should be as close as possible to
  the size of the support of f.  However, it is safe to pass the value
  returned by Cudd_ReadSize for numVars when the number of variables
  is under 1023.  If numVars is larger than 1023, it will cause
  overflow. If a 0 parameter is passed then the procedure will compute
  a value which will avoid overflow but will cause underflow with 2046
  variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SubsetShortPaths Cudd_SubsetHeavyBranch Cudd_UnderApprox
  Cudd_RemapUnderApprox Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_BiasedUnderApprox(
  DdManager *dd /* manager */,
  DdNode *f /* function to be subset */,
  DdNode *b /* bias function */,
  int numVars /* number of variables in the support of f */,
  int threshold /* when to stop approximation */,
  double quality1 /* minimum improvement for accepted changes when b=1 */,
  double quality0 /* minimum improvement for accepted changes when b=0 */)
{
    DdNode *subset;

    do {
	dd->reordered = 0;
	subset = cuddBiasedUnderApprox(dd, f, b, numVars, threshold, quality1,
				       quality0);
    } while (dd->reordered == 1);

    return(subset);

} /* end of Cudd_BiasedUnderApprox */


/**Function********************************************************************

  Synopsis    [Extracts a dense superset from a BDD with the biased
  underapproximation method.]

  Description [Extracts a dense superset from a BDD. The procedure is
  identical to the underapproximation procedure except for the fact that it
  works on the complement of the given function. Extracting the subset
  of the complement function is equivalent to extracting the superset
  of the function.
  Returns a pointer to the BDD of the superset if successful. NULL if
  intermediate result causes the procedure to run out of memory. The
  parameter numVars is the maximum number of variables to be used in
  minterm calculation.  The optimal number
  should be as close as possible to the size of the support of f.
  However, it is safe to pass the value returned by Cudd_ReadSize for
  numVars when the number of variables is under 1023.  If numVars is
  larger than 1023, it will overflow. If a 0 parameter is passed then
  the procedure will compute a value which will avoid overflow but
  will cause underflow with 2046 variables or more.]

  SideEffects [None]

  SeeAlso     [Cudd_SupersetHeavyBranch Cudd_SupersetShortPaths
  Cudd_RemapOverApprox Cudd_BiasedUnderApprox Cudd_ReadSize]

******************************************************************************/
DdNode *
Cudd_BiasedOverApprox(
  DdManager *dd /* manager */,
  DdNode *f /* function to be superset */,
  DdNode *b /* bias function */,
  int numVars /* number of variables in the support of f */,
  int threshold /* when to stop approximation */,
  double quality1 /* minimum improvement for accepted changes when b=1*/,
  double quality0 /* minimum improvement for accepted changes when b=0 */)
{
    DdNode *subset, *g;

    g = Cudd_Not(f);    
    do {
	dd->reordered = 0;
	subset = cuddBiasedUnderApprox(dd, g, b, numVars, threshold, quality1,
				      quality0);
    } while (dd->reordered == 1);
    
    return(Cudd_NotCond(subset, (subset != NULL)));
    
} /* end of Cudd_BiasedOverApprox */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Applies Tom Shiple's underappoximation algorithm.]

  Description [Applies Tom Shiple's underappoximation algorithm. Proceeds
  in three phases:
  <ul>
  <li> collect information on each node in the BDD; this is done via DFS.
  <li> traverse the BDD in top-down fashion and compute for each node
  whether its elimination increases density.
  <li> traverse the BDD via DFS and actually perform the elimination.
  </ul>
  Returns the approximated BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_UnderApprox]

******************************************************************************/
DdNode *
cuddUnderApprox(
  DdManager * dd /* DD manager */,
  DdNode * f /* current DD */,
  int  numVars /* maximum number of variables */,
  int  threshold /* threshold under which approximation stops */,
  int  safe /* enforce safe approximation */,
  double  quality /* minimum improvement for accepted changes */)
{
    ApproxInfo *info;
    DdNode *subset;
    int result;

    if (f == NULL) {
	fprintf(dd->err, "Cannot subset, nil object\n");
	return(NULL);
    }

    if (Cudd_IsConstant(f)) {
	return(f);
    }

    /* Create table where node data are accessible via a hash table. */
    info = gatherInfo(dd, f, numVars, safe);
    if (info == NULL) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    /* Mark nodes that should be replaced by zero. */
    result = UAmarkNodes(dd, f, info, threshold, safe, quality);
    if (result == 0) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	FREE(info->page);
	cuddHashTableGenericQuit(info->table);
	FREE(info);
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    /* Build the result. */
    subset = UAbuildSubset(dd, f, info);
#if 1
    if (subset && info->size < Cudd_DagSize(subset))
	(void) fprintf(dd->err, "Wrong prediction: %d versus actual %d\n",
		       info->size, Cudd_DagSize(subset));
#endif
    FREE(info->page);
    cuddHashTableGenericQuit(info->table);
    FREE(info);

#ifdef DD_DEBUG
    if (subset != NULL) {
	cuddRef(subset);
#if 0
	(void) Cudd_DebugCheck(dd);
	(void) Cudd_CheckKeys(dd);
#endif
	if (!Cudd_bddLeq(dd, subset, f)) {
	    (void) fprintf(dd->err, "Wrong subset\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	}
	cuddDeref(subset);
    }
#endif
    return(subset);

} /* end of cuddUnderApprox */


/**Function********************************************************************

  Synopsis    [Applies the remapping underappoximation algorithm.]

  Description [Applies the remapping underappoximation algorithm.
  Proceeds in three phases:
  <ul>
  <li> collect information on each node in the BDD; this is done via DFS.
  <li> traverse the BDD in top-down fashion and compute for each node
  whether remapping increases density.
  <li> traverse the BDD via DFS and actually perform the elimination.
  </ul>
  Returns the approximated BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_RemapUnderApprox]

******************************************************************************/
DdNode *
cuddRemapUnderApprox(
  DdManager * dd /* DD manager */,
  DdNode * f /* current DD */,
  int  numVars /* maximum number of variables */,
  int  threshold /* threshold under which approximation stops */,
  double  quality /* minimum improvement for accepted changes */)
{
    ApproxInfo *info;
    DdNode *subset;
    int result;

    if (f == NULL) {
	fprintf(dd->err, "Cannot subset, nil object\n");
	dd->errorCode = CUDD_INVALID_ARG;
	return(NULL);
    }

    if (Cudd_IsConstant(f)) {
	return(f);
    }

    /* Create table where node data are accessible via a hash table. */
    info = gatherInfo(dd, f, numVars, CUDD_TRUE);
    if (info == NULL) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    /* Mark nodes that should be replaced by zero. */
    result = RAmarkNodes(dd, f, info, threshold, quality);
    if (result == 0) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	FREE(info->page);
	cuddHashTableGenericQuit(info->table);
	FREE(info);
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    /* Build the result. */
    subset = RAbuildSubset(dd, f, info);
#if 1
    if (subset && info->size < Cudd_DagSize(subset))
	(void) fprintf(dd->err, "Wrong prediction: %d versus actual %d\n",
		       info->size, Cudd_DagSize(subset));
#endif
    FREE(info->page);
    cuddHashTableGenericQuit(info->table);
    FREE(info);

#ifdef DD_DEBUG
    if (subset != NULL) {
	cuddRef(subset);
#if 0
	(void) Cudd_DebugCheck(dd);
	(void) Cudd_CheckKeys(dd);
#endif
	if (!Cudd_bddLeq(dd, subset, f)) {
	    (void) fprintf(dd->err, "Wrong subset\n");
	}
	cuddDeref(subset);
	dd->errorCode = CUDD_INTERNAL_ERROR;
    }
#endif
    return(subset);

} /* end of cuddRemapUnderApprox */


/**Function********************************************************************

  Synopsis    [Applies the biased remapping underappoximation algorithm.]

  Description [Applies the biased remapping underappoximation algorithm.
  Proceeds in three phases:
  <ul>
  <li> collect information on each node in the BDD; this is done via DFS.
  <li> traverse the BDD in top-down fashion and compute for each node
  whether remapping increases density.
  <li> traverse the BDD via DFS and actually perform the elimination.
  </ul>
  Returns the approximated BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_BiasedUnderApprox]

******************************************************************************/
DdNode *
cuddBiasedUnderApprox(
  DdManager *dd /* DD manager */,
  DdNode *f /* current DD */,
  DdNode *b /* bias function */,
  int numVars /* maximum number of variables */,
  int threshold /* threshold under which approximation stops */,
  double quality1 /* minimum improvement for accepted changes when b=1 */,
  double quality0 /* minimum improvement for accepted changes when b=0 */)
{
    ApproxInfo *info;
    DdNode *subset;
    int result;
    DdHashTable	*cache;

    if (f == NULL) {
	fprintf(dd->err, "Cannot subset, nil object\n");
	dd->errorCode = CUDD_INVALID_ARG;
	return(NULL);
    }

    if (Cudd_IsConstant(f)) {
	return(f);
    }

    /* Create table where node data are accessible via a hash table. */
    info = gatherInfo(dd, f, numVars, CUDD_TRUE);
    if (info == NULL) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    cache = cuddHashTableInit(dd,2,2);
    result = BAapplyBias(dd, Cudd_Regular(f), b, info, cache);
    if (result == CARE_ERROR) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	cuddHashTableQuit(cache);
	FREE(info->page);
	cuddHashTableGenericQuit(info->table);
	FREE(info);
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    cuddHashTableQuit(cache);

    /* Mark nodes that should be replaced by zero. */
    result = BAmarkNodes(dd, f, info, threshold, quality1, quality0);
    if (result == 0) {
	(void) fprintf(dd->err, "Out-of-memory; Cannot subset\n");
	FREE(info->page);
	cuddHashTableGenericQuit(info->table);
	FREE(info);
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    /* Build the result. */
    subset = RAbuildSubset(dd, f, info);
#if 1
    if (subset && info->size < Cudd_DagSize(subset))
	(void) fprintf(dd->err, "Wrong prediction: %d versus actual %d\n",
		       info->size, Cudd_DagSize(subset));
#endif
    FREE(info->page);
    cuddHashTableGenericQuit(info->table);
    FREE(info);

#ifdef DD_DEBUG
    if (subset != NULL) {
	cuddRef(subset);
#if 0
	(void) Cudd_DebugCheck(dd);
	(void) Cudd_CheckKeys(dd);
#endif
	if (!Cudd_bddLeq(dd, subset, f)) {
	    (void) fprintf(dd->err, "Wrong subset\n");
	}
	cuddDeref(subset);
	dd->errorCode = CUDD_INTERNAL_ERROR;
    }
#endif
    return(subset);

} /* end of cuddBiasedUnderApprox */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Recursively update the parity of the paths reaching a node.]

  Description [Recursively update the parity of the paths reaching a node.
  Assumes that node is regular and propagates the invariant.]

  SideEffects [None]

  SeeAlso     [gatherInfoAux]

******************************************************************************/
static void
updateParity(
  DdNode * node /* function to analyze */,
  ApproxInfo * info /* info on BDD */,
  int newparity /* new parity for node */)
{
    NodeData *infoN;
    DdNode *E;

    if ((infoN = (NodeData *) cuddHashTableGenericLookup(info->table, node)) == NULL)
        return;
    if ((infoN->parity & newparity) != 0) return;
    infoN->parity |= (short) newparity;
    if (Cudd_IsConstant(node)) return;
    updateParity(cuddT(node),info,newparity);
    E = cuddE(node);
    if (Cudd_IsComplement(E)) {
	updateParity(Cudd_Not(E),info,3-newparity);
    } else {
	updateParity(E,info,newparity);
    }
    return;

} /* end of updateParity */


/**Function********************************************************************

  Synopsis    [Recursively counts minterms and computes reference counts
  of each node in the BDD.]

  Description [Recursively counts minterms and computes reference
  counts of each node in the BDD.  Similar to the cuddCountMintermAux
  which recursively counts the number of minterms for the dag rooted
  at each node in terms of the total number of variables (max). It assumes
  that the node pointer passed to it is regular and it maintains the
  invariant.]

  SideEffects [None]

  SeeAlso     [gatherInfo]

******************************************************************************/
static NodeData *
gatherInfoAux(
  DdNode * node /* function to analyze */,
  ApproxInfo * info /* info on BDD */,
  int parity /* gather parity information */)
{
    DdNode	*N, *Nt, *Ne;
    NodeData	*infoN, *infoT, *infoE;

    N = Cudd_Regular(node);

    /* Check whether entry for this node exists. */
    if ((infoN = (NodeData *) cuddHashTableGenericLookup(info->table, N)) != NULL) {
	if (parity) {
	    /* Update parity and propagate. */
	    updateParity(N, info, 1 +  (int) Cudd_IsComplement(node));
	}
	return(infoN);
    }

    /* Compute the cofactors. */
    Nt = Cudd_NotCond(cuddT(N), N != node);
    Ne = Cudd_NotCond(cuddE(N), N != node);

    infoT = gatherInfoAux(Nt, info, parity);
    if (infoT == NULL) return(NULL);
    infoE = gatherInfoAux(Ne, info, parity);
    if (infoE == NULL) return(NULL);

    infoT->functionRef++;
    infoE->functionRef++;

    /* Point to the correct location in the page. */
    infoN = &(info->page[info->index++]);
    infoN->parity |= (short) (1 + Cudd_IsComplement(node));

    infoN->mintermsP = infoT->mintermsP/2;
    infoN->mintermsN = infoT->mintermsN/2;
    if (Cudd_IsComplement(Ne) ^ Cudd_IsComplement(node)) {
	infoN->mintermsP += infoE->mintermsN/2;
	infoN->mintermsN += infoE->mintermsP/2;
    } else {
	infoN->mintermsP += infoE->mintermsP/2;
	infoN->mintermsN += infoE->mintermsN/2;
    }

    /* Insert entry for the node in the table. */
    if (cuddHashTableGenericInsert(info->table, N, infoN) == 0) {
	return(NULL);
    }
    return(infoN);

} /* end of gatherInfoAux */


/**Function********************************************************************

  Synopsis    [Gathers information about each node.]

  Description [Counts minterms and computes reference counts of each
  node in the BDD. The minterm count is separately computed for the
  node and its complement. This is to avoid cancellation
  errors. Returns a pointer to the data structure holding the
  information gathered if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddUnderApprox gatherInfoAux]

******************************************************************************/
static ApproxInfo *
gatherInfo(
  DdManager * dd /* manager */,
  DdNode * node /* function to be analyzed */,
  int numVars /* number of variables node depends on */,
  int parity /* gather parity information */)
{
    ApproxInfo * info;
    NodeData * infoTop;

    /* If user did not give numVars value, set it to the maximum
    ** exponent that the pow function can take. The -1 is due to the
    ** discrepancy in the value that pow takes and the value that
    ** log gives.
    */
    if (numVars == 0) {
	numVars = DBL_MAX_EXP - 1;
    }

    info = ALLOC(ApproxInfo,1);
    if (info == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    info->max = pow(2.0,(double) numVars);
    info->one = DD_ONE(dd);
    info->zero = Cudd_Not(info->one);
    info->size = Cudd_DagSize(node);
    /* All the information gathered will be stored in a contiguous
    ** piece of memory, which is allocated here. This can be done
    ** efficiently because we have counted the number of nodes of the
    ** BDD. info->index points to the next available entry in the array
    ** that stores the per-node information. */
    info->page = ALLOC(NodeData,info->size);
    if (info->page == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(info);
	return(NULL);
    }
    memset(info->page, 0, info->size * sizeof(NodeData)); /* clear all page */
    info->table = cuddHashTableInit(dd,1,info->size);
    if (info->table == NULL) {
	FREE(info->page);
	FREE(info);
	return(NULL);
    }
    /* We visit the DAG in post-order DFS. Hence, the constant node is
    ** in first position, and the root of the DAG is in last position. */

    /* Info for the constant node: Initialize only fields different from 0. */
    if (cuddHashTableGenericInsert(info->table, info->one, info->page) == 0) {
	FREE(info->page);
	FREE(info);
	cuddHashTableGenericQuit(info->table);
	return(NULL);
    }
    info->page[0].mintermsP = info->max;
    info->index = 1;

    infoTop = gatherInfoAux(node,info,parity);
    if (infoTop == NULL) {
	FREE(info->page);
	cuddHashTableGenericQuit(info->table);
	FREE(info);
	return(NULL);
    }
    if (Cudd_IsComplement(node)) {
	info->minterms = infoTop->mintermsN;
    } else {
	info->minterms = infoTop->mintermsP;
    }

    infoTop->functionRef = 1;
    return(info);

} /* end of gatherInfo */


/**Function********************************************************************

  Synopsis    [Counts the nodes that would be eliminated if a given node
  were replaced by zero.]

  Description [Counts the nodes that would be eliminated if a given
  node were replaced by zero. This procedure uses a queue passed by
  the caller for efficiency: since the queue is left empty at the
  endof the search, it can be reused as is by the next search. Returns
  the count (always striclty positive) if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [UAmarkNodes RAmarkNodes BAmarkNodes]

******************************************************************************/
static int
computeSavings(
  DdManager * dd,
  DdNode * f,
  DdNode * skip,
  ApproxInfo * info,
  DdLevelQueue * queue)
{
    NodeData *infoN;
    LocalQueueItem *item;
    DdNode *node;
    int savings = 0;

    node = Cudd_Regular(f);
    skip = Cudd_Regular(skip);
    /* Insert the given node in the level queue. Its local reference
    ** count is set equal to the function reference count so that the
    ** search will continue from it when it is retrieved. */
    item = (LocalQueueItem *)
	cuddLevelQueueFirst(queue,node,cuddI(dd,node->index));
    if (item == NULL)
	return(0);
    infoN = (NodeData *) cuddHashTableGenericLookup(info->table, node);
    item->localRef = infoN->functionRef;

    /* Process the queue. */
    while ((item = (LocalQueueItem *) queue->first) != NULL) {
	node = item->node;
	if (node != skip) {
            infoN = (NodeData *) cuddHashTableGenericLookup(info->table,node);
            if (item->localRef == infoN->functionRef) {
                /* This node is not shared. */
                DdNode *nodeT, *nodeE;
                savings++;
                nodeT = cuddT(node);
                if (!cuddIsConstant(nodeT)) {
                    item = (LocalQueueItem *)
                        cuddLevelQueueEnqueue(queue,nodeT,cuddI(dd,nodeT->index));
                    if (item == NULL) return(0);
                    item->localRef++;
                }
                nodeE = Cudd_Regular(cuddE(node));
                if (!cuddIsConstant(nodeE)) {
                    item = (LocalQueueItem *)
                        cuddLevelQueueEnqueue(queue,nodeE,cuddI(dd,nodeE->index));
                    if (item == NULL) return(0);
                    item->localRef++;
                }
            }
        }
	cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
    }

#ifdef DD_DEBUG
    /* At the end of a local search the queue should be empty. */
    assert(queue->size == 0);
#endif
    return(savings);

} /* end of computeSavings */


/**Function********************************************************************

  Synopsis    [Update function reference counts.]

  Description [Update function reference counts to account for replacement.
  Returns the number of nodes saved if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [UAmarkNodes RAmarkNodes BAmarkNodes]

******************************************************************************/
static int
updateRefs(
  DdManager * dd,
  DdNode * f,
  DdNode * skip,
  ApproxInfo * info,
  DdLevelQueue * queue)
{
    NodeData *infoN;
    LocalQueueItem *item;
    DdNode *node;
    int savings = 0;

    node = Cudd_Regular(f);
    /* Insert the given node in the level queue. Its function reference
    ** count is set equal to 0 so that the search will continue from it
    ** when it is retrieved. */
    item = (LocalQueueItem *) cuddLevelQueueFirst(queue,node,cuddI(dd,node->index));
    if (item == NULL)
	return(0);
    infoN = (NodeData *) cuddHashTableGenericLookup(info->table, node);
    infoN->functionRef = 0;

    if (skip != NULL) {
	/* Increase the function reference count of the node to be skipped
	** by 1 to account for the node pointing to it that will be created. */
	skip = Cudd_Regular(skip);
	infoN = (NodeData *) cuddHashTableGenericLookup(info->table, skip);
	infoN->functionRef++;
    }

    /* Process the queue. */
    while ((item = (LocalQueueItem *) queue->first) != NULL) {
	node = item->node;
	infoN = (NodeData *) cuddHashTableGenericLookup(info->table,node);
	if (infoN->functionRef == 0) {
	    /* This node is not shared or to be be skipped. */
            DdNode *nodeT, *nodeE;
            savings++;
            nodeT = cuddT(node);
            if (!cuddIsConstant(nodeT)) {
                item = (LocalQueueItem *)
                    cuddLevelQueueEnqueue(queue,nodeT,cuddI(dd,nodeT->index));
                if (item == NULL) return(0);
                infoN = (NodeData *) cuddHashTableGenericLookup(info->table,nodeT);
                infoN->functionRef--;
            }
            nodeE = Cudd_Regular(cuddE(node));
            if (!cuddIsConstant(nodeE)) {
                item = (LocalQueueItem *)
                    cuddLevelQueueEnqueue(queue,nodeE,cuddI(dd,nodeE->index));
                if (item == NULL) return(0);
                infoN = (NodeData *) cuddHashTableGenericLookup(info->table,nodeE);
                infoN->functionRef--;
            }
	}
	cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
    }

#ifdef DD_DEBUG
    /* At the end of a local search the queue should be empty. */
    assert(queue->size == 0);
#endif
    return(savings);

} /* end of updateRefs */


/**Function********************************************************************

  Synopsis    [Marks nodes for replacement by zero.]

  Description [Marks nodes for replacement by zero. Returns 1 if successful;
  0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddUnderApprox]

******************************************************************************/
static int
UAmarkNodes(
  DdManager * dd /* manager */,
  DdNode * f /* function to be analyzed */,
  ApproxInfo * info /* info on BDD */,
  int  threshold /* when to stop approximating */,
  int  safe /* enforce safe approximation */,
  double  quality /* minimum improvement for accepted changes */)
{
    DdLevelQueue *queue;
    DdLevelQueue *localQueue;
    NodeData *infoN;
    GlobalQueueItem *item;
    DdNode *node;
    double numOnset;
    double impactP, impactN;
    int savings;

#if 0
    (void) printf("initial size = %d initial minterms = %g\n",
		  info->size, info->minterms);
#endif
    queue = cuddLevelQueueInit(dd->size,sizeof(GlobalQueueItem),info->size);
    if (queue == NULL) {
	return(0);
    }
    localQueue = cuddLevelQueueInit(dd->size,sizeof(LocalQueueItem),
				    dd->initSlots);
    if (localQueue == NULL) {
	cuddLevelQueueQuit(queue);
	return(0);
    }
    node = Cudd_Regular(f);
    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,node,cuddI(dd,node->index));
    if (item == NULL) {
	cuddLevelQueueQuit(queue);
	cuddLevelQueueQuit(localQueue);
	return(0);
    }
    if (Cudd_IsComplement(f)) {
	item->impactP = 0.0;
	item->impactN = 1.0;
    } else {
	item->impactP = 1.0;
	item->impactN = 0.0;
    }
    while (queue->first != NULL) {
	/* If the size of the subset is below the threshold, quit. */
	if (info->size <= threshold)
	    break;
	item = (GlobalQueueItem *) queue->first;
	node = item->node;
	node = Cudd_Regular(node);
	infoN = (NodeData *) cuddHashTableGenericLookup(info->table, node);
	if (safe && infoN->parity == 3) {
	    cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
	    continue;
	}
	impactP = item->impactP;
	impactN = item->impactN;
	numOnset = infoN->mintermsP * impactP + infoN->mintermsN * impactN;
	savings = computeSavings(dd,node,NULL,info,localQueue);
	if (savings == 0) {
	    cuddLevelQueueQuit(queue);
	    cuddLevelQueueQuit(localQueue);
	    return(0);
	}
	cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
#if 0
	(void) printf("node %p: impact = %g/%g numOnset = %g savings %d\n",
		      node, impactP, impactN, numOnset, savings);
#endif
	if ((1 - numOnset / info->minterms) >
	    quality * (1 - (double) savings / info->size)) {
	    infoN->replace = CUDD_TRUE;
	    info->size -= savings;
	    info->minterms -=numOnset;
#if 0
	    (void) printf("replace: new size = %d new minterms = %g\n",
			  info->size, info->minterms);
#endif
	    savings -= updateRefs(dd,node,NULL,info,localQueue);
	    assert(savings == 0);
	    continue;
	}
	if (!cuddIsConstant(cuddT(node))) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,cuddT(node),
					 cuddI(dd,cuddT(node)->index));
	    item->impactP += impactP/2.0;
	    item->impactN += impactN/2.0;
	}
	if (!Cudd_IsConstant(cuddE(node))) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,Cudd_Regular(cuddE(node)),
					 cuddI(dd,Cudd_Regular(cuddE(node))->index));
	    if (Cudd_IsComplement(cuddE(node))) {
		item->impactP += impactN/2.0;
		item->impactN += impactP/2.0;
	    } else {
		item->impactP += impactP/2.0;
		item->impactN += impactN/2.0;
	    }
	}
    }

    cuddLevelQueueQuit(queue);
    cuddLevelQueueQuit(localQueue);
    return(1);

} /* end of UAmarkNodes */


/**Function********************************************************************

  Synopsis    [Builds the subset BDD.] 

  Description [Builds the subset BDD. Based on the info table,
  replaces selected nodes by zero. Returns a pointer to the result if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddUnderApprox]

******************************************************************************/
static DdNode *
UAbuildSubset(
  DdManager * dd /* DD manager */,
  DdNode * node /* current node */,
  ApproxInfo * info /* node info */)
{

    DdNode *Nt, *Ne, *N, *t, *e, *r;
    NodeData *infoN;

    if (Cudd_IsConstant(node))
	return(node);

    N = Cudd_Regular(node);

    if ((infoN = (NodeData *) cuddHashTableGenericLookup(info->table, N)) != NULL) {
	if (infoN->replace == CUDD_TRUE) {
	    return(info->zero);
	}
	if (N == node ) {
	    if (infoN->resultP != NULL) {
		return(infoN->resultP);
	    }
	} else {
	    if (infoN->resultN != NULL) {
		return(infoN->resultN);
	    }
	}
    } else {
	(void) fprintf(dd->err,
		       "Something is wrong, ought to be in info table\n");
	dd->errorCode = CUDD_INTERNAL_ERROR;
	return(NULL);
    }

    Nt = Cudd_NotCond(cuddT(N), Cudd_IsComplement(node));
    Ne = Cudd_NotCond(cuddE(N), Cudd_IsComplement(node));

    t = UAbuildSubset(dd, Nt, info);
    if (t == NULL) {
	return(NULL);
    }
    cuddRef(t);

    e = UAbuildSubset(dd, Ne, info);
    if (e == NULL) {
	Cudd_RecursiveDeref(dd,t);
	return(NULL);
    }
    cuddRef(e);

    if (Cudd_IsComplement(t)) {
	t = Cudd_Not(t);
	e = Cudd_Not(e);
	r = (t == e) ? t : cuddUniqueInter(dd, N->index, t, e);
	if (r == NULL) {
	    Cudd_RecursiveDeref(dd, e);
	    Cudd_RecursiveDeref(dd, t);
	    return(NULL);
	}
	r = Cudd_Not(r);
    } else {
	r = (t == e) ? t : cuddUniqueInter(dd, N->index, t, e);
	if (r == NULL) {
	    Cudd_RecursiveDeref(dd, e);
	    Cudd_RecursiveDeref(dd, t);
	    return(NULL);
	}
    }
    cuddDeref(t);
    cuddDeref(e);

    if (N == node) {
	infoN->resultP = r;
    } else {
	infoN->resultN = r;
    }

    return(r);

} /* end of UAbuildSubset */


/**Function********************************************************************

  Synopsis    [Marks nodes for remapping.]

  Description [Marks nodes for remapping. Returns 1 if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddRemapUnderApprox]

******************************************************************************/
static int
RAmarkNodes(
  DdManager * dd /* manager */,
  DdNode * f /* function to be analyzed */,
  ApproxInfo * info /* info on BDD */,
  int threshold /* when to stop approximating */,
  double quality /* minimum improvement for accepted changes */)
{
    DdLevelQueue *queue;
    DdLevelQueue *localQueue;
    NodeData *infoN, *infoT, *infoE;
    GlobalQueueItem *item;
    DdNode *node, *T, *E;
    DdNode *shared; /* grandchild shared by the two children of node */
    double numOnset;
    double impact, impactP, impactN;
    double minterms;
    int savings;
    int replace;

#if 0
    (void) fprintf(dd->out,"initial size = %d initial minterms = %g\n",
		  info->size, info->minterms);
#endif
    queue = cuddLevelQueueInit(dd->size,sizeof(GlobalQueueItem),info->size);
    if (queue == NULL) {
	return(0);
    }
    localQueue = cuddLevelQueueInit(dd->size,sizeof(LocalQueueItem),
				    dd->initSlots);
    if (localQueue == NULL) {
	cuddLevelQueueQuit(queue);
	return(0);
    }
    /* Enqueue regular pointer to root and initialize impact. */
    node = Cudd_Regular(f);
    item = (GlobalQueueItem *)
	cuddLevelQueueEnqueue(queue,node,cuddI(dd,node->index));
    if (item == NULL) {
	cuddLevelQueueQuit(queue);
	cuddLevelQueueQuit(localQueue);
	return(0);
    }
    if (Cudd_IsComplement(f)) {
	item->impactP = 0.0;
	item->impactN = 1.0;
    } else {
	item->impactP = 1.0;
	item->impactN = 0.0;
    }
    /* The nodes retrieved here are guaranteed to be non-terminal.
    ** The initial node is not terminal because constant nodes are
    ** dealt with in the calling procedure. Subsequent nodes are inserted
    ** only if they are not terminal. */
    while ((item = (GlobalQueueItem *) queue->first) != NULL) {
	/* If the size of the subset is below the threshold, quit. */
	if (info->size <= threshold)
	    break;
	node = item->node;
#ifdef DD_DEBUG
	assert(item->impactP >= 0 && item->impactP <= 1.0);
	assert(item->impactN >= 0 && item->impactN <= 1.0);
	assert(!Cudd_IsComplement(node));
	assert(!Cudd_IsConstant(node));
#endif
	if ((infoN = (NodeData *) cuddHashTableGenericLookup(info->table, node)) == NULL) {
	    cuddLevelQueueQuit(queue);
	    cuddLevelQueueQuit(localQueue);
	    return(0);
	}
#ifdef DD_DEBUG
	assert(infoN->parity >= 1 && infoN->parity <= 3);
#endif
	if (infoN->parity == 3) {
	    /* This node can be reached through paths of different parity.
	    ** It is not safe to replace it, because remapping will give
	    ** an incorrect result, while replacement by 0 may cause node
	    ** splitting. */
	    cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
	    continue;
	}
	T = cuddT(node);
	E = cuddE(node);
	shared = NULL;
	impactP = item->impactP;
	impactN = item->impactN;
	if (Cudd_bddLeq(dd,T,E)) {
	    /* Here we know that E is regular. */
#ifdef DD_DEBUG
	    assert(!Cudd_IsComplement(E));
#endif
	    infoT = (NodeData *) cuddHashTableGenericLookup(info->table, T);
	    infoE = (NodeData *) cuddHashTableGenericLookup(info->table, E);
	    if (infoN->parity == 1) {
		impact = impactP;
		minterms = infoE->mintermsP/2.0 - infoT->mintermsP/2.0;
		if (infoE->functionRef == 1 && !cuddIsConstant(E)) {
		    savings = 1 + computeSavings(dd,E,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_E;
	    } else {
#ifdef DD_DEBUG
		assert(infoN->parity == 2);
#endif
		impact = impactN;
		minterms = infoT->mintermsN/2.0 - infoE->mintermsN/2.0;
		if (infoT->functionRef == 1 && !cuddIsConstant(T)) {
		    savings = 1 + computeSavings(dd,T,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_T;
	    }
	    numOnset = impact * minterms;
	} else if (Cudd_bddLeq(dd,E,T)) {
	    /* Here E may be complemented. */
	    DdNode *Ereg = Cudd_Regular(E);
	    infoT = (NodeData *) cuddHashTableGenericLookup(info->table, T);
	    infoE = (NodeData *) cuddHashTableGenericLookup(info->table, Ereg);
	    if (infoN->parity == 1) {
		impact = impactP;
		minterms = infoT->mintermsP/2.0 -
		    ((E == Ereg) ? infoE->mintermsP : infoE->mintermsN)/2.0;
		if (infoT->functionRef == 1 && !cuddIsConstant(T)) {
		    savings = 1 + computeSavings(dd,T,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_T;
	    } else {
#ifdef DD_DEBUG
		assert(infoN->parity == 2);
#endif
		impact = impactN;
		minterms = ((E == Ereg) ? infoE->mintermsN :
			    infoE->mintermsP)/2.0 - infoT->mintermsN/2.0;
		if (infoE->functionRef == 1 && !cuddIsConstant(Ereg)) {
		    savings = 1 + computeSavings(dd,E,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_E;
	    }
	    numOnset = impact * minterms;
	} else {
	    DdNode *Ereg = Cudd_Regular(E);
	    DdNode *TT = cuddT(T);
	    DdNode *ET = Cudd_NotCond(cuddT(Ereg), Cudd_IsComplement(E));
	    if (T->index == Ereg->index && TT == ET) {
		shared = TT;
		replace = REPLACE_TT;
	    } else {
		DdNode *TE = cuddE(T);
		DdNode *EE = Cudd_NotCond(cuddE(Ereg), Cudd_IsComplement(E));
		if (T->index == Ereg->index && TE == EE) {
		    shared = TE;
		    replace = REPLACE_TE;
		} else {
		    replace = REPLACE_N;
		}
	    }
	    numOnset = infoN->mintermsP * impactP + infoN->mintermsN * impactN;
	    savings = computeSavings(dd,node,shared,info,localQueue);
	    if (shared != NULL) {
		NodeData *infoS;
		infoS = (NodeData *) cuddHashTableGenericLookup(info->table, Cudd_Regular(shared));
		if (Cudd_IsComplement(shared)) {
		    numOnset -= (infoS->mintermsN * impactP +
			infoS->mintermsP * impactN)/2.0;
		} else {
		    numOnset -= (infoS->mintermsP * impactP +
			infoS->mintermsN * impactN)/2.0;
		}
		savings--;
	    }
	}

	cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
#if 0
	if (replace == REPLACE_T || replace == REPLACE_E)
	    (void) printf("node %p: impact = %g numOnset = %g savings %d\n",
			  node, impact, numOnset, savings);
	else
	    (void) printf("node %p: impact = %g/%g numOnset = %g savings %d\n",
			  node, impactP, impactN, numOnset, savings);
#endif
	if ((1 - numOnset / info->minterms) >
	    quality * (1 - (double) savings / info->size)) {
	    infoN->replace = (char) replace;
	    info->size -= savings;
	    info->minterms -=numOnset;
#if 0
	    (void) printf("remap(%d): new size = %d new minterms = %g\n",
			  replace, info->size, info->minterms);
#endif
	    if (replace == REPLACE_N) {
		savings -= updateRefs(dd,node,NULL,info,localQueue);
	    } else if (replace == REPLACE_T) {
		savings -= updateRefs(dd,node,E,info,localQueue);
	    } else if (replace == REPLACE_E) {
		savings -= updateRefs(dd,node,T,info,localQueue);
	    } else {
#ifdef DD_DEBUG
		assert(replace == REPLACE_TT || replace == REPLACE_TE);
#endif
		savings -= updateRefs(dd,node,shared,info,localQueue) - 1;
	    }
	    assert(savings == 0);
	} else {
	    replace = NOTHING;
	}
	if (replace == REPLACE_N) continue;
	if ((replace == REPLACE_E || replace == NOTHING) &&
	    !cuddIsConstant(cuddT(node))) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,cuddT(node),
					 cuddI(dd,cuddT(node)->index));
	    if (replace == REPLACE_E) {
		item->impactP += impactP;
		item->impactN += impactN;
	    } else {
		item->impactP += impactP/2.0;
		item->impactN += impactN/2.0;
	    }
	}
	if ((replace == REPLACE_T || replace == NOTHING) &&
	    !Cudd_IsConstant(cuddE(node))) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,Cudd_Regular(cuddE(node)),
					 cuddI(dd,Cudd_Regular(cuddE(node))->index));
	    if (Cudd_IsComplement(cuddE(node))) {
		if (replace == REPLACE_T) {
		    item->impactP += impactN;
		    item->impactN += impactP;
		} else {
		    item->impactP += impactN/2.0;
		    item->impactN += impactP/2.0;
		}
	    } else {
		if (replace == REPLACE_T) {
		    item->impactP += impactP;
		    item->impactN += impactN;
		} else {
		    item->impactP += impactP/2.0;
		    item->impactN += impactN/2.0;
		}
	    }
	}
	if ((replace == REPLACE_TT || replace == REPLACE_TE) &&
	    !Cudd_IsConstant(shared)) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,Cudd_Regular(shared),
					 cuddI(dd,Cudd_Regular(shared)->index));
	    if (Cudd_IsComplement(shared)) {
	        item->impactP += impactN;
		item->impactN += impactP;
	    } else {
	        item->impactP += impactP;
		item->impactN += impactN;
	    }
	}
    }

    cuddLevelQueueQuit(queue);
    cuddLevelQueueQuit(localQueue);
    return(1);

} /* end of RAmarkNodes */


/**Function********************************************************************

  Synopsis    [Marks nodes for remapping.]

  Description [Marks nodes for remapping. Returns 1 if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddBiasedUnderApprox]

******************************************************************************/
static int
BAmarkNodes(
  DdManager *dd /* manager */,
  DdNode *f /* function to be analyzed */,
  ApproxInfo *info /* info on BDD */,
  int threshold /* when to stop approximating */,
  double quality1 /* minimum improvement for accepted changes when b=1 */,
  double quality0 /* minimum improvement for accepted changes when b=0 */)
{
    DdLevelQueue *queue;
    DdLevelQueue *localQueue;
    NodeData *infoN, *infoT, *infoE;
    GlobalQueueItem *item;
    DdNode *node, *T, *E;
    DdNode *shared; /* grandchild shared by the two children of node */
    double numOnset;
    double impact, impactP, impactN;
    double minterms;
    double quality;
    int savings;
    int replace;

#if 0
    (void) fprintf(dd->out,"initial size = %d initial minterms = %g\n",
		  info->size, info->minterms);
#endif
    queue = cuddLevelQueueInit(dd->size,sizeof(GlobalQueueItem),info->size);
    if (queue == NULL) {
	return(0);
    }
    localQueue = cuddLevelQueueInit(dd->size,sizeof(LocalQueueItem),
				    dd->initSlots);
    if (localQueue == NULL) {
	cuddLevelQueueQuit(queue);
	return(0);
    }
    /* Enqueue regular pointer to root and initialize impact. */
    node = Cudd_Regular(f);
    item = (GlobalQueueItem *)
	cuddLevelQueueEnqueue(queue,node,cuddI(dd,node->index));
    if (item == NULL) {
	cuddLevelQueueQuit(queue);
	cuddLevelQueueQuit(localQueue);
	return(0);
    }
    if (Cudd_IsComplement(f)) {
	item->impactP = 0.0;
	item->impactN = 1.0;
    } else {
	item->impactP = 1.0;
	item->impactN = 0.0;
    }
    /* The nodes retrieved here are guaranteed to be non-terminal.
    ** The initial node is not terminal because constant nodes are
    ** dealt with in the calling procedure. Subsequent nodes are inserted
    ** only if they are not terminal. */
    while (queue->first != NULL) {
	/* If the size of the subset is below the threshold, quit. */
	if (info->size <= threshold)
	    break;
	item = (GlobalQueueItem *) queue->first;
	node = item->node;
#ifdef DD_DEBUG
	assert(item->impactP >= 0 && item->impactP <= 1.0);
	assert(item->impactN >= 0 && item->impactN <= 1.0);
	assert(!Cudd_IsComplement(node));
	assert(!Cudd_IsConstant(node));
#endif
	if ((infoN = (NodeData *) cuddHashTableGenericLookup(info->table, node)) == NULL) {
	    cuddLevelQueueQuit(queue);
	    cuddLevelQueueQuit(localQueue);
	    return(0);
	}
	quality = infoN->care ? quality1 : quality0;
#ifdef DD_DEBUG
	assert(infoN->parity >= 1 && infoN->parity <= 3);
#endif
	if (infoN->parity == 3) {
	    /* This node can be reached through paths of different parity.
	    ** It is not safe to replace it, because remapping will give
	    ** an incorrect result, while replacement by 0 may cause node
	    ** splitting. */
	    cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
	    continue;
	}
	T = cuddT(node);
	E = cuddE(node);
	shared = NULL;
	impactP = item->impactP;
	impactN = item->impactN;
	if (Cudd_bddLeq(dd,T,E)) {
	    /* Here we know that E is regular. */
#ifdef DD_DEBUG
	    assert(!Cudd_IsComplement(E));
#endif
	    infoT = (NodeData *) cuddHashTableGenericLookup(info->table, T);
	    infoE = (NodeData *) cuddHashTableGenericLookup(info->table, E);
	    if (infoN->parity == 1) {
		impact = impactP;
		minterms = infoE->mintermsP/2.0 - infoT->mintermsP/2.0;
		if (infoE->functionRef == 1 && !Cudd_IsConstant(E)) {
		    savings = 1 + computeSavings(dd,E,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_E;
	    } else {
#ifdef DD_DEBUG
		assert(infoN->parity == 2);
#endif
		impact = impactN;
		minterms = infoT->mintermsN/2.0 - infoE->mintermsN/2.0;
		if (infoT->functionRef == 1 && !Cudd_IsConstant(T)) {
		    savings = 1 + computeSavings(dd,T,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_T;
	    }
	    numOnset = impact * minterms;
	} else if (Cudd_bddLeq(dd,E,T)) {
	    /* Here E may be complemented. */
	    DdNode *Ereg = Cudd_Regular(E);
	    infoT = (NodeData *) cuddHashTableGenericLookup(info->table, T);
	    infoE = (NodeData *) cuddHashTableGenericLookup(info->table, Ereg);
	    if (infoN->parity == 1) {
		impact = impactP;
		minterms = infoT->mintermsP/2.0 -
		    ((E == Ereg) ? infoE->mintermsP : infoE->mintermsN)/2.0;
		if (infoT->functionRef == 1 && !Cudd_IsConstant(T)) {
		    savings = 1 + computeSavings(dd,T,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_T;
	    } else {
#ifdef DD_DEBUG
		assert(infoN->parity == 2);
#endif
		impact = impactN;
		minterms = ((E == Ereg) ? infoE->mintermsN :
			    infoE->mintermsP)/2.0 - infoT->mintermsN/2.0;
		if (infoE->functionRef == 1 && !Cudd_IsConstant(E)) {
		    savings = 1 + computeSavings(dd,E,NULL,info,localQueue);
		    if (savings == 1) {
			cuddLevelQueueQuit(queue);
			cuddLevelQueueQuit(localQueue);
			return(0);
		    }
		} else {
		    savings = 1;
		}
		replace = REPLACE_E;
	    }
	    numOnset = impact * minterms;
	} else {
	    DdNode *Ereg = Cudd_Regular(E);
	    DdNode *TT = cuddT(T);
	    DdNode *ET = Cudd_NotCond(cuddT(Ereg), Cudd_IsComplement(E));
	    if (T->index == Ereg->index && TT == ET) {
		shared = TT;
		replace = REPLACE_TT;
	    } else {
		DdNode *TE = cuddE(T);
		DdNode *EE = Cudd_NotCond(cuddE(Ereg), Cudd_IsComplement(E));
		if (T->index == Ereg->index && TE == EE) {
		    shared = TE;
		    replace = REPLACE_TE;
		} else {
		    replace = REPLACE_N;
		}
	    }
	    numOnset = infoN->mintermsP * impactP + infoN->mintermsN * impactN;
	    savings = computeSavings(dd,node,shared,info,localQueue);
	    if (shared != NULL) {
		NodeData *infoS;
		infoS = (NodeData *) cuddHashTableGenericLookup(info->table, Cudd_Regular(shared));
		if (Cudd_IsComplement(shared)) {
		    numOnset -= (infoS->mintermsN * impactP +
			infoS->mintermsP * impactN)/2.0;
		} else {
		    numOnset -= (infoS->mintermsP * impactP +
			infoS->mintermsN * impactN)/2.0;
		}
		savings--;
	    }
	}

	cuddLevelQueueDequeue(queue,cuddI(dd,node->index));
#if 0
	if (replace == REPLACE_T || replace == REPLACE_E)
	    (void) printf("node %p: impact = %g numOnset = %g savings %d\n",
			  node, impact, numOnset, savings);
	else
	    (void) printf("node %p: impact = %g/%g numOnset = %g savings %d\n",
			  node, impactP, impactN, numOnset, savings);
#endif
	if ((1 - numOnset / info->minterms) >
	    quality * (1 - (double) savings / info->size)) {
	    infoN->replace = (char) replace;
	    info->size -= savings;
	    info->minterms -=numOnset;
#if 0
	    (void) printf("remap(%d): new size = %d new minterms = %g\n",
			  replace, info->size, info->minterms);
#endif
	    if (replace == REPLACE_N) {
		savings -= updateRefs(dd,node,NULL,info,localQueue);
	    } else if (replace == REPLACE_T) {
		savings -= updateRefs(dd,node,E,info,localQueue);
	    } else if (replace == REPLACE_E) {
		savings -= updateRefs(dd,node,T,info,localQueue);
	    } else {
#ifdef DD_DEBUG
		assert(replace == REPLACE_TT || replace == REPLACE_TE);
#endif
		savings -= updateRefs(dd,node,shared,info,localQueue) - 1;
	    }
	    assert(savings == 0);
	} else {
	    replace = NOTHING;
	}
	if (replace == REPLACE_N) continue;
	if ((replace == REPLACE_E || replace == NOTHING) &&
	    !cuddIsConstant(cuddT(node))) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,cuddT(node),
					 cuddI(dd,cuddT(node)->index));
	    if (replace == REPLACE_E) {
		item->impactP += impactP;
		item->impactN += impactN;
	    } else {
		item->impactP += impactP/2.0;
		item->impactN += impactN/2.0;
	    }
	}
	if ((replace == REPLACE_T || replace == NOTHING) &&
	    !Cudd_IsConstant(cuddE(node))) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,Cudd_Regular(cuddE(node)),
					 cuddI(dd,Cudd_Regular(cuddE(node))->index));
	    if (Cudd_IsComplement(cuddE(node))) {
		if (replace == REPLACE_T) {
		    item->impactP += impactN;
		    item->impactN += impactP;
		} else {
		    item->impactP += impactN/2.0;
		    item->impactN += impactP/2.0;
		}
	    } else {
		if (replace == REPLACE_T) {
		    item->impactP += impactP;
		    item->impactN += impactN;
		} else {
		    item->impactP += impactP/2.0;
		    item->impactN += impactN/2.0;
		}
	    }
	}
	if ((replace == REPLACE_TT || replace == REPLACE_TE) &&
	    !Cudd_IsConstant(shared)) {
	    item = (GlobalQueueItem *) cuddLevelQueueEnqueue(queue,Cudd_Regular(shared),
					 cuddI(dd,Cudd_Regular(shared)->index));
	    if (Cudd_IsComplement(shared)) {
		if (replace == REPLACE_T) {
		    item->impactP += impactN;
		    item->impactN += impactP;
		} else {
		    item->impactP += impactN/2.0;
		    item->impactN += impactP/2.0;
		}
	    } else {
		if (replace == REPLACE_T) {
		    item->impactP += impactP;
		    item->impactN += impactN;
		} else {
		    item->impactP += impactP/2.0;
		    item->impactN += impactN/2.0;
		}
	    }
	}
    }

    cuddLevelQueueQuit(queue);
    cuddLevelQueueQuit(localQueue);
    return(1);

} /* end of BAmarkNodes */


/**Function********************************************************************

  Synopsis [Builds the subset BDD for cuddRemapUnderApprox.]

  Description [Builds the subset BDDfor cuddRemapUnderApprox.  Based
  on the info table, performs remapping or replacement at selected
  nodes. Returns a pointer to the result if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddRemapUnderApprox]

******************************************************************************/
static DdNode *
RAbuildSubset(
  DdManager * dd /* DD manager */,
  DdNode * node /* current node */,
  ApproxInfo * info /* node info */)
{
    DdNode *Nt, *Ne, *N, *t, *e, *r;
    NodeData *infoN;

    if (Cudd_IsConstant(node))
	return(node);

    N = Cudd_Regular(node);

    Nt = Cudd_NotCond(cuddT(N), Cudd_IsComplement(node));
    Ne = Cudd_NotCond(cuddE(N), Cudd_IsComplement(node));

    if ((infoN = (NodeData *) cuddHashTableGenericLookup(info->table, N)) != NULL) {
	if (N == node ) {
	    if (infoN->resultP != NULL) {
		return(infoN->resultP);
	    }
	} else {
	    if (infoN->resultN != NULL) {
		return(infoN->resultN);
	    }
	}
	if (infoN->replace == REPLACE_T) {
	    r = RAbuildSubset(dd, Ne, info);
	    return(r);
	} else if (infoN->replace == REPLACE_E) {
	    r = RAbuildSubset(dd, Nt, info);
	    return(r);
	} else if (infoN->replace == REPLACE_N) {
	    return(info->zero);
	} else if (infoN->replace == REPLACE_TT) {
	    DdNode *Ntt = Cudd_NotCond(cuddT(cuddT(N)),
				       Cudd_IsComplement(node));
	    int index = cuddT(N)->index;
	    e = info->zero;
	    t = RAbuildSubset(dd, Ntt, info);
	    if (t == NULL) {
		return(NULL);
	    }
	    cuddRef(t);
	    if (Cudd_IsComplement(t)) {
		t = Cudd_Not(t);
		e = Cudd_Not(e);
		r = (t == e) ? t : cuddUniqueInter(dd, index, t, e);
		if (r == NULL) {
		    Cudd_RecursiveDeref(dd, t);
		    return(NULL);
		}
		r = Cudd_Not(r);
	    } else {
		r = (t == e) ? t : cuddUniqueInter(dd, index, t, e);
		if (r == NULL) {
		    Cudd_RecursiveDeref(dd, t);
		    return(NULL);
		}
	    }
	    cuddDeref(t);
	    return(r);
	} else if (infoN->replace == REPLACE_TE) {
	    DdNode *Nte = Cudd_NotCond(cuddE(cuddT(N)),
				       Cudd_IsComplement(node));
	    int index = cuddT(N)->index;
	    t = info->one;
	    e = RAbuildSubset(dd, Nte, info);
	    if (e == NULL) {
		return(NULL);
	    }
	    cuddRef(e);
	    e = Cudd_Not(e);
	    r = (t == e) ? t : cuddUniqueInter(dd, index, t, e);
	    if (r == NULL) {
		Cudd_RecursiveDeref(dd, e);
		return(NULL);
	    }
	    r =Cudd_Not(r);
	    cuddDeref(e);
	    return(r);
	}
    } else {
	(void) fprintf(dd->err,
		       "Something is wrong, ought to be in info table\n");
	dd->errorCode = CUDD_INTERNAL_ERROR;
	return(NULL);
    }

    t = RAbuildSubset(dd, Nt, info);
    if (t == NULL) {
	return(NULL);
    }
    cuddRef(t);

    e = RAbuildSubset(dd, Ne, info);
    if (e == NULL) {
	Cudd_RecursiveDeref(dd,t);
	return(NULL);
    }
    cuddRef(e);

    if (Cudd_IsComplement(t)) {
	t = Cudd_Not(t);
	e = Cudd_Not(e);
	r = (t == e) ? t : cuddUniqueInter(dd, N->index, t, e);
	if (r == NULL) {
	    Cudd_RecursiveDeref(dd, e);
	    Cudd_RecursiveDeref(dd, t);
	    return(NULL);
	}
	r = Cudd_Not(r);
    } else {
	r = (t == e) ? t : cuddUniqueInter(dd, N->index, t, e);
	if (r == NULL) {
	    Cudd_RecursiveDeref(dd, e);
	    Cudd_RecursiveDeref(dd, t);
	    return(NULL);
	}
    }
    cuddDeref(t);
    cuddDeref(e);

    if (N == node) {
	infoN->resultP = r;
    } else {
	infoN->resultN = r;
    }

    return(r);

} /* end of RAbuildSubset */


/**Function********************************************************************

  Synopsis    [Finds don't care nodes.]

  Description [Finds don't care nodes by traversing f and b in parallel.
  Returns the care status of the visited f node if successful; CARE_ERROR
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddBiasedUnderApprox]

******************************************************************************/
static int
BAapplyBias(
  DdManager *dd,
  DdNode *f,
  DdNode *b,
  ApproxInfo *info,
  DdHashTable *cache)
{
    DdNode *one, *zero, *res;
    DdNode *Ft, *Fe, *B, *Bt, *Be;
    unsigned int topf, topb;
    NodeData *infoF;
    int careT, careE;

    one = DD_ONE(dd);
    zero = Cudd_Not(one);

    if ((infoF = (NodeData *) cuddHashTableGenericLookup(info->table, f)) == NULL)
	return(CARE_ERROR);
    if (f == one) return(TOTAL_CARE);
    if (b == zero) return(infoF->care);
    if (infoF->care == TOTAL_CARE) return(TOTAL_CARE);

    if ((f->ref != 1 || Cudd_Regular(b)->ref != 1) &&
	(res = cuddHashTableLookup2(cache,f,b)) != NULL) {
	if (res->ref == 0) {
	    cache->manager->dead++;
	    cache->manager->constants.dead++;
	}
	return(infoF->care);
    }

    topf = dd->perm[f->index];
    B = Cudd_Regular(b);
    topb = cuddI(dd,B->index);
    if (topf <= topb) {
	Ft = cuddT(f); Fe = cuddE(f);
    } else {
	Ft = Fe = f;
    }
    if (topb <= topf) {
	/* We know that b is not constant because f is not. */
	Bt = cuddT(B); Be = cuddE(B);
	if (Cudd_IsComplement(b)) {
	    Bt = Cudd_Not(Bt);
	    Be = Cudd_Not(Be);
	}
    } else {
	Bt = Be = b;
    }

    careT = BAapplyBias(dd, Ft, Bt, info, cache);
    if (careT == CARE_ERROR)
	return(CARE_ERROR);
    careE = BAapplyBias(dd, Cudd_Regular(Fe), Be, info, cache);
    if (careE == CARE_ERROR)
	return(CARE_ERROR);
    if (careT == TOTAL_CARE && careE == TOTAL_CARE) {
	infoF->care = TOTAL_CARE;
    } else {
	infoF->care = CARE;
    }

    if (f->ref != 1 || Cudd_Regular(b)->ref != 1) {
	ptrint fanout = (ptrint) f->ref * Cudd_Regular(b)->ref;
	cuddSatDec(fanout);
	if (!cuddHashTableInsert2(cache,f,b,one,fanout)) {
	    return(CARE_ERROR);
	}
    }
    return(infoF->care);

} /* end of BAapplyBias */
