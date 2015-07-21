/**CFile***********************************************************************

  FileName    [cuddUtil.c]

  PackageName [cudd]

  Synopsis    [Utility functions.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_PrintMinterm()
		<li> Cudd_bddPrintCover()
		<li> Cudd_PrintDebug()
		<li> Cudd_DagSize()
		<li> Cudd_EstimateCofactor()
		<li> Cudd_EstimateCofactorSimple()
		<li> Cudd_SharingSize()
		<li> Cudd_CountMinterm()
		<li> Cudd_EpdCountMinterm()
		<li> Cudd_CountPath()
		<li> Cudd_CountPathsToNonZero()
                <li> Cudd_SupportIndices()
		<li> Cudd_Support()
		<li> Cudd_SupportIndex()
		<li> Cudd_SupportSize()
		<li> Cudd_VectorSupportIndices()
		<li> Cudd_VectorSupport()
		<li> Cudd_VectorSupportIndex()
		<li> Cudd_VectorSupportSize()
		<li> Cudd_ClassifySupport()
		<li> Cudd_CountLeaves()
		<li> Cudd_bddPickOneCube()
		<li> Cudd_bddPickOneMinterm()
		<li> Cudd_bddPickArbitraryMinterms()
		<li> Cudd_SubsetWithMaskVars()
		<li> Cudd_FirstCube()
		<li> Cudd_NextCube()
		<li> Cudd_bddComputeCube()
		<li> Cudd_addComputeCube()
		<li> Cudd_FirstNode()
		<li> Cudd_NextNode()
		<li> Cudd_GenFree()
		<li> Cudd_IsGenEmpty()
		<li> Cudd_IndicesToCube()
		<li> Cudd_PrintVersion()
		<li> Cudd_AverageDistance()
		<li> Cudd_Random()
		<li> Cudd_Srandom()
		<li> Cudd_Density()
		</ul>
	Internal procedures included in this module:
		<ul>
		<li> cuddP()
		<li> cuddStCountfree()
		<li> cuddCollectNodes()
		<li> cuddNodeArray()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> dp2()
		<li> ddPrintMintermAux()
		<li> ddDagInt()
		<li> ddCountMintermAux()
		<li> ddEpdCountMintermAux()
		<li> ddCountPathAux()
		<li> ddSupportStep()
		<li> ddClearFlag()
		<li> ddLeavesInt()
		<li> ddPickArbitraryMinterms()
		<li> ddPickRepresentativeCube()
		<li> ddEpdFree()
                <li> ddFindSupport()
                <li> ddClearVars()
                <li> indexCompare()
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

/* Random generator constants. */
#define MODULUS1 2147483563
#define LEQA1 40014
#define LEQQ1 53668
#define LEQR1 12211
#define MODULUS2 2147483399
#define LEQA2 40692
#define LEQQ2 52774
#define LEQR2 3791
#define STAB_SIZE 64
#define STAB_DIV (1 + (MODULUS1 - 1) / STAB_SIZE)

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
static char rcsid[] DD_UNUSED = "$Id: cuddUtil.c,v 1.83 2012/02/05 01:07:19 fabio Exp $";
#endif

static	DdNode	*background, *zero;

static	long cuddRand = 0;
static	long cuddRand2;
static	long shuffleSelect;
static	long shuffleTable[STAB_SIZE];

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#define bang(f)	((Cudd_IsComplement(f)) ? '!' : ' ')

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int dp2 (DdManager *dd, DdNode *f, st_table *t);
static void ddPrintMintermAux (DdManager *dd, DdNode *node, int *list);
static int ddDagInt (DdNode *n);
static int cuddNodeArrayRecur (DdNode *f, DdNodePtr *table, int index);
static int cuddEstimateCofactor (DdManager *dd, st_table *table, DdNode * node, int i, int phase, DdNode ** ptr);
static DdNode * cuddUniqueLookup (DdManager * unique, int  index, DdNode * T, DdNode * E);
static int cuddEstimateCofactorSimple (DdNode * node, int i);
static double ddCountMintermAux (DdNode *node, double max, DdHashTable *table);
static int ddEpdCountMintermAux (DdNode *node, EpDouble *max, EpDouble *epd, st_table *table);
static double ddCountPathAux (DdNode *node, st_table *table);
static double ddCountPathsToNonZero (DdNode * N, st_table * table);
static void ddSupportStep (DdNode *f, int *support);
static void ddClearFlag (DdNode *f);
static int ddLeavesInt (DdNode *n);
static int ddPickArbitraryMinterms (DdManager *dd, DdNode *node, int nvars, int nminterms, char **string);
static int ddPickRepresentativeCube (DdManager *dd, DdNode *node, double *weight, char *string);
static enum st_retval ddEpdFree (char * key, char * value, char * arg);
static void ddFindSupport(DdManager *dd, DdNode *f, int *SP);
static void ddClearVars(DdManager *dd, int SP);
static int indexCompare(const void *a, const void *b);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Prints a disjoint sum of products.]

  Description [Prints a disjoint sum of product cover for the function
  rooted at node. Each product corresponds to a path from node to a
  leaf node different from the logical zero, and different from the
  background value. Uses the package default output file.  Returns 1
  if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintDebug Cudd_bddPrintCover]

******************************************************************************/
int
Cudd_PrintMinterm(
  DdManager * manager,
  DdNode * node)
{
    int		i, *list;

    background = manager->background;
    zero = Cudd_Not(manager->one);
    list = ALLOC(int,manager->size);
    if (list == NULL) {
	manager->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (i = 0; i < manager->size; i++) list[i] = 2;
    ddPrintMintermAux(manager,node,list);
    FREE(list);
    return(1);

} /* end of Cudd_PrintMinterm */


/**Function********************************************************************

  Synopsis    [Prints a sum of prime implicants of a BDD.]

  Description [Prints a sum of product cover for an incompletely
  specified function given by a lower bound and an upper bound.  Each
  product is a prime implicant obtained by expanding the product
  corresponding to a path from node to the constant one.  Uses the
  package default output file.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintMinterm]

******************************************************************************/
int
Cudd_bddPrintCover(
  DdManager *dd,
  DdNode *l,
  DdNode *u)
{
    int *array;
    int q, result;
    DdNode *lb;
#ifdef DD_DEBUG
    DdNode *cover;
#endif

    array = ALLOC(int, Cudd_ReadSize(dd));
    if (array == NULL) return(0);
    lb = l;
    cuddRef(lb);
#ifdef DD_DEBUG
    cover = Cudd_ReadLogicZero(dd);
    cuddRef(cover);
#endif
    while (lb != Cudd_ReadLogicZero(dd)) {
	DdNode *implicant, *prime, *tmp;
	int length;
	implicant = Cudd_LargestCube(dd,lb,&length);
	if (implicant == NULL) {
	    Cudd_RecursiveDeref(dd,lb);
	    FREE(array);
	    return(0);
	}
	cuddRef(implicant);
	prime = Cudd_bddMakePrime(dd,implicant,u);
	if (prime == NULL) {
	    Cudd_RecursiveDeref(dd,lb);
	    Cudd_RecursiveDeref(dd,implicant);
	    FREE(array);
	    return(0);
	}
	cuddRef(prime);
	Cudd_RecursiveDeref(dd,implicant);
	tmp = Cudd_bddAnd(dd,lb,Cudd_Not(prime));
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,lb);
	    Cudd_RecursiveDeref(dd,prime);
	    FREE(array);
	    return(0);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,lb);
	lb = tmp;
	result = Cudd_BddToCubeArray(dd,prime,array);
	if (result == 0) {
	    Cudd_RecursiveDeref(dd,lb);
	    Cudd_RecursiveDeref(dd,prime);
	    FREE(array);
	    return(0);
	}
	for (q = 0; q < dd->size; q++) {
	    switch (array[q]) {
	    case 0:
		(void) fprintf(dd->out, "0");
		break;
	    case 1:
		(void) fprintf(dd->out, "1");
		break;
	    case 2:
		(void) fprintf(dd->out, "-");
		break;
	    default:
		(void) fprintf(dd->out, "?");
	    }
	}
	(void) fprintf(dd->out, " 1\n");
#ifdef DD_DEBUG
	tmp = Cudd_bddOr(dd,prime,cover);
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,cover);
	    Cudd_RecursiveDeref(dd,lb);
	    Cudd_RecursiveDeref(dd,prime);
	    FREE(array);
	    return(0);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,cover);
	cover = tmp;
#endif
	Cudd_RecursiveDeref(dd,prime);
    }
    (void) fprintf(dd->out, "\n");
    Cudd_RecursiveDeref(dd,lb);
    FREE(array);
#ifdef DD_DEBUG
    if (!Cudd_bddLeq(dd,cover,u) || !Cudd_bddLeq(dd,l,cover)) {
	Cudd_RecursiveDeref(dd,cover);
	return(0);
    }
    Cudd_RecursiveDeref(dd,cover);
#endif
    return(1);

} /* end of Cudd_bddPrintCover */


/**Function********************************************************************

  Synopsis    [Prints to the standard output a DD and its statistics.]

  Description [Prints to the standard output a DD and its statistics.
  The statistics include the number of nodes, the number of leaves, and
  the number of minterms. (The number of minterms is the number of
  assignments to the variables that cause the function to be different
  from the logical zero (for BDDs) and from the background value (for
  ADDs.) The statistics are printed if pr &gt; 0. Specifically:
  <ul>
  <li> pr = 0 : prints nothing
  <li> pr = 1 : prints counts of nodes and minterms
  <li> pr = 2 : prints counts + disjoint sum of product
  <li> pr = 3 : prints counts + list of nodes
  <li> pr &gt; 3 : prints counts + disjoint sum of product + list of nodes
  </ul>
  For the purpose of counting the number of minterms, the function is
  supposed to depend on n variables. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_DagSize Cudd_CountLeaves Cudd_CountMinterm
  Cudd_PrintMinterm]

******************************************************************************/
int
Cudd_PrintDebug(
  DdManager * dd,
  DdNode * f,
  int  n,
  int  pr)
{
    DdNode *azero, *bzero;
    int	   nodes;
    int	   leaves;
    double minterms;
    int    retval = 1;

    if (f == NULL) {
	(void) fprintf(dd->out,": is the NULL DD\n");
	(void) fflush(dd->out);
	return(0);
    }
    azero = DD_ZERO(dd);
    bzero = Cudd_Not(DD_ONE(dd));
    if ((f == azero || f == bzero) && pr > 0){
       (void) fprintf(dd->out,": is the zero DD\n");
       (void) fflush(dd->out);
       return(1);
    }
    if (pr > 0) {
	nodes = Cudd_DagSize(f);
	if (nodes == CUDD_OUT_OF_MEM) retval = 0;
	leaves = Cudd_CountLeaves(f);
	if (leaves == CUDD_OUT_OF_MEM) retval = 0;
	minterms = Cudd_CountMinterm(dd, f, n);
	if (minterms == (double)CUDD_OUT_OF_MEM) retval = 0;
	(void) fprintf(dd->out,": %d nodes %d leaves %g minterms\n",
		       nodes, leaves, minterms);
	if (pr > 2) {
	    if (!cuddP(dd, f)) retval = 0;
	}
	if (pr == 2 || pr > 3) {
	    if (!Cudd_PrintMinterm(dd,f)) retval = 0;
	    (void) fprintf(dd->out,"\n");
	}
	(void) fflush(dd->out);
    }
    return(retval);

} /* end of Cudd_PrintDebug */


/**Function********************************************************************

  Synopsis    [Counts the number of nodes in a DD.]

  Description [Counts the number of nodes in a DD. Returns the number
  of nodes in the graph rooted at node.]

  SideEffects [None]

  SeeAlso     [Cudd_SharingSize Cudd_PrintDebug]

******************************************************************************/
int
Cudd_DagSize(
  DdNode * node)
{
    int	i;

    i = ddDagInt(Cudd_Regular(node));
    ddClearFlag(Cudd_Regular(node));

    return(i);

} /* end of Cudd_DagSize */


/**Function********************************************************************

  Synopsis    [Estimates the number of nodes in a cofactor of a DD.]

  Description [Estimates the number of nodes in a cofactor of a DD.
  Returns an estimate of the number of nodes in a cofactor of
  the graph rooted at node with respect to the variable whose index is i.
  In case of failure, returns CUDD_OUT_OF_MEM.
  This function uses a refinement of the algorithm of Cabodi et al.
  (ICCAD96). The refinement allows the procedure to account for part
  of the recombination that may occur in the part of the cofactor above
  the cofactoring variable. This procedure does not create any new node.
  It does keep a small table of results; therefore it may run out of memory.
  If this is a concern, one should use Cudd_EstimateCofactorSimple, which
  is faster, does not allocate any memory, but is less accurate.]

  SideEffects [None]

  SeeAlso     [Cudd_DagSize Cudd_EstimateCofactorSimple]

******************************************************************************/
int
Cudd_EstimateCofactor(
  DdManager *dd /* manager */,
  DdNode * f	/* function */,
  int i		/* index of variable */,
  int phase	/* 1: positive; 0: negative */
  )
{
    int	val;
    DdNode *ptr;
    st_table *table;

    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) return(CUDD_OUT_OF_MEM);
    val = cuddEstimateCofactor(dd,table,Cudd_Regular(f),i,phase,&ptr);
    ddClearFlag(Cudd_Regular(f));
    st_free_table(table);

    return(val);

} /* end of Cudd_EstimateCofactor */


/**Function********************************************************************

  Synopsis    [Estimates the number of nodes in a cofactor of a DD.]

  Description [Estimates the number of nodes in a cofactor of a DD.
  Returns an estimate of the number of nodes in the positive cofactor of
  the graph rooted at node with respect to the variable whose index is i.
  This procedure implements with minor changes the algorithm of Cabodi et al.
  (ICCAD96). It does not allocate any memory, it does not change the
  state of the manager, and it is fast. However, it has been observed to
  overestimate the size of the cofactor by as much as a factor of 2.]

  SideEffects [None]

  SeeAlso     [Cudd_DagSize]

******************************************************************************/
int
Cudd_EstimateCofactorSimple(
  DdNode * node,
  int i)
{
    int	val;

    val = cuddEstimateCofactorSimple(Cudd_Regular(node),i);
    ddClearFlag(Cudd_Regular(node));

    return(val);

} /* end of Cudd_EstimateCofactorSimple */


/**Function********************************************************************

  Synopsis    [Counts the number of nodes in an array of DDs.]

  Description [Counts the number of nodes in an array of DDs. Shared
  nodes are counted only once.  Returns the total number of nodes.]

  SideEffects [None]

  SeeAlso     [Cudd_DagSize]

******************************************************************************/
int
Cudd_SharingSize(
  DdNode ** nodeArray,
  int  n)
{
    int	i,j;

    i = 0;
    for (j = 0; j < n; j++) {
	i += ddDagInt(Cudd_Regular(nodeArray[j]));
    }
    for (j = 0; j < n; j++) {
	ddClearFlag(Cudd_Regular(nodeArray[j]));
    }
    return(i);

} /* end of Cudd_SharingSize */


/**Function********************************************************************

  Synopsis    [Counts the number of minterms of a DD.]

  Description [Counts the number of minterms of a DD. The function is
  assumed to depend on nvars variables. The minterm count is
  represented as a double, to allow for a larger number of variables.
  Returns the number of minterms of the function rooted at node if
  successful; (double) CUDD_OUT_OF_MEM otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintDebug Cudd_CountPath]

******************************************************************************/
double
Cudd_CountMinterm(
  DdManager * manager,
  DdNode * node,
  int  nvars)
{
    double	max;
    DdHashTable	*table;
    double	res;
    CUDD_VALUE_TYPE epsilon;

    background = manager->background;
    zero = Cudd_Not(manager->one);

    max = pow(2.0,(double)nvars);
    table = cuddHashTableInit(manager,1,2);
    if (table == NULL) {
	return((double)CUDD_OUT_OF_MEM);
    }
    epsilon = Cudd_ReadEpsilon(manager);
    Cudd_SetEpsilon(manager,(CUDD_VALUE_TYPE)0.0);
    res = ddCountMintermAux(node,max,table);
    cuddHashTableQuit(table);
    Cudd_SetEpsilon(manager,epsilon);

    return(res);

} /* end of Cudd_CountMinterm */


/**Function********************************************************************

  Synopsis    [Counts the number of paths of a DD.]

  Description [Counts the number of paths of a DD.  Paths to all
  terminal nodes are counted. The path count is represented as a
  double, to allow for a larger number of variables.  Returns the
  number of paths of the function rooted at node if successful;
  (double) CUDD_OUT_OF_MEM otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_CountMinterm]

******************************************************************************/
double
Cudd_CountPath(
  DdNode * node)
{

    st_table	*table;
    double	i;

    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) {
	return((double)CUDD_OUT_OF_MEM);
    }
    i = ddCountPathAux(Cudd_Regular(node),table);
    st_foreach(table, cuddStCountfree, NULL);
    st_free_table(table);
    return(i);

} /* end of Cudd_CountPath */


/**Function********************************************************************

  Synopsis    [Counts the number of minterms of a DD with extended precision.]

  Description [Counts the number of minterms of a DD with extended precision.
  The function is assumed to depend on nvars variables. The minterm count is
  represented as an EpDouble, to allow any number of variables.
  Returns 0 if successful; CUDD_OUT_OF_MEM otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintDebug Cudd_CountPath]

******************************************************************************/
int
Cudd_EpdCountMinterm(
  DdManager * manager,
  DdNode * node,
  int  nvars,
  EpDouble * epd)
{
    EpDouble	max, tmp;
    st_table	*table;
    int		status;

    background = manager->background;
    zero = Cudd_Not(manager->one);

    EpdPow2(nvars, &max);
    table = st_init_table(EpdCmp, st_ptrhash);
    if (table == NULL) {
	EpdMakeZero(epd, 0);
	return(CUDD_OUT_OF_MEM);
    }
    status = ddEpdCountMintermAux(Cudd_Regular(node),&max,epd,table);
    st_foreach(table, ddEpdFree, NULL);
    st_free_table(table);
    if (status == CUDD_OUT_OF_MEM) {
	EpdMakeZero(epd, 0);
	return(CUDD_OUT_OF_MEM);
    }
    if (Cudd_IsComplement(node)) {
	EpdSubtract3(&max, epd, &tmp);
	EpdCopy(&tmp, epd);
    }
    return(0);

} /* end of Cudd_EpdCountMinterm */


/**Function********************************************************************

  Synopsis    [Counts the number of paths to a non-zero terminal of a DD.]

  Description [Counts the number of paths to a non-zero terminal of a
  DD.  The path count is
  represented as a double, to allow for a larger number of variables.
  Returns the number of paths of the function rooted at node.]

  SideEffects [None]

  SeeAlso     [Cudd_CountMinterm Cudd_CountPath]

******************************************************************************/
double
Cudd_CountPathsToNonZero(
  DdNode * node)
{

    st_table	*table;
    double	i;

    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) {
	return((double)CUDD_OUT_OF_MEM);
    }
    i = ddCountPathsToNonZero(node,table);
    st_foreach(table, cuddStCountfree, NULL);
    st_free_table(table);
    return(i);

} /* end of Cudd_CountPathsToNonZero */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a DD depends.]

  Description [Finds the variables on which a DD depends.  Returns the
  number of variables if successful; CUDD_OUT_OF_MEM otherwise.]

  SideEffects [The indices of the support variables are returned as
  side effects.  If the function is constant, no array is allocated.]

  SeeAlso     [Cudd_Support Cudd_SupportIndex Cudd_VectorSupportIndices]

******************************************************************************/
int
Cudd_SupportIndices(
  DdManager * dd /* manager */,
  DdNode * f /* DD whose support is sought */,
  int **indices /* array containing (on return) the indices */)
{
    int SP = 0;

    ddFindSupport(dd, Cudd_Regular(f), &SP);
    ddClearFlag(Cudd_Regular(f));
    ddClearVars(dd, SP);
    if (SP > 0) {
        int i;
        *indices = ALLOC(int, SP);
        if (*indices == NULL) {
            dd->errorCode = CUDD_MEMORY_OUT;
            return(CUDD_OUT_OF_MEM);
        }

        for (i = 0; i < SP; i++)
            (*indices)[i] = (int) (ptrint) dd->stack[i];

        qsort(*indices, SP, sizeof(int), indexCompare);
    } else {
        *indices = NULL;
    }

    return(SP);

} /* end of Cudd_SupportIndices */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a DD depends.]

  Description [Finds the variables on which a DD depends.
  Returns a BDD consisting of the product of the variables if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_VectorSupport Cudd_ClassifySupport]

******************************************************************************/
DdNode *
Cudd_Support(
  DdManager * dd /* manager */,
  DdNode * f /* DD whose support is sought */)
{
    int	*support;
    DdNode *res;
    int j;

    int size = Cudd_SupportIndices(dd, f, &support);
    if (size == CUDD_OUT_OF_MEM)
        return(NULL);

    /* Transform support from array of indices to cube. */
    res = DD_ONE(dd);
    cuddRef(res);
    
    for (j = size - 1; j >= 0; j--) { /* for each index bottom-up (almost) */
        int index = support[j];
        DdNode *var = dd->vars[index];
        DdNode *tmp = Cudd_bddAnd(dd,res,var);
        if (tmp == NULL) {
            Cudd_RecursiveDeref(dd,res);
            FREE(support);
            return(NULL);
        }
        cuddRef(tmp);
        Cudd_RecursiveDeref(dd,res);
        res = tmp;
    }

    FREE(support);
    cuddDeref(res);
    return(res);

} /* end of Cudd_Support */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a DD depends.]

  Description [Finds the variables on which a DD depends.  Returns an
  index array of the variables if successful; NULL otherwise.  The
  size of the array equals the number of variables in the manager.
  Each entry of the array is 1 if the corresponding variable is in the
  support of the DD and 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Support Cudd_SupportIndices Cudd_ClassifySupport]

******************************************************************************/
int *
Cudd_SupportIndex(
  DdManager * dd /* manager */,
  DdNode * f /* DD whose support is sought */)
{
    int	*support;
    int	i;
    int size;

    /* Allocate and initialize support array for ddSupportStep. */
    size = ddMax(dd->size, dd->sizeZ);
    support = ALLOC(int,size);
    if (support == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < size; i++) {
	support[i] = 0;
    }

    /* Compute support and clean up markers. */
    ddSupportStep(Cudd_Regular(f),support);
    ddClearFlag(Cudd_Regular(f));

    return(support);

} /* end of Cudd_SupportIndex */


/**Function********************************************************************

  Synopsis    [Counts the variables on which a DD depends.]

  Description [Returns the variables on which a DD depends.]

  SideEffects [None]

  SeeAlso     [Cudd_Support Cudd_SupportIndices]

******************************************************************************/
int
Cudd_SupportSize(
  DdManager * dd /* manager */,
  DdNode * f /* DD whose support size is sought */)
{
    int SP = 0;

    ddFindSupport(dd, Cudd_Regular(f), &SP);
    ddClearFlag(Cudd_Regular(f));
    ddClearVars(dd, SP);

    return(SP);

} /* end of Cudd_SupportSize */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a set of DDs depends.]

  Description [Finds the variables on which a set of DDs depends.  The
  set must contain either BDDs and ADDs, or ZDDs.  Returns the number
  of variables if successful; CUDD_OUT_OF_MEM otherwise.]

  SideEffects [The indices of the support variables are returned as
  side effects.  If the function is constant, no array is allocated.]

  SeeAlso     [Cudd_Support Cudd_SupportIndex Cudd_VectorSupportIndices]

******************************************************************************/
int
Cudd_VectorSupportIndices(
  DdManager * dd /* manager */,
  DdNode ** F /* DD whose support is sought */,
  int  n /* size of the array */,
  int **indices /* array containing (on return) the indices */)
{
    int i;
    int SP = 0;

    /* Compute support and clean up markers. */
    for (i = 0; i < n; i++) {
	ddFindSupport(dd, Cudd_Regular(F[i]), &SP);
    }
    for (i = 0; i < n; i++) {
	ddClearFlag(Cudd_Regular(F[i]));
    }
    ddClearVars(dd, SP);

    if (SP > 0) {
        int i;
        *indices = ALLOC(int, SP);
        if (*indices == NULL) {
            dd->errorCode = CUDD_MEMORY_OUT;
            return(CUDD_OUT_OF_MEM);
        }

        for (i = 0; i < SP; i++)
            (*indices)[i] = (int) (ptrint) dd->stack[i];

        qsort(*indices, SP, sizeof(int), indexCompare);
    } else {
        *indices = NULL;
    }

    return(SP);

} /* end of Cudd_VectorSupportIndices */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a set of DDs depends.]

  Description [Finds the variables on which a set of DDs depends.
  The set must contain either BDDs and ADDs, or ZDDs.
  Returns a BDD consisting of the product of the variables if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Support Cudd_ClassifySupport]

******************************************************************************/
DdNode *
Cudd_VectorSupport(
  DdManager * dd /* manager */,
  DdNode ** F /* array of DDs whose support is sought */,
  int  n /* size of the array */)
{
    int	*support;
    DdNode *res;
    int	j;
    int size = Cudd_VectorSupportIndices(dd, F, n, &support);
    if (size == CUDD_OUT_OF_MEM)
        return(NULL);

    /* Transform support from array of indices to cube. */
    res = DD_ONE(dd);
    cuddRef(res);
    
    for (j = size - 1; j >= 0; j--) { /* for each index bottom-up (almost) */
        int index = support[j];
        DdNode *var = dd->vars[index];
        DdNode *tmp = Cudd_bddAnd(dd,res,var);
        if (tmp == NULL) {
            Cudd_RecursiveDeref(dd,res);
            FREE(support);
            return(NULL);
        }
        cuddRef(tmp);
        Cudd_RecursiveDeref(dd,res);
        res = tmp;
    }

    FREE(support);
    cuddDeref(res);
    return(res);

} /* end of Cudd_VectorSupport */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a set of DDs depends.]

  Description [Finds the variables on which a set of DDs depends.
  The set must contain either BDDs and ADDs, or ZDDs.
  Returns an index array of the variables if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_SupportIndex Cudd_VectorSupport Cudd_VectorSupportIndices]

******************************************************************************/
int *
Cudd_VectorSupportIndex(
  DdManager * dd /* manager */,
  DdNode ** F /* array of DDs whose support is sought */,
  int  n /* size of the array */)
{
    int	*support;
    int	i;
    int size;

    /* Allocate and initialize support array for ddSupportStep. */
    size = ddMax(dd->size, dd->sizeZ);
    support = ALLOC(int,size);
    if (support == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < size; i++) {
	support[i] = 0;
    }

    /* Compute support and clean up markers. */
    for (i = 0; i < n; i++) {
	ddSupportStep(Cudd_Regular(F[i]),support);
    }
    for (i = 0; i < n; i++) {
	ddClearFlag(Cudd_Regular(F[i]));
    }

    return(support);

} /* end of Cudd_VectorSupportIndex */


/**Function********************************************************************

  Synopsis    [Counts the variables on which a set of DDs depends.]

  Description [Returns the variables on which a set of DDs depends.
  The set must contain either BDDs and ADDs, or ZDDs.]

  SideEffects [None]

  SeeAlso     [Cudd_VectorSupport Cudd_SupportSize]

******************************************************************************/
int
Cudd_VectorSupportSize(
  DdManager * dd /* manager */,
  DdNode ** F /* array of DDs whose support is sought */,
  int  n /* size of the array */)
{
    int i;
    int SP = 0;

    /* Compute support and clean up markers. */
    for (i = 0; i < n; i++) {
	ddFindSupport(dd, Cudd_Regular(F[i]), &SP);
    }
    for (i = 0; i < n; i++) {
	ddClearFlag(Cudd_Regular(F[i]));
    }
    ddClearVars(dd, SP);

    return(SP);

} /* end of Cudd_VectorSupportSize */


/**Function********************************************************************

  Synopsis    [Classifies the variables in the support of two DDs.]

  Description [Classifies the variables in the support of two DDs
  <code>f</code> and <code>g</code>, depending on whther they appear
  in both DDs, only in <code>f</code>, or only in <code>g</code>.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [The cubes of the three classes of variables are
  returned as side effects.]

  SeeAlso     [Cudd_Support Cudd_VectorSupport]

******************************************************************************/
int
Cudd_ClassifySupport(
  DdManager * dd /* manager */,
  DdNode * f /* first DD */,
  DdNode * g /* second DD */,
  DdNode ** common /* cube of shared variables */,
  DdNode ** onlyF /* cube of variables only in f */,
  DdNode ** onlyG /* cube of variables only in g */)
{
    int	*supportF, *supportG;
    int	fi, gi;
    int sizeF, sizeG;

    sizeF = Cudd_SupportIndices(dd, f, &supportF);
    if (sizeF == CUDD_OUT_OF_MEM)
        return(0);

    sizeG = Cudd_SupportIndices(dd, g, &supportG);
    if (sizeG == CUDD_OUT_OF_MEM) {
        FREE(supportF);
        return(0);
    }

    /* Classify variables and create cubes. This part of the procedure
    ** relies on the sorting of the indices in the two support arrays.
    */
    *common = *onlyF = *onlyG = DD_ONE(dd);
    cuddRef(*common); cuddRef(*onlyF); cuddRef(*onlyG);
    fi = sizeF - 1;
    gi = sizeG - 1;
    while (fi >= 0 || gi >= 0) {
        int indexF = fi >= 0 ? supportF[fi] : -1;
        int indexG = gi >= 0 ? supportG[gi] : -1;
        int index = ddMax(indexF, indexG);
        DdNode *var = dd->vars[index];
#ifdef DD_DEBUG
        assert(index >= 0);
#endif
        if (indexF == indexG) {
            DdNode *tmp = Cudd_bddAnd(dd,*common,var);
	    if (tmp == NULL) {
		Cudd_RecursiveDeref(dd,*common);
		Cudd_RecursiveDeref(dd,*onlyF);
		Cudd_RecursiveDeref(dd,*onlyG);
		FREE(supportF); FREE(supportG);
		return(0);
	    }
	    cuddRef(tmp);
	    Cudd_RecursiveDeref(dd,*common);
	    *common = tmp;
            fi--;
            gi--;
        } else if (index == indexF) {
	    DdNode *tmp = Cudd_bddAnd(dd,*onlyF,var);
	    if (tmp == NULL) {
		Cudd_RecursiveDeref(dd,*common);
		Cudd_RecursiveDeref(dd,*onlyF);
		Cudd_RecursiveDeref(dd,*onlyG);
		FREE(supportF); FREE(supportG);
		return(0);
	    }
	    cuddRef(tmp);
	    Cudd_RecursiveDeref(dd,*onlyF);
	    *onlyF = tmp;
            fi--;
        } else { /* index == indexG */
	    DdNode *tmp = Cudd_bddAnd(dd,*onlyG,var);
	    if (tmp == NULL) {
		Cudd_RecursiveDeref(dd,*common);
		Cudd_RecursiveDeref(dd,*onlyF);
		Cudd_RecursiveDeref(dd,*onlyG);
		FREE(supportF); FREE(supportG);
		return(0);
	    }
	    cuddRef(tmp);
	    Cudd_RecursiveDeref(dd,*onlyG);
	    *onlyG = tmp;
            gi--;
        }
    }

    FREE(supportF); FREE(supportG);
    cuddDeref(*common); cuddDeref(*onlyF); cuddDeref(*onlyG);
    return(1);

} /* end of Cudd_ClassifySupport */


/**Function********************************************************************

  Synopsis    [Counts the number of leaves in a DD.]

  Description [Counts the number of leaves in a DD. Returns the number
  of leaves in the DD rooted at node if successful; CUDD_OUT_OF_MEM
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintDebug]

******************************************************************************/
int
Cudd_CountLeaves(
  DdNode * node)
{
    int	i;

    i = ddLeavesInt(Cudd_Regular(node));
    ddClearFlag(Cudd_Regular(node));
    return(i);

} /* end of Cudd_CountLeaves */


/**Function********************************************************************

  Synopsis    [Picks one on-set cube randomly from the given DD.]

  Description [Picks one on-set cube randomly from the given DD. The
  cube is written into an array of characters.  The array must have at
  least as many entries as there are variables. Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPickOneMinterm]

******************************************************************************/
int
Cudd_bddPickOneCube(
  DdManager * ddm,
  DdNode * node,
  char * string)
{
    DdNode *N, *T, *E;
    DdNode *one, *bzero;
    char   dir;
    int    i;

    if (string == NULL || node == NULL) return(0);

    /* The constant 0 function has no on-set cubes. */
    one = DD_ONE(ddm);
    bzero = Cudd_Not(one);
    if (node == bzero) return(0);

    for (i = 0; i < ddm->size; i++) string[i] = 2;

    for (;;) {

	if (node == one) break;

	N = Cudd_Regular(node);

	T = cuddT(N); E = cuddE(N);
	if (Cudd_IsComplement(node)) {
	    T = Cudd_Not(T); E = Cudd_Not(E);
	}
	if (T == bzero) {
	    string[N->index] = 0;
	    node = E;
	} else if (E == bzero) {
	    string[N->index] = 1;
	    node = T;
	} else {
	    dir = (char) ((Cudd_Random() & 0x2000) >> 13);
	    string[N->index] = dir;
	    node = dir ? T : E;
	}
    }
    return(1);

} /* end of Cudd_bddPickOneCube */


/**Function********************************************************************

  Synopsis    [Picks one on-set minterm randomly from the given DD.]

  Description [Picks one on-set minterm randomly from the given
  DD. The minterm is in terms of <code>vars</code>. The array
  <code>vars</code> should contain at least all variables in the
  support of <code>f</code>; if this condition is not met the minterm
  built by this procedure may not be contained in
  <code>f</code>. Builds a BDD for the minterm and returns a pointer
  to it if successful; NULL otherwise. There are three reasons why the
  procedure may fail:
  <ul>
  <li> It may run out of memory;
  <li> the function <code>f</code> may be the constant 0;
  <li> the minterm may not be contained in <code>f</code>.
  </ul>]

  SideEffects [None]

  SeeAlso     [Cudd_bddPickOneCube]

******************************************************************************/
DdNode *
Cudd_bddPickOneMinterm(
  DdManager * dd /* manager */,
  DdNode * f /* function from which to pick one minterm */,
  DdNode ** vars /* array of variables */,
  int  n /* size of <code>vars</code> */)
{
    char *string;
    int i, size;
    int *indices;
    int result;
    DdNode *old, *neW;

    size = dd->size;
    string = ALLOC(char, size);
    if (string == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    indices = ALLOC(int,n);
    if (indices == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(string);
	return(NULL);
    }

    for (i = 0; i < n; i++) {
	indices[i] = vars[i]->index;
    }

    result = Cudd_bddPickOneCube(dd,f,string);
    if (result == 0) {
	FREE(string);
	FREE(indices);
	return(NULL);
    }

    /* Randomize choice for don't cares. */
    for (i = 0; i < n; i++) {
	if (string[indices[i]] == 2)
	    string[indices[i]] = (char) ((Cudd_Random() & 0x20) >> 5);
    }

    /* Build result BDD. */
    old = Cudd_ReadOne(dd);
    cuddRef(old);

    for (i = n-1; i >= 0; i--) {
	neW = Cudd_bddAnd(dd,old,Cudd_NotCond(vars[i],string[indices[i]]==0));
	if (neW == NULL) {
	    FREE(string);
	    FREE(indices);
	    Cudd_RecursiveDeref(dd,old);
	    return(NULL);
	}
	cuddRef(neW);
	Cudd_RecursiveDeref(dd,old);
	old = neW;
    }

#ifdef DD_DEBUG
    /* Test. */
    if (Cudd_bddLeq(dd,old,f)) {
	cuddDeref(old);
    } else {
	Cudd_RecursiveDeref(dd,old);
	old = NULL;
    }
#else
    cuddDeref(old);
#endif

    FREE(string);
    FREE(indices);
    return(old);

}  /* end of Cudd_bddPickOneMinterm */


/**Function********************************************************************

  Synopsis    [Picks k on-set minterms evenly distributed from given DD.]

  Description [Picks k on-set minterms evenly distributed from given DD.
  The minterms are in terms of <code>vars</code>. The array
  <code>vars</code> should contain at least all variables in the
  support of <code>f</code>; if this condition is not met the minterms
  built by this procedure may not be contained in
  <code>f</code>. Builds an array of BDDs for the minterms and returns a
  pointer to it if successful; NULL otherwise. There are three reasons
  why the procedure may fail:
  <ul>
  <li> It may run out of memory;
  <li> the function <code>f</code> may be the constant 0;
  <li> the minterms may not be contained in <code>f</code>.
  </ul>]

  SideEffects [None]

  SeeAlso     [Cudd_bddPickOneMinterm Cudd_bddPickOneCube]

******************************************************************************/
DdNode **
Cudd_bddPickArbitraryMinterms(
  DdManager * dd /* manager */,
  DdNode * f /* function from which to pick k minterms */,
  DdNode ** vars /* array of variables */,
  int  n /* size of <code>vars</code> */,
  int  k /* number of minterms to find */)
{
    char **string;
    int i, j, l, size;
    int *indices;
    int result;
    DdNode **old, *neW;
    double minterms;
    char *saveString;
    int saveFlag, savePoint, isSame;

    minterms = Cudd_CountMinterm(dd,f,n);
    if ((double)k > minterms) {
	return(NULL);
    }

    size = dd->size;
    string = ALLOC(char *, k);
    if (string == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < k; i++) {
	string[i] = ALLOC(char, size + 1);
	if (string[i] == NULL) {
	    for (j = 0; j < i; j++)
		FREE(string[i]);
	    FREE(string);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(NULL);
	}
	for (j = 0; j < size; j++) string[i][j] = '2';
	string[i][size] = '\0';
    }
    indices = ALLOC(int,n);
    if (indices == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	for (i = 0; i < k; i++)
	    FREE(string[i]);
	FREE(string);
	return(NULL);
    }

    for (i = 0; i < n; i++) {
	indices[i] = vars[i]->index;
    }

    result = ddPickArbitraryMinterms(dd,f,n,k,string);
    if (result == 0) {
	for (i = 0; i < k; i++)
	    FREE(string[i]);
	FREE(string);
	FREE(indices);
	return(NULL);
    }

    old = ALLOC(DdNode *, k);
    if (old == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	for (i = 0; i < k; i++)
	    FREE(string[i]);
	FREE(string);
	FREE(indices);
	return(NULL);
    }
    saveString = ALLOC(char, size + 1);
    if (saveString == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	for (i = 0; i < k; i++)
	    FREE(string[i]);
	FREE(string);
	FREE(indices);
	FREE(old);
	return(NULL);
    }
    saveFlag = 0;

    /* Build result BDD array. */
    for (i = 0; i < k; i++) {
	isSame = 0;
	if (!saveFlag) {
	    for (j = i + 1; j < k; j++) {
		if (strcmp(string[i], string[j]) == 0) {
		    savePoint = i;
		    strcpy(saveString, string[i]);
		    saveFlag = 1;
		    break;
		}
	    }
	} else {
	    if (strcmp(string[i], saveString) == 0) {
		isSame = 1;
	    } else {
		saveFlag = 0;
		for (j = i + 1; j < k; j++) {
		    if (strcmp(string[i], string[j]) == 0) {
			savePoint = i;
			strcpy(saveString, string[i]);
			saveFlag = 1;
			break;
		    }
		}
	    }
	}
	/* Randomize choice for don't cares. */
	for (j = 0; j < n; j++) {
	    if (string[i][indices[j]] == '2')
		string[i][indices[j]] =
		  (char) ((Cudd_Random() & 0x20) ? '1' : '0');
	}

	while (isSame) {
	    isSame = 0;
	    for (j = savePoint; j < i; j++) {
		if (strcmp(string[i], string[j]) == 0) {
		    isSame = 1;
		    break;
		}
	    }
	    if (isSame) {
		strcpy(string[i], saveString);
		/* Randomize choice for don't cares. */
		for (j = 0; j < n; j++) {
		    if (string[i][indices[j]] == '2')
			string[i][indices[j]] =
			  (char) ((Cudd_Random() & 0x20) ? '1' : '0');
		}
	    }
	}

	old[i] = Cudd_ReadOne(dd);
	cuddRef(old[i]);

	for (j = 0; j < n; j++) {
	    if (string[i][indices[j]] == '0') {
		neW = Cudd_bddAnd(dd,old[i],Cudd_Not(vars[j]));
	    } else {
		neW = Cudd_bddAnd(dd,old[i],vars[j]);
	    }
	    if (neW == NULL) {
		FREE(saveString);
		for (l = 0; l < k; l++)
		    FREE(string[l]);
		FREE(string);
		FREE(indices);
		for (l = 0; l <= i; l++)
		    Cudd_RecursiveDeref(dd,old[l]);
		FREE(old);
		return(NULL);
	    }
	    cuddRef(neW);
	    Cudd_RecursiveDeref(dd,old[i]);
	    old[i] = neW;
	}

	/* Test. */
	if (!Cudd_bddLeq(dd,old[i],f)) {
	    FREE(saveString);
	    for (l = 0; l < k; l++)
		FREE(string[l]);
	    FREE(string);
	    FREE(indices);
	    for (l = 0; l <= i; l++)
		Cudd_RecursiveDeref(dd,old[l]);
	    FREE(old);
	    return(NULL);
	}
    }

    FREE(saveString);
    for (i = 0; i < k; i++) {
	cuddDeref(old[i]);
	FREE(string[i]);
    }
    FREE(string);
    FREE(indices);
    return(old);

}  /* end of Cudd_bddPickArbitraryMinterms */


/**Function********************************************************************

  Synopsis    [Extracts a subset from a BDD.]

  Description [Extracts a subset from a BDD in the following procedure.
  1. Compute the weight for each mask variable by counting the number of
     minterms for both positive and negative cofactors of the BDD with
     respect to each mask variable. (weight = #positive - #negative)
  2. Find a representative cube of the BDD by using the weight. From the
     top variable of the BDD, for each variable, if the weight is greater
     than 0.0, choose THEN branch, othereise ELSE branch, until meeting
     the constant 1.
  3. Quantify out the variables not in maskVars from the representative
     cube and if a variable in maskVars is don't care, replace the
     variable with a constant(1 or 0) depending on the weight.
  4. Make a subset of the BDD by multiplying with the modified cube.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_SubsetWithMaskVars(
  DdManager * dd /* manager */,
  DdNode * f /* function from which to pick a cube */,
  DdNode ** vars /* array of variables */,
  int  nvars /* size of <code>vars</code> */,
  DdNode ** maskVars /* array of variables */,
  int  mvars /* size of <code>maskVars</code> */)
{
    double	*weight;
    char	*string;
    int		i, size;
    int		*indices, *mask;
    int		result;
    DdNode	*zero, *cube, *newCube, *subset;
    DdNode	*cof;

    DdNode	*support;
    support = Cudd_Support(dd,f);
    cuddRef(support);
    Cudd_RecursiveDeref(dd,support);

    zero = Cudd_Not(dd->one);
    size = dd->size;

    weight = ALLOC(double,size);
    if (weight == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < size; i++) {
	weight[i] = 0.0;
    }
    for (i = 0; i < mvars; i++) {
	cof = Cudd_Cofactor(dd, f, maskVars[i]);
	cuddRef(cof);
	weight[i] = Cudd_CountMinterm(dd, cof, nvars);
	Cudd_RecursiveDeref(dd,cof);

	cof = Cudd_Cofactor(dd, f, Cudd_Not(maskVars[i]));
	cuddRef(cof);
	weight[i] -= Cudd_CountMinterm(dd, cof, nvars);
	Cudd_RecursiveDeref(dd,cof);
    }

    string = ALLOC(char, size + 1);
    if (string == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(weight);
	return(NULL);
    }
    mask = ALLOC(int, size);
    if (mask == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(weight);
	FREE(string);
	return(NULL);
    }
    for (i = 0; i < size; i++) {
	string[i] = '2';
	mask[i] = 0;
    }
    string[size] = '\0';
    indices = ALLOC(int,nvars);
    if (indices == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(weight);
	FREE(string);
	FREE(mask);
	return(NULL);
    }
    for (i = 0; i < nvars; i++) {
	indices[i] = vars[i]->index;
    }

    result = ddPickRepresentativeCube(dd,f,weight,string);
    if (result == 0) {
	FREE(weight);
	FREE(string);
	FREE(mask);
	FREE(indices);
	return(NULL);
    }

    cube = Cudd_ReadOne(dd);
    cuddRef(cube);
    zero = Cudd_Not(Cudd_ReadOne(dd));
    for (i = 0; i < nvars; i++) {
	if (string[indices[i]] == '0') {
	    newCube = Cudd_bddIte(dd,cube,Cudd_Not(vars[i]),zero);
	} else if (string[indices[i]] == '1') {
	    newCube = Cudd_bddIte(dd,cube,vars[i],zero);
	} else
	    continue;
	if (newCube == NULL) {
	    FREE(weight);
	    FREE(string);
	    FREE(mask);
	    FREE(indices);
	    Cudd_RecursiveDeref(dd,cube);
	    return(NULL);
	}
	cuddRef(newCube);
	Cudd_RecursiveDeref(dd,cube);
	cube = newCube;
    }
    Cudd_RecursiveDeref(dd,cube);

    for (i = 0; i < mvars; i++) {
	mask[maskVars[i]->index] = 1;
    }
    for (i = 0; i < nvars; i++) {
	if (mask[indices[i]]) {
	    if (string[indices[i]] == '2') {
		if (weight[indices[i]] >= 0.0)
		    string[indices[i]] = '1';
		else
		    string[indices[i]] = '0';
	    }
	} else {
	    string[indices[i]] = '2';
	}
    }

    cube = Cudd_ReadOne(dd);
    cuddRef(cube);
    zero = Cudd_Not(Cudd_ReadOne(dd));

    /* Build result BDD. */
    for (i = 0; i < nvars; i++) {
	if (string[indices[i]] == '0') {
	    newCube = Cudd_bddIte(dd,cube,Cudd_Not(vars[i]),zero);
	} else if (string[indices[i]] == '1') {
	    newCube = Cudd_bddIte(dd,cube,vars[i],zero);
	} else
	    continue;
	if (newCube == NULL) {
	    FREE(weight);
	    FREE(string);
	    FREE(mask);
	    FREE(indices);
	    Cudd_RecursiveDeref(dd,cube);
	    return(NULL);
	}
	cuddRef(newCube);
	Cudd_RecursiveDeref(dd,cube);
	cube = newCube;
    }

    subset = Cudd_bddAnd(dd,f,cube);
    cuddRef(subset);
    Cudd_RecursiveDeref(dd,cube);

    /* Test. */
    if (Cudd_bddLeq(dd,subset,f)) {
	cuddDeref(subset);
    } else {
	Cudd_RecursiveDeref(dd,subset);
	subset = NULL;
    }

    FREE(weight);
    FREE(string);
    FREE(mask);
    FREE(indices);
    return(subset);

} /* end of Cudd_SubsetWithMaskVars */


/**Function********************************************************************

  Synopsis    [Finds the first cube of a decision diagram.]

  Description [Defines an iterator on the onset of a decision diagram
  and finds its first cube. Returns a generator that contains the
  information necessary to continue the enumeration if successful; NULL
  otherwise.<p>
  A cube is represented as an array of literals, which are integers in
  {0, 1, 2}; 0 represents a complemented literal, 1 represents an
  uncomplemented literal, and 2 stands for don't care. The enumeration
  produces a disjoint cover of the function associated with the diagram.
  The size of the array equals the number of variables in the manager at
  the time Cudd_FirstCube is called.<p>
  For each cube, a value is also returned. This value is always 1 for a
  BDD, while it may be different from 1 for an ADD.
  For BDDs, the offset is the set of cubes whose value is the logical zero.
  For ADDs, the offset is the set of cubes whose value is the
  background value. The cubes of the offset are not enumerated.]

  SideEffects [The first cube and its value are returned as side effects.]

  SeeAlso     [Cudd_ForeachCube Cudd_NextCube Cudd_GenFree Cudd_IsGenEmpty
  Cudd_FirstNode]

******************************************************************************/
DdGen *
Cudd_FirstCube(
  DdManager * dd,
  DdNode * f,
  int ** cube,
  CUDD_VALUE_TYPE * value)
{
    DdGen *gen;
    DdNode *top, *treg, *next, *nreg, *prev, *preg;
    int i;
    int nvars;

    /* Sanity Check. */
    if (dd == NULL || f == NULL) return(NULL);

    /* Allocate generator an initialize it. */
    gen = ALLOC(DdGen,1);
    if (gen == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    gen->manager = dd;
    gen->type = CUDD_GEN_CUBES;
    gen->status = CUDD_GEN_EMPTY;
    gen->gen.cubes.cube = NULL;
    gen->gen.cubes.value = DD_ZERO_VAL;
    gen->stack.sp = 0;
    gen->stack.stack = NULL;
    gen->node = NULL;

    nvars = dd->size;
    gen->gen.cubes.cube = ALLOC(int,nvars);
    if (gen->gen.cubes.cube == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(gen);
	return(NULL);
    }
    for (i = 0; i < nvars; i++) gen->gen.cubes.cube[i] = 2;

    /* The maximum stack depth is one plus the number of variables.
    ** because a path may have nodes at all levels, including the
    ** constant level.
    */
    gen->stack.stack = ALLOC(DdNodePtr, nvars+1);
    if (gen->stack.stack == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(gen->gen.cubes.cube);
	FREE(gen);
	return(NULL);
    }
    for (i = 0; i <= nvars; i++) gen->stack.stack[i] = NULL;

    /* Find the first cube of the onset. */
    gen->stack.stack[gen->stack.sp] = f; gen->stack.sp++;

    while (1) {
	top = gen->stack.stack[gen->stack.sp-1];
	treg = Cudd_Regular(top);
	if (!cuddIsConstant(treg)) {
	    /* Take the else branch first. */
	    gen->gen.cubes.cube[treg->index] = 0;
	    next = cuddE(treg);
	    if (top != treg) next = Cudd_Not(next);
	    gen->stack.stack[gen->stack.sp] = next; gen->stack.sp++;
	} else if (top == Cudd_Not(DD_ONE(dd)) || top == dd->background) {
	    /* Backtrack */
	    while (1) {
		if (gen->stack.sp == 1) {
		    /* The current node has no predecessor. */
		    gen->status = CUDD_GEN_EMPTY;
		    gen->stack.sp--;
		    goto done;
		}
		prev = gen->stack.stack[gen->stack.sp-2];
		preg = Cudd_Regular(prev);
		nreg = cuddT(preg);
		if (prev != preg) {next = Cudd_Not(nreg);} else {next = nreg;}
		if (next != top) { /* follow the then branch next */
		    gen->gen.cubes.cube[preg->index] = 1;
		    gen->stack.stack[gen->stack.sp-1] = next;
		    break;
		}
		/* Pop the stack and try again. */
		gen->gen.cubes.cube[preg->index] = 2;
		gen->stack.sp--;
		top = gen->stack.stack[gen->stack.sp-1];
		treg = Cudd_Regular(top);
	    }
	} else {
	    gen->status = CUDD_GEN_NONEMPTY;
	    gen->gen.cubes.value = cuddV(top);
	    goto done;
	}
    }

done:
    *cube = gen->gen.cubes.cube;
    *value = gen->gen.cubes.value;
    return(gen);

} /* end of Cudd_FirstCube */


/**Function********************************************************************

  Synopsis    [Generates the next cube of a decision diagram onset.]

  Description [Generates the next cube of a decision diagram onset,
  using generator gen. Returns 0 if the enumeration is completed; 1
  otherwise.]

  SideEffects [The cube and its value are returned as side effects. The
  generator is modified.]

  SeeAlso     [Cudd_ForeachCube Cudd_FirstCube Cudd_GenFree Cudd_IsGenEmpty
  Cudd_NextNode]

******************************************************************************/
int
Cudd_NextCube(
  DdGen * gen,
  int ** cube,
  CUDD_VALUE_TYPE * value)
{
    DdNode *top, *treg, *next, *nreg, *prev, *preg;
    DdManager *dd = gen->manager;

    /* Backtrack from previously reached terminal node. */
    while (1) {
	if (gen->stack.sp == 1) {
	    /* The current node has no predecessor. */
	    gen->status = CUDD_GEN_EMPTY;
	    gen->stack.sp--;
	    goto done;
	}
	top = gen->stack.stack[gen->stack.sp-1];
	treg = Cudd_Regular(top);
	prev = gen->stack.stack[gen->stack.sp-2];
	preg = Cudd_Regular(prev);
	nreg = cuddT(preg);
	if (prev != preg) {next = Cudd_Not(nreg);} else {next = nreg;}
	if (next != top) { /* follow the then branch next */
	    gen->gen.cubes.cube[preg->index] = 1;
	    gen->stack.stack[gen->stack.sp-1] = next;
	    break;
	}
	/* Pop the stack and try again. */
	gen->gen.cubes.cube[preg->index] = 2;
	gen->stack.sp--;
    }

    while (1) {
	top = gen->stack.stack[gen->stack.sp-1];
	treg = Cudd_Regular(top);
	if (!cuddIsConstant(treg)) {
	    /* Take the else branch first. */
	    gen->gen.cubes.cube[treg->index] = 0;
	    next = cuddE(treg);
	    if (top != treg) next = Cudd_Not(next);
	    gen->stack.stack[gen->stack.sp] = next; gen->stack.sp++;
	} else if (top == Cudd_Not(DD_ONE(dd)) || top == dd->background) {
	    /* Backtrack */
	    while (1) {
		if (gen->stack.sp == 1) {
		    /* The current node has no predecessor. */
		    gen->status = CUDD_GEN_EMPTY;
		    gen->stack.sp--;
		    goto done;
		}
		prev = gen->stack.stack[gen->stack.sp-2];
		preg = Cudd_Regular(prev);
		nreg = cuddT(preg);
		if (prev != preg) {next = Cudd_Not(nreg);} else {next = nreg;}
		if (next != top) { /* follow the then branch next */
		    gen->gen.cubes.cube[preg->index] = 1;
		    gen->stack.stack[gen->stack.sp-1] = next;
		    break;
		}
		/* Pop the stack and try again. */
		gen->gen.cubes.cube[preg->index] = 2;
		gen->stack.sp--;
		top = gen->stack.stack[gen->stack.sp-1];
		treg = Cudd_Regular(top);
	    }
	} else {
	    gen->status = CUDD_GEN_NONEMPTY;
	    gen->gen.cubes.value = cuddV(top);
	    goto done;
	}
    }

done:
    if (gen->status == CUDD_GEN_EMPTY) return(0);
    *cube = gen->gen.cubes.cube;
    *value = gen->gen.cubes.value;
    return(1);

} /* end of Cudd_NextCube */


/**Function********************************************************************

  Synopsis    [Finds the first prime of a Boolean function.]

  Description [Defines an iterator on a pair of BDDs describing a
  (possibly incompletely specified) Boolean functions and finds the
  first cube of a cover of the function.  Returns a generator
  that contains the information necessary to continue the enumeration
  if successful; NULL otherwise.<p>

  The two argument BDDs are the lower and upper bounds of an interval.
  It is a mistake to call this function with a lower bound that is not
  less than or equal to the upper bound.<p>

  A cube is represented as an array of literals, which are integers in
  {0, 1, 2}; 0 represents a complemented literal, 1 represents an
  uncomplemented literal, and 2 stands for don't care. The enumeration
  produces a prime and irredundant cover of the function associated
  with the two BDDs.  The size of the array equals the number of
  variables in the manager at the time Cudd_FirstCube is called.<p>

  This iterator can only be used on BDDs.]

  SideEffects [The first cube is returned as side effect.]

  SeeAlso     [Cudd_ForeachPrime Cudd_NextPrime Cudd_GenFree Cudd_IsGenEmpty
  Cudd_FirstCube Cudd_FirstNode]

******************************************************************************/
DdGen *
Cudd_FirstPrime(
  DdManager *dd,
  DdNode *l,
  DdNode *u,
  int **cube)
{
    DdGen *gen;
    DdNode *implicant, *prime, *tmp;
    int length, result;

    /* Sanity Check. */
    if (dd == NULL || l == NULL || u == NULL) return(NULL);

    /* Allocate generator an initialize it. */
    gen = ALLOC(DdGen,1);
    if (gen == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    gen->manager = dd;
    gen->type = CUDD_GEN_PRIMES;
    gen->status = CUDD_GEN_EMPTY;
    gen->gen.primes.cube = NULL;
    gen->gen.primes.ub = u;
    gen->stack.sp = 0;
    gen->stack.stack = NULL;
    gen->node = l;
    cuddRef(l);

    gen->gen.primes.cube = ALLOC(int,dd->size);
    if (gen->gen.primes.cube == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	FREE(gen);
	return(NULL);
    }

    if (gen->node == Cudd_ReadLogicZero(dd)) {
	gen->status = CUDD_GEN_EMPTY;
    } else {
	implicant = Cudd_LargestCube(dd,gen->node,&length);
	if (implicant == NULL) {
	    Cudd_RecursiveDeref(dd,gen->node);
	    FREE(gen->gen.primes.cube);
	    FREE(gen);
	    return(NULL);
	}
	cuddRef(implicant);
	prime = Cudd_bddMakePrime(dd,implicant,gen->gen.primes.ub);
	if (prime == NULL) {
	    Cudd_RecursiveDeref(dd,gen->node);
	    Cudd_RecursiveDeref(dd,implicant);
	    FREE(gen->gen.primes.cube);
	    FREE(gen);
	    return(NULL);
	}
	cuddRef(prime);
	Cudd_RecursiveDeref(dd,implicant);
	tmp = Cudd_bddAnd(dd,gen->node,Cudd_Not(prime));
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,gen->node);
	    Cudd_RecursiveDeref(dd,prime);
	    FREE(gen->gen.primes.cube);
	    FREE(gen);
	    return(NULL);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,gen->node);
	gen->node = tmp;
	result = Cudd_BddToCubeArray(dd,prime,gen->gen.primes.cube);
	if (result == 0) {
	    Cudd_RecursiveDeref(dd,gen->node);
	    Cudd_RecursiveDeref(dd,prime);
	    FREE(gen->gen.primes.cube);
	    FREE(gen);
	    return(NULL);
	}
	Cudd_RecursiveDeref(dd,prime);
	gen->status = CUDD_GEN_NONEMPTY;
    }
    *cube = gen->gen.primes.cube;
    return(gen);

} /* end of Cudd_FirstPrime */


/**Function********************************************************************

  Synopsis    [Generates the next prime of a Boolean function.]

  Description [Generates the next cube of a Boolean function,
  using generator gen. Returns 0 if the enumeration is completed; 1
  otherwise.]

  SideEffects [The cube and is returned as side effects. The
  generator is modified.]

  SeeAlso     [Cudd_ForeachPrime Cudd_FirstPrime Cudd_GenFree Cudd_IsGenEmpty
  Cudd_NextCube Cudd_NextNode]

******************************************************************************/
int
Cudd_NextPrime(
  DdGen *gen,
  int **cube)
{
    DdNode *implicant, *prime, *tmp;
    DdManager *dd = gen->manager;
    int length, result;

    if (gen->node == Cudd_ReadLogicZero(dd)) {
	gen->status = CUDD_GEN_EMPTY;
    } else {
	implicant = Cudd_LargestCube(dd,gen->node,&length);
	if (implicant == NULL) {
	    gen->status = CUDD_GEN_EMPTY;
	    return(0);
	}
	cuddRef(implicant);
	prime = Cudd_bddMakePrime(dd,implicant,gen->gen.primes.ub);
	if (prime == NULL) {
	    Cudd_RecursiveDeref(dd,implicant);
	    gen->status = CUDD_GEN_EMPTY;
	    return(0);
	}
	cuddRef(prime);
	Cudd_RecursiveDeref(dd,implicant);
	tmp = Cudd_bddAnd(dd,gen->node,Cudd_Not(prime));
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,prime);
	    gen->status = CUDD_GEN_EMPTY;
	    return(0);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,gen->node);
	gen->node = tmp;
	result = Cudd_BddToCubeArray(dd,prime,gen->gen.primes.cube);
	if (result == 0) {
	    Cudd_RecursiveDeref(dd,prime);
	    gen->status = CUDD_GEN_EMPTY;
	    return(0);
	}
	Cudd_RecursiveDeref(dd,prime);
	gen->status = CUDD_GEN_NONEMPTY;
    }
    if (gen->status == CUDD_GEN_EMPTY) return(0);
    *cube = gen->gen.primes.cube;
    return(1);

} /* end of Cudd_NextPrime */


/**Function********************************************************************

  Synopsis    [Computes the cube of an array of BDD variables.]

  Description [Computes the cube of an array of BDD variables. If
  non-null, the phase argument indicates which literal of each
  variable should appear in the cube. If phase\[i\] is nonzero, then the
  positive literal is used. If phase is NULL, the cube is positive unate.
  Returns a pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addComputeCube Cudd_IndicesToCube Cudd_CubeArrayToBdd]

******************************************************************************/
DdNode *
Cudd_bddComputeCube(
  DdManager * dd,
  DdNode ** vars,
  int * phase,
  int  n)
{
    DdNode	*cube;
    DdNode	*fn;
    int         i;

    cube = DD_ONE(dd);
    cuddRef(cube);

    for (i = n - 1; i >= 0; i--) {
	if (phase == NULL || phase[i] != 0) {
	    fn = Cudd_bddAnd(dd,vars[i],cube);
	} else {
	    fn = Cudd_bddAnd(dd,Cudd_Not(vars[i]),cube);
	}
	if (fn == NULL) {
	    Cudd_RecursiveDeref(dd,cube);
	    return(NULL);
	}
	cuddRef(fn);
	Cudd_RecursiveDeref(dd,cube);
	cube = fn;
    }
    cuddDeref(cube);

    return(cube);

}  /* end of Cudd_bddComputeCube */


/**Function********************************************************************

  Synopsis    [Computes the cube of an array of ADD variables.]

  Description [Computes the cube of an array of ADD variables.  If
  non-null, the phase argument indicates which literal of each
  variable should appear in the cube. If phase\[i\] is nonzero, then the
  positive literal is used. If phase is NULL, the cube is positive unate.
  Returns a pointer to the result if successful; NULL otherwise.]

  SideEffects [none]

  SeeAlso     [Cudd_bddComputeCube]

******************************************************************************/
DdNode *
Cudd_addComputeCube(
  DdManager * dd,
  DdNode ** vars,
  int * phase,
  int  n)
{
    DdNode	*cube, *zero;
    DdNode	*fn;
    int         i;

    cube = DD_ONE(dd);
    cuddRef(cube);
    zero = DD_ZERO(dd);

    for (i = n - 1; i >= 0; i--) {
	if (phase == NULL || phase[i] != 0) {
	    fn = Cudd_addIte(dd,vars[i],cube,zero);
	} else {
	    fn = Cudd_addIte(dd,vars[i],zero,cube);
	}
	if (fn == NULL) {
	    Cudd_RecursiveDeref(dd,cube);
	    return(NULL);
	}
	cuddRef(fn);
	Cudd_RecursiveDeref(dd,cube);
	cube = fn;
    }
    cuddDeref(cube);

    return(cube);

} /* end of Cudd_addComputeCube */


/**Function********************************************************************

  Synopsis    [Builds the BDD of a cube from a positional array.]

  Description [Builds a cube from a positional array.  The array must
  have one integer entry for each BDD variable.  If the i-th entry is
  1, the variable of index i appears in true form in the cube; If the
  i-th entry is 0, the variable of index i appears complemented in the
  cube; otherwise the variable does not appear in the cube.  Returns a
  pointer to the BDD for the cube if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddComputeCube Cudd_IndicesToCube Cudd_BddToCubeArray]

******************************************************************************/
DdNode *
Cudd_CubeArrayToBdd(
  DdManager *dd,
  int *array)
{
    DdNode *cube, *var, *tmp;
    int i;
    int size = Cudd_ReadSize(dd);

    cube = DD_ONE(dd);
    cuddRef(cube);
    for (i = size - 1; i >= 0; i--) {
	if ((array[i] & ~1) == 0) {
	    var = Cudd_bddIthVar(dd,i);
	    tmp = Cudd_bddAnd(dd,cube,Cudd_NotCond(var,array[i]==0));
	    if (tmp == NULL) {
		Cudd_RecursiveDeref(dd,cube);
		return(NULL);
	    }
	    cuddRef(tmp);
	    Cudd_RecursiveDeref(dd,cube);
	    cube = tmp;
	}
    }
    cuddDeref(cube);
    return(cube);

} /* end of Cudd_CubeArrayToBdd */


/**Function********************************************************************

  Synopsis    [Builds a positional array from the BDD of a cube.]

  Description [Builds a positional array from the BDD of a cube.
  Array must have one entry for each BDD variable.  The positional
  array has 1 in i-th position if the variable of index i appears in
  true form in the cube; it has 0 in i-th position if the variable of
  index i appears in complemented form in the cube; finally, it has 2
  in i-th position if the variable of index i does not appear in the
  cube.  Returns 1 if successful (the BDD is indeed a cube); 0
  otherwise.]

  SideEffects [The result is in the array passed by reference.]

  SeeAlso     [Cudd_CubeArrayToBdd]

******************************************************************************/
int
Cudd_BddToCubeArray(
  DdManager *dd,
  DdNode *cube,
  int *array)
{
    DdNode *scan, *t, *e;
    int i;
    int size = Cudd_ReadSize(dd);
    DdNode *zero = Cudd_Not(DD_ONE(dd));

    for (i = size-1; i >= 0; i--) {
	array[i] = 2;
    }
    scan = cube;
    while (!Cudd_IsConstant(scan)) {
	int index = Cudd_Regular(scan)->index;
	cuddGetBranches(scan,&t,&e);
	if (t == zero) {
	    array[index] = 0;
	    scan = e;
	} else if (e == zero) {
	    array[index] = 1;
	    scan = t;
	} else {
	    return(0);	/* cube is not a cube */
	}
    }
    if (scan == zero) {
	return(0);
    } else {
	return(1);
    }

} /* end of Cudd_BddToCubeArray */


/**Function********************************************************************

  Synopsis    [Finds the first node of a decision diagram.]

  Description [Defines an iterator on the nodes of a decision diagram
  and finds its first node. Returns a generator that contains the
  information necessary to continue the enumeration if successful;
  NULL otherwise.  The nodes are enumerated in a reverse topological
  order, so that a node is always preceded in the enumeration by its
  descendants.]

  SideEffects [The first node is returned as a side effect.]

  SeeAlso     [Cudd_ForeachNode Cudd_NextNode Cudd_GenFree Cudd_IsGenEmpty
  Cudd_FirstCube]

******************************************************************************/
DdGen *
Cudd_FirstNode(
  DdManager * dd,
  DdNode * f,
  DdNode ** node)
{
    DdGen *gen;
    int size;

    /* Sanity Check. */
    if (dd == NULL || f == NULL) return(NULL);

    /* Allocate generator an initialize it. */
    gen = ALLOC(DdGen,1);
    if (gen == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    gen->manager = dd;
    gen->type = CUDD_GEN_NODES;
    gen->status = CUDD_GEN_EMPTY;
    gen->stack.sp = 0;
    gen->node = NULL;

    /* Collect all the nodes on the generator stack for later perusal. */
    gen->stack.stack = cuddNodeArray(Cudd_Regular(f), &size);
    if (gen->stack.stack == NULL) {
	FREE(gen);
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    gen->gen.nodes.size = size;

    /* Find the first node. */
    if (gen->stack.sp < gen->gen.nodes.size) {
	gen->status = CUDD_GEN_NONEMPTY;
	gen->node = gen->stack.stack[gen->stack.sp];
	*node = gen->node;
    }

    return(gen);

} /* end of Cudd_FirstNode */


/**Function********************************************************************

  Synopsis    [Finds the next node of a decision diagram.]

  Description [Finds the node of a decision diagram, using generator
  gen. Returns 0 if the enumeration is completed; 1 otherwise.]

  SideEffects [The next node is returned as a side effect.]

  SeeAlso     [Cudd_ForeachNode Cudd_FirstNode Cudd_GenFree Cudd_IsGenEmpty
  Cudd_NextCube]

******************************************************************************/
int
Cudd_NextNode(
  DdGen * gen,
  DdNode ** node)
{
    /* Find the next node. */
    gen->stack.sp++;
    if (gen->stack.sp < gen->gen.nodes.size) {
	gen->node = gen->stack.stack[gen->stack.sp];
	*node = gen->node;
	return(1);
    } else {
	gen->status = CUDD_GEN_EMPTY;
	return(0);
    }

} /* end of Cudd_NextNode */


/**Function********************************************************************

  Synopsis    [Frees a CUDD generator.]

  Description [Frees a CUDD generator. Always returns 0, so that it can
  be used in mis-like foreach constructs.]

  SideEffects [None]

  SeeAlso     [Cudd_ForeachCube Cudd_ForeachNode Cudd_FirstCube Cudd_NextCube
  Cudd_FirstNode Cudd_NextNode Cudd_IsGenEmpty]

******************************************************************************/
int
Cudd_GenFree(
  DdGen * gen)
{
    if (gen == NULL) return(0);
    switch (gen->type) {
    case CUDD_GEN_CUBES:
    case CUDD_GEN_ZDD_PATHS:
	FREE(gen->gen.cubes.cube);
	FREE(gen->stack.stack);
	break;
    case CUDD_GEN_PRIMES:
	FREE(gen->gen.primes.cube);
	Cudd_RecursiveDeref(gen->manager,gen->node);
	break;
    case CUDD_GEN_NODES:
	FREE(gen->stack.stack);
	break;
    default:
	return(0);
    }
    FREE(gen);
    return(0);

} /* end of Cudd_GenFree */


/**Function********************************************************************

  Synopsis    [Queries the status of a generator.]

  Description [Queries the status of a generator. Returns 1 if the
  generator is empty or NULL; 0 otherswise.]

  SideEffects [None]

  SeeAlso     [Cudd_ForeachCube Cudd_ForeachNode Cudd_FirstCube Cudd_NextCube
  Cudd_FirstNode Cudd_NextNode Cudd_GenFree]

******************************************************************************/
int
Cudd_IsGenEmpty(
  DdGen * gen)
{
    if (gen == NULL) return(1);
    return(gen->status == CUDD_GEN_EMPTY);

} /* end of Cudd_IsGenEmpty */


/**Function********************************************************************

  Synopsis    [Builds a cube of BDD variables from an array of indices.]

  Description [Builds a cube of BDD variables from an array of indices.
  Returns a pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddComputeCube Cudd_CubeArrayToBdd]

******************************************************************************/
DdNode *
Cudd_IndicesToCube(
  DdManager * dd,
  int * array,
  int  n)
{
    DdNode *cube, *tmp;
    int i;

    cube = DD_ONE(dd);
    cuddRef(cube);
    for (i = n - 1; i >= 0; i--) {
	tmp = Cudd_bddAnd(dd,Cudd_bddIthVar(dd,array[i]),cube);
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,cube);
	    return(NULL);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,cube);
	cube = tmp;
    }

    cuddDeref(cube);
    return(cube);

} /* end of Cudd_IndicesToCube */


/**Function********************************************************************

  Synopsis    [Prints the package version number.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
Cudd_PrintVersion(
  FILE * fp)
{
    (void) fprintf(fp, "%s\n", CUDD_VERSION);

} /* end of Cudd_PrintVersion */


/**Function********************************************************************

  Synopsis    [Computes the average distance between adjacent nodes.]

  Description [Computes the average distance between adjacent nodes in
  the manager. Adjacent nodes are node pairs such that the second node
  is the then child, else child, or next node in the collision list.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
double
Cudd_AverageDistance(
  DdManager * dd)
{
    double tetotal, nexttotal;
    double tesubtotal, nextsubtotal;
    double temeasured, nextmeasured;
    int i, j;
    int slots, nvars;
    long diff;
    DdNode *scan;
    DdNodePtr *nodelist;
    DdNode *sentinel = &(dd->sentinel);

    nvars = dd->size;
    if (nvars == 0) return(0.0);

    /* Initialize totals. */
    tetotal = 0.0;
    nexttotal = 0.0;
    temeasured = 0.0;
    nextmeasured = 0.0;

    /* Scan the variable subtables. */
    for (i = 0; i < nvars; i++) {
	nodelist = dd->subtables[i].nodelist;
	tesubtotal = 0.0;
	nextsubtotal = 0.0;
	slots = dd->subtables[i].slots;
	for (j = 0; j < slots; j++) {
	    scan = nodelist[j];
	    while (scan != sentinel) {
		diff = (long) scan - (long) cuddT(scan);
		tesubtotal += (double) ddAbs(diff);
		diff = (long) scan - (long) Cudd_Regular(cuddE(scan));
		tesubtotal += (double) ddAbs(diff);
		temeasured += 2.0;
		if (scan->next != sentinel) {
		    diff = (long) scan - (long) scan->next;
		    nextsubtotal += (double) ddAbs(diff);
		    nextmeasured += 1.0;
		}
		scan = scan->next;
	    }
	}
	tetotal += tesubtotal;
	nexttotal += nextsubtotal;
    }

    /* Scan the constant table. */
    nodelist = dd->constants.nodelist;
    nextsubtotal = 0.0;
    slots = dd->constants.slots;
    for (j = 0; j < slots; j++) {
	scan = nodelist[j];
	while (scan != NULL) {
	    if (scan->next != NULL) {
		diff = (long) scan - (long) scan->next;
		nextsubtotal += (double) ddAbs(diff);
		nextmeasured += 1.0;
	    }
	    scan = scan->next;
	}
    }
    nexttotal += nextsubtotal;

    return((tetotal + nexttotal) / (temeasured + nextmeasured));

} /* end of Cudd_AverageDistance */


/**Function********************************************************************

  Synopsis    [Portable random number generator.]

  Description [Portable number generator based on ran2 from "Numerical
  Recipes in C." It is a long period (> 2 * 10^18) random number generator
  of L'Ecuyer with Bays-Durham shuffle. Returns a long integer uniformly
  distributed between 0 and 2147483561 (inclusive of the endpoint values).
  The random generator can be explicitly initialized by calling
  Cudd_Srandom. If no explicit initialization is performed, then the
  seed 1 is assumed.]

  SideEffects [None]

  SeeAlso     [Cudd_Srandom]

******************************************************************************/
long
Cudd_Random(void)
{
    int i;	/* index in the shuffle table */
    long int w; /* work variable */

    /* cuddRand == 0 if the geneartor has not been initialized yet. */
    if (cuddRand == 0) Cudd_Srandom(1);

    /* Compute cuddRand = (cuddRand * LEQA1) % MODULUS1 avoiding
    ** overflows by Schrage's method.
    */
    w          = cuddRand / LEQQ1;
    cuddRand   = LEQA1 * (cuddRand - w * LEQQ1) - w * LEQR1;
    cuddRand  += (cuddRand < 0) * MODULUS1;

    /* Compute cuddRand2 = (cuddRand2 * LEQA2) % MODULUS2 avoiding
    ** overflows by Schrage's method.
    */
    w          = cuddRand2 / LEQQ2;
    cuddRand2  = LEQA2 * (cuddRand2 - w * LEQQ2) - w * LEQR2;
    cuddRand2 += (cuddRand2 < 0) * MODULUS2;

    /* cuddRand is shuffled with the Bays-Durham algorithm.
    ** shuffleSelect and cuddRand2 are combined to generate the output.
    */

    /* Pick one element from the shuffle table; "i" will be in the range
    ** from 0 to STAB_SIZE-1.
    */
    i = (int) (shuffleSelect / STAB_DIV);
    /* Mix the element of the shuffle table with the current iterate of
    ** the second sub-generator, and replace the chosen element of the
    ** shuffle table with the current iterate of the first sub-generator.
    */
    shuffleSelect   = shuffleTable[i] - cuddRand2;
    shuffleTable[i] = cuddRand;
    shuffleSelect  += (shuffleSelect < 1) * (MODULUS1 - 1);
    /* Since shuffleSelect != 0, and we want to be able to return 0,
    ** here we subtract 1 before returning.
    */
    return(shuffleSelect - 1);

} /* end of Cudd_Random */


/**Function********************************************************************

  Synopsis    [Initializer for the portable random number generator.]

  Description [Initializer for the portable number generator based on
  ran2 in "Numerical Recipes in C." The input is the seed for the
  generator. If it is negative, its absolute value is taken as seed.
  If it is 0, then 1 is taken as seed. The initialized sets up the two
  recurrences used to generate a long-period stream, and sets up the
  shuffle table.]

  SideEffects [None]

  SeeAlso     [Cudd_Random]

******************************************************************************/
void
Cudd_Srandom(
  long  seed)
{
    int i;

    if (seed < 0)       cuddRand = -seed;
    else if (seed == 0) cuddRand = 1;
    else                cuddRand = seed;
    cuddRand2 = cuddRand;
    /* Load the shuffle table (after 11 warm-ups). */
    for (i = 0; i < STAB_SIZE + 11; i++) {
	long int w;
	w = cuddRand / LEQQ1;
	cuddRand = LEQA1 * (cuddRand - w * LEQQ1) - w * LEQR1;
	cuddRand += (cuddRand < 0) * MODULUS1;
	shuffleTable[i % STAB_SIZE] = cuddRand;
    }
    shuffleSelect = shuffleTable[1 % STAB_SIZE];

} /* end of Cudd_Srandom */


/**Function********************************************************************

  Synopsis    [Computes the density of a BDD or ADD.]

  Description [Computes the density of a BDD or ADD. The density is
  the ratio of the number of minterms to the number of nodes. If 0 is
  passed as number of variables, the number of variables existing in
  the manager is used. Returns the density if successful; (double)
  CUDD_OUT_OF_MEM otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_CountMinterm Cudd_DagSize]

******************************************************************************/
double
Cudd_Density(
  DdManager * dd /* manager */,
  DdNode * f /* function whose density is sought */,
  int  nvars /* size of the support of f */)
{
    double minterms;
    int nodes;
    double density;

    if (nvars == 0) nvars = dd->size;
    minterms = Cudd_CountMinterm(dd,f,nvars);
    if (minterms == (double) CUDD_OUT_OF_MEM) return(minterms);
    nodes = Cudd_DagSize(f);
    density = minterms / (double) nodes;
    return(density);

} /* end of Cudd_Density */


/**Function********************************************************************

  Synopsis    [Warns that a memory allocation failed.]

  Description [Warns that a memory allocation failed.
  This function can be used as replacement of MMout_of_memory to prevent
  the safe_mem functions of the util package from exiting when malloc
  returns NULL. One possible use is in case of discretionary allocations;
  for instance, the allocation of memory to enlarge the computed table.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
Cudd_OutOfMem(
  long size /* size of the allocation that failed */)
{
    (void) fflush(stdout);
    (void) fprintf(stderr, "\nunable to allocate %ld bytes\n", size);
    return;

} /* end of Cudd_OutOfMem */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Prints a DD to the standard output. One line per node is
  printed.]

  Description [Prints a DD to the standard output. One line per node is
  printed. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintDebug]

******************************************************************************/
int
cuddP(
  DdManager * dd,
  DdNode * f)
{
    int retval;
    st_table *table = st_init_table(st_ptrcmp,st_ptrhash);

    if (table == NULL) return(0);

    retval = dp2(dd,f,table);
    st_free_table(table);
    (void) fputc('\n',dd->out);
    return(retval);

} /* end of cuddP */


/**Function********************************************************************

  Synopsis [Frees the memory used to store the minterm counts recorded
  in the visited table.]

  Description [Frees the memory used to store the minterm counts
  recorded in the visited table. Returns ST_CONTINUE.]

  SideEffects [None]

******************************************************************************/
enum st_retval
cuddStCountfree(
  char * key,
  char * value,
  char * arg)
{
    double	*d;

    d = (double *)value;
    FREE(d);
    return(ST_CONTINUE);

} /* end of cuddStCountfree */


/**Function********************************************************************

  Synopsis    [Recursively collects all the nodes of a DD in a symbol
  table.]

  Description [Traverses the DD f and collects all its nodes in a
  symbol table.  f is assumed to be a regular pointer and
  cuddCollectNodes guarantees this assumption in the recursive calls.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddCollectNodes(
  DdNode * f,
  st_table * visited)
{
    DdNode	*T, *E;
    int		retval;

#ifdef DD_DEBUG
    assert(!Cudd_IsComplement(f));
#endif

    /* If already visited, nothing to do. */
    if (st_is_member(visited, (char *) f) == 1)
	return(1);

    /* Check for abnormal condition that should never happen. */
    if (f == NULL)
	return(0);

    /* Mark node as visited. */
    if (st_add_direct(visited, (char *) f, NULL) == ST_OUT_OF_MEM)
	return(0);

    /* Check terminal case. */
    if (cuddIsConstant(f))
	return(1);

    /* Recursive calls. */
    T = cuddT(f);
    retval = cuddCollectNodes(T,visited);
    if (retval != 1) return(retval);
    E = Cudd_Regular(cuddE(f));
    retval = cuddCollectNodes(E,visited);
    return(retval);

} /* end of cuddCollectNodes */


/**Function********************************************************************

  Synopsis    [Recursively collects all the nodes of a DD in an array.]

  Description [Traverses the DD f and collects all its nodes in an array.
  The caller should free the array returned by cuddNodeArray.
  Returns a pointer to the array of nodes in case of success; NULL
  otherwise.  The nodes are collected in reverse topological order, so
  that a node is always preceded in the array by all its descendants.]

  SideEffects [The number of nodes is returned as a side effect.]

  SeeAlso     [Cudd_FirstNode]

******************************************************************************/
DdNodePtr *
cuddNodeArray(
  DdNode *f,
  int *n)
{
    DdNodePtr *table;
    int size, retval;

    size = ddDagInt(Cudd_Regular(f));
    table = ALLOC(DdNodePtr, size);
    if (table == NULL) {
	ddClearFlag(Cudd_Regular(f));
	return(NULL);
    }

    retval = cuddNodeArrayRecur(f, table, 0);
    assert(retval == size);

    *n = size;
    return(table);

} /* cuddNodeArray */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of cuddP.]

  Description [Performs the recursive step of cuddP. Returns 1 in case
  of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
dp2(
  DdManager *dd,
  DdNode * f,
  st_table * t)
{
    DdNode *g, *n, *N;
    int T,E;

    if (f == NULL) {
	return(0);
    }
    g = Cudd_Regular(f);
    if (cuddIsConstant(g)) {
#if SIZEOF_VOID_P == 8
	(void) fprintf(dd->out,"ID = %c0x%lx\tvalue = %-9g\n", bang(f),
		(ptruint) g / (ptruint) sizeof(DdNode),cuddV(g));
#else
	(void) fprintf(dd->out,"ID = %c0x%x\tvalue = %-9g\n", bang(f),
		(ptruint) g / (ptruint) sizeof(DdNode),cuddV(g));
#endif
	return(1);
    }
    if (st_is_member(t,(char *) g) == 1) {
	return(1);
    }
    if (st_add_direct(t,(char *) g,NULL) == ST_OUT_OF_MEM)
	return(0);
#ifdef DD_STATS
#if SIZEOF_VOID_P == 8
    (void) fprintf(dd->out,"ID = %c0x%lx\tindex = %d\tr = %d\t", bang(f),
		(ptruint) g / (ptruint) sizeof(DdNode), g->index, g->ref);
#else
    (void) fprintf(dd->out,"ID = %c0x%x\tindex = %d\tr = %d\t", bang(f),
		(ptruint) g / (ptruint) sizeof(DdNode),g->index,g->ref);
#endif
#else
#if SIZEOF_VOID_P == 8
    (void) fprintf(dd->out,"ID = %c0x%lx\tindex = %u\t", bang(f),
		(ptruint) g / (ptruint) sizeof(DdNode),g->index);
#else
    (void) fprintf(dd->out,"ID = %c0x%x\tindex = %hu\t", bang(f),
		(ptruint) g / (ptruint) sizeof(DdNode),g->index);
#endif
#endif
    n = cuddT(g);
    if (cuddIsConstant(n)) {
	(void) fprintf(dd->out,"T = %-9g\t",cuddV(n));
	T = 1;
    } else {
#if SIZEOF_VOID_P == 8
	(void) fprintf(dd->out,"T = 0x%lx\t",(ptruint) n / (ptruint) sizeof(DdNode));
#else
	(void) fprintf(dd->out,"T = 0x%x\t",(ptruint) n / (ptruint) sizeof(DdNode));
#endif
	T = 0;
    }

    n = cuddE(g);
    N = Cudd_Regular(n);
    if (cuddIsConstant(N)) {
	(void) fprintf(dd->out,"E = %c%-9g\n",bang(n),cuddV(N));
	E = 1;
    } else {
#if SIZEOF_VOID_P == 8
	(void) fprintf(dd->out,"E = %c0x%lx\n", bang(n), (ptruint) N/(ptruint) sizeof(DdNode));
#else
	(void) fprintf(dd->out,"E = %c0x%x\n", bang(n), (ptruint) N/(ptruint) sizeof(DdNode));
#endif
	E = 0;
    }
    if (E == 0) {
	if (dp2(dd,N,t) == 0)
	    return(0);
    }
    if (T == 0) {
	if (dp2(dd,cuddT(g),t) == 0)
	    return(0);
    }
    return(1);

} /* end of dp2 */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_PrintMinterm.]

  Description []

  SideEffects [None]

******************************************************************************/
static void
ddPrintMintermAux(
  DdManager * dd /* manager */,
  DdNode * node /* current node */,
  int * list /* current recursion path */)
{
    DdNode	*N,*Nv,*Nnv;
    int		i,v,index;

    N = Cudd_Regular(node);

    if (cuddIsConstant(N)) {
	/* Terminal case: Print one cube based on the current recursion
	** path, unless we have reached the background value (ADDs) or
	** the logical zero (BDDs).
	*/
	if (node != background && node != zero) {
	    for (i = 0; i < dd->size; i++) {
		v = list[i];
		if (v == 0) (void) fprintf(dd->out,"0");
		else if (v == 1) (void) fprintf(dd->out,"1");
		else (void) fprintf(dd->out,"-");
	    }
	    (void) fprintf(dd->out," % g\n", cuddV(node));
	}
    } else {
	Nv  = cuddT(N);
	Nnv = cuddE(N);
	if (Cudd_IsComplement(node)) {
	    Nv  = Cudd_Not(Nv);
	    Nnv = Cudd_Not(Nnv);
	}
	index = N->index;
	list[index] = 0;
	ddPrintMintermAux(dd,Nnv,list);
	list[index] = 1;
	ddPrintMintermAux(dd,Nv,list);
	list[index] = 2;
    }
    return;

} /* end of ddPrintMintermAux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_DagSize.]

  Description [Performs the recursive step of Cudd_DagSize. Returns the
  number of nodes in the graph rooted at n.]

  SideEffects [None]

******************************************************************************/
static int
ddDagInt(
  DdNode * n)
{
    int tval, eval;

    if (Cudd_IsComplement(n->next)) {
	return(0);
    }
    n->next = Cudd_Not(n->next);
    if (cuddIsConstant(n)) {
	return(1);
    }
    tval = ddDagInt(cuddT(n));
    eval = ddDagInt(Cudd_Regular(cuddE(n)));
    return(1 + tval + eval);

} /* end of ddDagInt */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of cuddNodeArray.]

  Description [Performs the recursive step of cuddNodeArray.  Returns
  an the number of nodes in the DD.  Clear the least significant bit
  of the next field that was used as visited flag by
  cuddNodeArrayRecur when counting the nodes.  node is supposed to be
  regular; the invariant is maintained by this procedure.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddNodeArrayRecur(
  DdNode *f,
  DdNodePtr *table,
  int index)
{
    int tindex, eindex;

    if (!Cudd_IsComplement(f->next)) {
	return(index);
    }
    /* Clear visited flag. */
    f->next = Cudd_Regular(f->next);
    if (cuddIsConstant(f)) {
	table[index] = f;
	return(index + 1);
    }
    tindex = cuddNodeArrayRecur(cuddT(f), table, index);
    eindex = cuddNodeArrayRecur(Cudd_Regular(cuddE(f)), table, tindex);
    table[eindex] = f;
    return(eindex + 1);

} /* end of cuddNodeArrayRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CofactorEstimate.]

  Description [Performs the recursive step of Cudd_CofactorEstimate.
  Returns an estimate of the number of nodes in the DD of a
  cofactor of node. Uses the least significant bit of the next field as
  visited flag. node is supposed to be regular; the invariant is maintained
  by this procedure.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddEstimateCofactor(
  DdManager *dd,
  st_table *table,
  DdNode * node,
  int i,
  int phase,
  DdNode ** ptr)
{
    int tval, eval, val;
    DdNode *ptrT, *ptrE;

    if (Cudd_IsComplement(node->next)) {
	if (!st_lookup(table,(char *)node,(char **)ptr)) {
	    if (st_add_direct(table,(char *)node,(char *)node) ==
		ST_OUT_OF_MEM)
		return(CUDD_OUT_OF_MEM);
	    *ptr = node;
	}
	return(0);
    }
    node->next = Cudd_Not(node->next);
    if (cuddIsConstant(node)) {
	*ptr = node;
	if (st_add_direct(table,(char *)node,(char *)node) == ST_OUT_OF_MEM)
	    return(CUDD_OUT_OF_MEM);
	return(1);
    }
    if ((int) node->index == i) {
	if (phase == 1) {
	    *ptr = cuddT(node);
	    val = ddDagInt(cuddT(node));
	} else {
	    *ptr = cuddE(node);
	    val = ddDagInt(Cudd_Regular(cuddE(node)));
	}
	if (node->ref > 1) {
	    if (st_add_direct(table,(char *)node,(char *)*ptr) ==
		ST_OUT_OF_MEM)
		return(CUDD_OUT_OF_MEM);
	}
	return(val);
    }
    if (dd->perm[node->index] > dd->perm[i]) {
	*ptr = node;
	tval = ddDagInt(cuddT(node));
	eval = ddDagInt(Cudd_Regular(cuddE(node)));
	if (node->ref > 1) {
	    if (st_add_direct(table,(char *)node,(char *)node) ==
		ST_OUT_OF_MEM)
		return(CUDD_OUT_OF_MEM);
	}
	val = 1 + tval + eval;
	return(val);
    }
    tval = cuddEstimateCofactor(dd,table,cuddT(node),i,phase,&ptrT);
    eval = cuddEstimateCofactor(dd,table,Cudd_Regular(cuddE(node)),i,
				phase,&ptrE);
    ptrE = Cudd_NotCond(ptrE,Cudd_IsComplement(cuddE(node)));
    if (ptrT == ptrE) {		/* recombination */
	*ptr = ptrT;
	val = tval;
	if (node->ref > 1) {
	    if (st_add_direct(table,(char *)node,(char *)*ptr) ==
		    ST_OUT_OF_MEM)
		return(CUDD_OUT_OF_MEM);
	}
    } else if ((ptrT != cuddT(node) || ptrE != cuddE(node)) &&
	       (*ptr = cuddUniqueLookup(dd,node->index,ptrT,ptrE)) != NULL) {
	if (Cudd_IsComplement((*ptr)->next)) {
	    val = 0;
	} else {
	    val = 1 + tval + eval;
	}
	if (node->ref > 1) {
	    if (st_add_direct(table,(char *)node,(char *)*ptr) ==
		    ST_OUT_OF_MEM)
		return(CUDD_OUT_OF_MEM);
	}
    } else {
	*ptr = node;
	val = 1 + tval + eval;
    }
    return(val);

} /* end of cuddEstimateCofactor */


/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of an internal node.]

  Description [Checks the unique table for the existence of an internal
  node. Returns a pointer to the node if it is in the table; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInter]

******************************************************************************/
static DdNode *
cuddUniqueLookup(
  DdManager * unique,
  int  index,
  DdNode * T,
  DdNode * E)
{
    int posn;
    unsigned int level;
    DdNodePtr *nodelist;
    DdNode *looking;
    DdSubtable *subtable;

    if (index >= unique->size) {
	return(NULL);
    }

    level = unique->perm[index];
    subtable = &(unique->subtables[level]);

#ifdef DD_DEBUG
    assert(level < (unsigned) cuddI(unique,T->index));
    assert(level < (unsigned) cuddI(unique,Cudd_Regular(E)->index));
#endif

    posn = ddHash(T, E, subtable->shift);
    nodelist = subtable->nodelist;
    looking = nodelist[posn];

    while (T < cuddT(looking)) {
	looking = Cudd_Regular(looking->next);
    }
    while (T == cuddT(looking) && E < cuddE(looking)) {
	looking = Cudd_Regular(looking->next);
    }
    if (cuddT(looking) == T && cuddE(looking) == E) {
	return(looking);
    }

    return(NULL);

} /* end of cuddUniqueLookup */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CofactorEstimateSimple.]

  Description [Performs the recursive step of Cudd_CofactorEstimateSimple.
  Returns an estimate of the number of nodes in the DD of the positive
  cofactor of node. Uses the least significant bit of the next field as
  visited flag. node is supposed to be regular; the invariant is maintained
  by this procedure.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddEstimateCofactorSimple(
  DdNode * node,
  int i)
{
    int tval, eval;

    if (Cudd_IsComplement(node->next)) {
	return(0);
    }
    node->next = Cudd_Not(node->next);
    if (cuddIsConstant(node)) {
	return(1);
    }
    tval = cuddEstimateCofactorSimple(cuddT(node),i);
    if ((int) node->index == i) return(tval);
    eval = cuddEstimateCofactorSimple(Cudd_Regular(cuddE(node)),i);
    return(1 + tval + eval);

} /* end of cuddEstimateCofactorSimple */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountMinterm.]

  Description [Performs the recursive step of Cudd_CountMinterm.
  It is based on the following identity. Let |f| be the
  number of minterms of f. Then:
  <xmp>
    |f| = (|f0|+|f1|)/2
  </xmp>
  where f0 and f1 are the two cofactors of f.  Does not use the
  identity |f'| = max - |f|, to minimize loss of accuracy due to
  roundoff.  Returns the number of minterms of the function rooted at
  node.]

  SideEffects [None]

******************************************************************************/
static double
ddCountMintermAux(
  DdNode * node,
  double  max,
  DdHashTable * table)
{
    DdNode	*N, *Nt, *Ne;
    double	min, minT, minE;
    DdNode	*res;

    N = Cudd_Regular(node);

    if (cuddIsConstant(N)) {
	if (node == background || node == zero) {
	    return(0.0);
	} else {
	    return(max);
	}
    }
    if (N->ref != 1 && (res = cuddHashTableLookup1(table,node)) != NULL) {
	min = cuddV(res);
	if (res->ref == 0) {
	    table->manager->dead++;
	    table->manager->constants.dead++;
	}
	return(min);
    }

    Nt = cuddT(N); Ne = cuddE(N);
    if (Cudd_IsComplement(node)) {
	Nt = Cudd_Not(Nt); Ne = Cudd_Not(Ne);
    }

    minT = ddCountMintermAux(Nt,max,table);
    if (minT == (double)CUDD_OUT_OF_MEM) return((double)CUDD_OUT_OF_MEM);
    minT *= 0.5;
    minE = ddCountMintermAux(Ne,max,table);
    if (minE == (double)CUDD_OUT_OF_MEM) return((double)CUDD_OUT_OF_MEM);
    minE *= 0.5;
    min = minT + minE;

    if (N->ref != 1) {
	ptrint fanout = (ptrint) N->ref;
	cuddSatDec(fanout);
	res = cuddUniqueConst(table->manager,min);
	if (!cuddHashTableInsert1(table,node,res,fanout)) {
	    cuddRef(res); Cudd_RecursiveDeref(table->manager, res);
	    return((double)CUDD_OUT_OF_MEM);
	}
    }

    return(min);

} /* end of ddCountMintermAux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountPath.]

  Description [Performs the recursive step of Cudd_CountPath.
  It is based on the following identity. Let |f| be the
  number of paths of f. Then:
  <xmp>
    |f| = |f0|+|f1|
  </xmp>
  where f0 and f1 are the two cofactors of f.  Uses the
  identity |f'| = |f|, to improve the utilization of the (local) cache.
  Returns the number of paths of the function rooted at node.]

  SideEffects [None]

******************************************************************************/
static double
ddCountPathAux(
  DdNode * node,
  st_table * table)
{

    DdNode	*Nv, *Nnv;
    double	paths, *ppaths, paths1, paths2;
    double	*dummy;


    if (cuddIsConstant(node)) {
	return(1.0);
    }
    if (st_lookup(table, node, &dummy)) {
	paths = *dummy;
	return(paths);
    }

    Nv = cuddT(node); Nnv = cuddE(node);

    paths1 = ddCountPathAux(Nv,table);
    if (paths1 == (double)CUDD_OUT_OF_MEM) return((double)CUDD_OUT_OF_MEM);
    paths2 = ddCountPathAux(Cudd_Regular(Nnv),table);
    if (paths2 == (double)CUDD_OUT_OF_MEM) return((double)CUDD_OUT_OF_MEM);
    paths = paths1 + paths2;

    ppaths = ALLOC(double,1);
    if (ppaths == NULL) {
	return((double)CUDD_OUT_OF_MEM);
    }

    *ppaths = paths;

    if (st_add_direct(table,(char *)node, (char *)ppaths) == ST_OUT_OF_MEM) {
	FREE(ppaths);
	return((double)CUDD_OUT_OF_MEM);
    }
    return(paths);

} /* end of ddCountPathAux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_EpdCountMinterm.]

  Description [Performs the recursive step of Cudd_EpdCountMinterm.
  It is based on the following identity. Let |f| be the
  number of minterms of f. Then:
  <xmp>
    |f| = (|f0|+|f1|)/2
  </xmp>
  where f0 and f1 are the two cofactors of f.  Does not use the
  identity |f'| = max - |f|, to minimize loss of accuracy due to
  roundoff.  Returns the number of minterms of the function rooted at
  node.]

  SideEffects [None]

******************************************************************************/
static int
ddEpdCountMintermAux(
  DdNode * node,
  EpDouble * max,
  EpDouble * epd,
  st_table * table)
{
    DdNode	*Nt, *Ne;
    EpDouble	*min, minT, minE;
    EpDouble	*res;
    int		status;

    /* node is assumed to be regular */
    if (cuddIsConstant(node)) {
	if (node == background || node == zero) {
	    EpdMakeZero(epd, 0);
	} else {
	    EpdCopy(max, epd);
	}
	return(0);
    }
    if (node->ref != 1 && st_lookup(table, node, &res)) {
	EpdCopy(res, epd);
	return(0);
    }

    Nt = cuddT(node); Ne = cuddE(node);

    status = ddEpdCountMintermAux(Nt,max,&minT,table);
    if (status == CUDD_OUT_OF_MEM) return(CUDD_OUT_OF_MEM);
    EpdMultiply(&minT, (double)0.5);
    status = ddEpdCountMintermAux(Cudd_Regular(Ne),max,&minE,table);
    if (status == CUDD_OUT_OF_MEM) return(CUDD_OUT_OF_MEM);
    if (Cudd_IsComplement(Ne)) {
	EpdSubtract3(max, &minE, epd);
	EpdCopy(epd, &minE);
    }
    EpdMultiply(&minE, (double)0.5);
    EpdAdd3(&minT, &minE, epd);

    if (node->ref > 1) {
	min = EpdAlloc();
	if (!min)
	    return(CUDD_OUT_OF_MEM);
	EpdCopy(epd, min);
	if (st_insert(table, (char *)node, (char *)min) == ST_OUT_OF_MEM) {
	    EpdFree(min);
	    return(CUDD_OUT_OF_MEM);
	}
    }

    return(0);

} /* end of ddEpdCountMintermAux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountPathsToNonZero.]

  Description [Performs the recursive step of Cudd_CountPathsToNonZero.
  It is based on the following identity. Let |f| be the
  number of paths of f. Then:
  <xmp>
    |f| = |f0|+|f1|
  </xmp>
  where f0 and f1 are the two cofactors of f.  Returns the number of
  paths of the function rooted at node.]

  SideEffects [None]

******************************************************************************/
static double
ddCountPathsToNonZero(
  DdNode * N,
  st_table * table)
{

    DdNode	*node, *Nt, *Ne;
    double	paths, *ppaths, paths1, paths2;
    double	*dummy;

    node = Cudd_Regular(N);
    if (cuddIsConstant(node)) {
	return((double) !(Cudd_IsComplement(N) || cuddV(node)==DD_ZERO_VAL));
    }
    if (st_lookup(table, N, &dummy)) {
	paths = *dummy;
	return(paths);
    }

    Nt = cuddT(node); Ne = cuddE(node);
    if (node != N) {
	Nt = Cudd_Not(Nt); Ne = Cudd_Not(Ne);
    }

    paths1 = ddCountPathsToNonZero(Nt,table);
    if (paths1 == (double)CUDD_OUT_OF_MEM) return((double)CUDD_OUT_OF_MEM);
    paths2 = ddCountPathsToNonZero(Ne,table);
    if (paths2 == (double)CUDD_OUT_OF_MEM) return((double)CUDD_OUT_OF_MEM);
    paths = paths1 + paths2;

    ppaths = ALLOC(double,1);
    if (ppaths == NULL) {
	return((double)CUDD_OUT_OF_MEM);
    }

    *ppaths = paths;

    if (st_add_direct(table,(char *)N, (char *)ppaths) == ST_OUT_OF_MEM) {
	FREE(ppaths);
	return((double)CUDD_OUT_OF_MEM);
    }
    return(paths);

} /* end of ddCountPathsToNonZero */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_Support.]

  Description [Performs the recursive step of Cudd_Support. Performs a
  DFS from f. The support is accumulated in supp as a side effect. Uses
  the LSB of the then pointer as visited flag.]

  SideEffects [None]

  SeeAlso     [ddClearFlag]

******************************************************************************/
static void
ddSupportStep(
  DdNode * f,
  int * support)
{
    if (cuddIsConstant(f) || Cudd_IsComplement(f->next))
	return;

    support[f->index] = 1;
    ddSupportStep(cuddT(f),support);
    ddSupportStep(Cudd_Regular(cuddE(f)),support);
    /* Mark as visited. */
    f->next = Cudd_Complement(f->next);

} /* end of ddSupportStep */


/**Function********************************************************************

  Synopsis    [Performs a DFS from f, clearing the LSB of the next
  pointers.]

  Description []

  SideEffects [None]

  SeeAlso     [ddSupportStep ddFindSupport ddLeavesInt ddDagInt]

******************************************************************************/
static void
ddClearFlag(
  DdNode * f)
{
    if (!Cudd_IsComplement(f->next)) {
	return;
    }
    /* Clear visited flag. */
    f->next = Cudd_Regular(f->next);
    if (cuddIsConstant(f)) {
	return;
    }
    ddClearFlag(cuddT(f));
    ddClearFlag(Cudd_Regular(cuddE(f)));
    return;

} /* end of ddClearFlag */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountLeaves.]

  Description [Performs the recursive step of Cudd_CountLeaves. Returns
  the number of leaves in the DD rooted at n.]

  SideEffects [None]

  SeeAlso     [Cudd_CountLeaves]

******************************************************************************/
static int
ddLeavesInt(
  DdNode * n)
{
    int tval, eval;

    if (Cudd_IsComplement(n->next)) {
	return(0);
    }
    n->next = Cudd_Not(n->next);
    if (cuddIsConstant(n)) {
	return(1);
    }
    tval = ddLeavesInt(cuddT(n));
    eval = ddLeavesInt(Cudd_Regular(cuddE(n)));
    return(tval + eval);

} /* end of ddLeavesInt */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddPickArbitraryMinterms.]

  Description [Performs the recursive step of Cudd_bddPickArbitraryMinterms.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [none]

  SeeAlso [Cudd_bddPickArbitraryMinterms]

******************************************************************************/
static int
ddPickArbitraryMinterms(
  DdManager *dd,
  DdNode *node,
  int nvars,
  int nminterms,
  char **string)
{
    DdNode *N, *T, *E;
    DdNode *one, *bzero;
    int    i, t, result;
    double min1, min2;

    if (string == NULL || node == NULL) return(0);

    /* The constant 0 function has no on-set cubes. */
    one = DD_ONE(dd);
    bzero = Cudd_Not(one);
    if (nminterms == 0 || node == bzero) return(1);
    if (node == one) {
	return(1);
    }

    N = Cudd_Regular(node);
    T = cuddT(N); E = cuddE(N);
    if (Cudd_IsComplement(node)) {
	T = Cudd_Not(T); E = Cudd_Not(E);
    }

    min1 = Cudd_CountMinterm(dd, T, nvars) / 2.0;
    if (min1 == (double)CUDD_OUT_OF_MEM) return(0);
    min2 = Cudd_CountMinterm(dd, E, nvars) / 2.0;
    if (min2 == (double)CUDD_OUT_OF_MEM) return(0);

    t = (int)((double)nminterms * min1 / (min1 + min2) + 0.5);
    for (i = 0; i < t; i++)
	string[i][N->index] = '1';
    for (i = t; i < nminterms; i++)
	string[i][N->index] = '0';

    result = ddPickArbitraryMinterms(dd,T,nvars,t,&string[0]);
    if (result == 0)
	return(0);
    result = ddPickArbitraryMinterms(dd,E,nvars,nminterms-t,&string[t]);
    return(result);

} /* end of ddPickArbitraryMinterms */


/**Function********************************************************************

  Synopsis    [Finds a representative cube of a BDD.]

  Description [Finds a representative cube of a BDD with the weight of
  each variable. From the top variable, if the weight is greater than or
  equal to 0.0, choose THEN branch unless the child is the constant 0.
  Otherwise, choose ELSE branch unless the child is the constant 0.]

  SideEffects [Cudd_SubsetWithMaskVars Cudd_bddPickOneCube]

******************************************************************************/
static int
ddPickRepresentativeCube(
  DdManager *dd,
  DdNode *node,
  double *weight,
  char *string)
{
    DdNode *N, *T, *E;
    DdNode *one, *bzero;

    if (string == NULL || node == NULL) return(0);

    /* The constant 0 function has no on-set cubes. */
    one = DD_ONE(dd);
    bzero = Cudd_Not(one);
    if (node == bzero) return(0);

    if (node == DD_ONE(dd)) return(1);

    for (;;) {
	N = Cudd_Regular(node);
	if (N == one)
	    break;
	T = cuddT(N);
	E = cuddE(N);
	if (Cudd_IsComplement(node)) {
	    T = Cudd_Not(T);
	    E = Cudd_Not(E);
	}
	if (weight[N->index] >= 0.0) {
	    if (T == bzero) {
		node = E;
		string[N->index] = '0';
	    } else {
		node = T;
		string[N->index] = '1';
	    }
	} else {
	    if (E == bzero) {
		node = T;
		string[N->index] = '1';
	    } else {
		node = E;
		string[N->index] = '0';
	    }
	}
    }
    return(1);

} /* end of ddPickRepresentativeCube */


/**Function********************************************************************

  Synopsis [Frees the memory used to store the minterm counts recorded
  in the visited table.]

  Description [Frees the memory used to store the minterm counts
  recorded in the visited table. Returns ST_CONTINUE.]

  SideEffects [None]

******************************************************************************/
static enum st_retval
ddEpdFree(
  char * key,
  char * value,
  char * arg)
{
    EpDouble	*epd;

    epd = (EpDouble *) value;
    EpdFree(epd);
    return(ST_CONTINUE);

} /* end of ddEpdFree */


/**Function********************************************************************

  Synopsis [Recursively find the support of f.]

  Description [Recursively find the support of f.  This function uses the
  LSB of the next field of the nodes of f as visited flag.  It also uses the
  LSB of the next field of the variables as flag to remember whether a
  certain index has already been seen.  Finally, it uses the manager stack
  to record all seen indices.]

  SideEffects [The stack pointer SP is modified by side-effect.  The next
  fields are changed and need to be reset.]

******************************************************************************/
static void
ddFindSupport(
  DdManager *dd,
  DdNode *f,
  int *SP)
{
    int index;
    DdNode *var;

    if (cuddIsConstant(f) || Cudd_IsComplement(f->next)) {
	return;
    }

    index = f->index;
    var = dd->vars[index];
    /* It is possible that var is embedded in f.  That causes no problem,
    ** though, because if we see it after encountering another node with
    ** the same index, nothing is supposed to happen.
    */
    if (!Cudd_IsComplement(var->next)) {
        var->next = Cudd_Complement(var->next);
        dd->stack[*SP] = (DdNode *)(ptrint) index;
        (*SP)++;
    }
    ddFindSupport(dd, cuddT(f), SP);
    ddFindSupport(dd, Cudd_Regular(cuddE(f)), SP);
    /* Mark as visited. */
    f->next = Cudd_Complement(f->next);

} /* end of ddFindSupport */


/**Function********************************************************************

  Synopsis [Clears visited flags for variables.]

  Description [Clears visited flags for variables.]

  SideEffects [None]

******************************************************************************/
static void
ddClearVars(
  DdManager *dd,
  int SP)
{
    int i;

    for (i = 0; i < SP; i++) {
        int index = (int) (ptrint) dd->stack[i];
        DdNode *var = dd->vars[index];
        var->next = Cudd_Regular(var->next);
    }
    
} /* end of ddClearVars */


/**Function********************************************************************

  Synopsis [Compares indices for qsort.]

  Description [Compares indices for qsort.  Subtracting these integers
  cannot produce overflow, because they are non-negative.]

  SideEffects [None]

******************************************************************************/
static int
indexCompare(
  const void *a,
  const void *b)
{
    int ia = *((int *) a);
    int ib = *((int *) b);
    return(ia - ib);

} /* end of indexCompare */
