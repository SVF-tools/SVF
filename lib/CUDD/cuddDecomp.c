/**CFile***********************************************************************

  FileName    [cuddDecomp.c]

  PackageName [cudd]

  Synopsis    [Functions for BDD decomposition.]

  Description [External procedures included in this file:
		<ul>
		<li> Cudd_bddApproxConjDecomp()
		<li> Cudd_bddApproxDisjDecomp()
		<li> Cudd_bddIterConjDecomp()
		<li> Cudd_bddIterDisjDecomp()
		<li> Cudd_bddGenConjDecomp()
		<li> Cudd_bddGenDisjDecomp()
		<li> Cudd_bddVarConjDecomp()
		<li> Cudd_bddVarDisjDecomp()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> cuddConjunctsAux()
		<li> CreateBotDist()
		<li> BuildConjuncts()
		<li> ConjunctsFree()
		</ul>]

  Author      [Kavita Ravi, Fabio Somenzi]

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
#define DEPTH 5
#define THRESHOLD 10
#define NONE 0
#define PAIR_ST 1
#define PAIR_CR 2
#define G_ST 3
#define G_CR 4
#define H_ST 5
#define H_CR 6
#define BOTH_G 7
#define BOTH_H 8

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/
typedef struct Conjuncts {
    DdNode *g;
    DdNode *h;
} Conjuncts;

typedef struct  NodeStat {
    int distance;
    int localRef;
} NodeStat;


/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddDecomp.c,v 1.45 2012/02/05 01:07:18 fabio Exp $";
#endif

static	DdNode	*one, *zero;
long lastTimeG;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


#define FactorsNotStored(factors)  ((int)((long)(factors) & 01))

#define FactorsComplement(factors) ((Conjuncts *)((long)(factors) | 01))

#define FactorsUncomplement(factors) ((Conjuncts *)((long)(factors) ^ 01))

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static NodeStat * CreateBotDist (DdNode * node, st_table * distanceTable);
static double CountMinterms (DdNode * node, double max, st_table * mintermTable, FILE *fp);
static void ConjunctsFree (DdManager * dd, Conjuncts * factors);
static int PairInTables (DdNode * g, DdNode * h, st_table * ghTable);
static Conjuncts * CheckTablesCacheAndReturn (DdNode * node, DdNode * g, DdNode * h, st_table * ghTable, st_table * cacheTable);
static Conjuncts * PickOnePair (DdNode * node, DdNode * g1, DdNode * h1, DdNode * g2, DdNode * h2, st_table * ghTable, st_table * cacheTable);
static Conjuncts * CheckInTables (DdNode * node, DdNode * g1, DdNode * h1, DdNode * g2, DdNode * h2, st_table * ghTable, st_table * cacheTable, int * outOfMem);
static Conjuncts * ZeroCase (DdManager * dd, DdNode * node, Conjuncts * factorsNv, st_table * ghTable, st_table * cacheTable, int switched);
static Conjuncts * BuildConjuncts (DdManager * dd, DdNode * node, st_table * distanceTable, st_table * cacheTable, int approxDistance, int maxLocalRef, st_table * ghTable, st_table * mintermTable);
static int cuddConjunctsAux (DdManager * dd, DdNode * f, DdNode ** c1, DdNode ** c2);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs two-way conjunctive decomposition of a BDD.]

  Description [Performs two-way conjunctive decomposition of a
  BDD. This procedure owes its name to the use of supersetting to
  obtain an initial factor of the given function. Returns the number
  of conjuncts produced, that is, 2 if successful; 1 if no meaningful
  decomposition was found; 0 otherwise. The conjuncts produced by this
  procedure tend to be imbalanced.]

  SideEffects [The factors are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the conjuncts are already
  referenced. If the function returns 0, the array for the conjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddApproxDisjDecomp Cudd_bddIterConjDecomp
  Cudd_bddGenConjDecomp Cudd_bddVarConjDecomp Cudd_RemapOverApprox
  Cudd_bddSqueeze Cudd_bddLICompaction]

******************************************************************************/
int
Cudd_bddApproxConjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** conjuncts /* address of the first factor */)
{
    DdNode *superset1, *superset2, *glocal, *hlocal;
    int nvars = Cudd_SupportSize(dd,f);

    /* Find a tentative first factor by overapproximation and minimization. */
    superset1 = Cudd_RemapOverApprox(dd,f,nvars,0,1.0);
    if (superset1 == NULL) return(0);
    cuddRef(superset1);
    superset2 = Cudd_bddSqueeze(dd,f,superset1);
    if (superset2 == NULL) {
	Cudd_RecursiveDeref(dd,superset1);
	return(0);
    }
    cuddRef(superset2);
    Cudd_RecursiveDeref(dd,superset1);

    /* Compute the second factor by minimization. */
    hlocal = Cudd_bddLICompaction(dd,f,superset2);
    if (hlocal == NULL) {
	Cudd_RecursiveDeref(dd,superset2);
	return(0);
    }
    cuddRef(hlocal);

    /* Refine the first factor by minimization. If h turns out to be f, this
    ** step guarantees that g will be 1. */
    glocal = Cudd_bddLICompaction(dd,superset2,hlocal);
    if (glocal == NULL) {
	Cudd_RecursiveDeref(dd,superset2);
	Cudd_RecursiveDeref(dd,hlocal);
	return(0);
    }
    cuddRef(glocal);
    Cudd_RecursiveDeref(dd,superset2);

    if (glocal != DD_ONE(dd)) {
	if (hlocal != DD_ONE(dd)) {
	    *conjuncts = ALLOC(DdNode *,2);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,glocal);
		Cudd_RecursiveDeref(dd,hlocal);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = glocal;
	    (*conjuncts)[1] = hlocal;
	    return(2);
	} else {
	    Cudd_RecursiveDeref(dd,hlocal);
	    *conjuncts = ALLOC(DdNode *,1);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,glocal);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = glocal;
	    return(1);
	}
    } else {
	Cudd_RecursiveDeref(dd,glocal);
	*conjuncts = ALLOC(DdNode *,1);
	if (*conjuncts == NULL) {
	    Cudd_RecursiveDeref(dd,hlocal);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	(*conjuncts)[0] = hlocal;
	return(1);
    }

} /* end of Cudd_bddApproxConjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way disjunctive decomposition of a BDD.]

  Description [Performs two-way disjunctive decomposition of a BDD.
  Returns the number of disjuncts produced, that is, 2 if successful;
  1 if no meaningful decomposition was found; 0 otherwise. The
  disjuncts produced by this procedure tend to be imbalanced.]

  SideEffects [The two disjuncts are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the disjuncts are already
  referenced. If the function returns 0, the array for the disjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddApproxConjDecomp Cudd_bddIterDisjDecomp
  Cudd_bddGenDisjDecomp Cudd_bddVarDisjDecomp]

******************************************************************************/
int
Cudd_bddApproxDisjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** disjuncts /* address of the array of the disjuncts */)
{
    int result, i;

    result = Cudd_bddApproxConjDecomp(dd,Cudd_Not(f),disjuncts);
    for (i = 0; i < result; i++) {
	(*disjuncts)[i] = Cudd_Not((*disjuncts)[i]);
    }
    return(result);

} /* end of Cudd_bddApproxDisjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way conjunctive decomposition of a BDD.]

  Description [Performs two-way conjunctive decomposition of a
  BDD. This procedure owes its name to the iterated use of
  supersetting to obtain a factor of the given function. Returns the
  number of conjuncts produced, that is, 2 if successful; 1 if no
  meaningful decomposition was found; 0 otherwise. The conjuncts
  produced by this procedure tend to be imbalanced.]

  SideEffects [The factors are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the conjuncts are already
  referenced. If the function returns 0, the array for the conjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddIterDisjDecomp Cudd_bddApproxConjDecomp
  Cudd_bddGenConjDecomp Cudd_bddVarConjDecomp Cudd_RemapOverApprox
  Cudd_bddSqueeze Cudd_bddLICompaction]

******************************************************************************/
int
Cudd_bddIterConjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** conjuncts /* address of the array of conjuncts */)
{
    DdNode *superset1, *superset2, *old[2], *res[2];
    int sizeOld, sizeNew;
    int nvars = Cudd_SupportSize(dd,f);

    old[0] = DD_ONE(dd);
    cuddRef(old[0]);
    old[1] = f;
    cuddRef(old[1]);
    sizeOld = Cudd_SharingSize(old,2);

    do {
	/* Find a tentative first factor by overapproximation and
	** minimization. */
	superset1 = Cudd_RemapOverApprox(dd,old[1],nvars,0,1.0);
	if (superset1 == NULL) {
	    Cudd_RecursiveDeref(dd,old[0]);
	    Cudd_RecursiveDeref(dd,old[1]);
	    return(0);
	}
	cuddRef(superset1);
	superset2 = Cudd_bddSqueeze(dd,old[1],superset1);
	if (superset2 == NULL) {
	    Cudd_RecursiveDeref(dd,old[0]);
	    Cudd_RecursiveDeref(dd,old[1]);
	    Cudd_RecursiveDeref(dd,superset1);
	    return(0);
	}
	cuddRef(superset2);
	Cudd_RecursiveDeref(dd,superset1);
	res[0] = Cudd_bddAnd(dd,old[0],superset2);
	if (res[0] == NULL) {
	    Cudd_RecursiveDeref(dd,superset2);
	    Cudd_RecursiveDeref(dd,old[0]);
	    Cudd_RecursiveDeref(dd,old[1]);
	    return(0);
	}
	cuddRef(res[0]);
	Cudd_RecursiveDeref(dd,superset2);
	if (res[0] == old[0]) {
	    Cudd_RecursiveDeref(dd,res[0]);
	    break;	/* avoid infinite loop */
	}

	/* Compute the second factor by minimization. */
	res[1] = Cudd_bddLICompaction(dd,old[1],res[0]);
	if (res[1] == NULL) {
	    Cudd_RecursiveDeref(dd,old[0]);
	    Cudd_RecursiveDeref(dd,old[1]);
	    return(0);
	}
	cuddRef(res[1]);

	sizeNew = Cudd_SharingSize(res,2);
	if (sizeNew <= sizeOld) {
	    Cudd_RecursiveDeref(dd,old[0]);
	    old[0] = res[0];
	    Cudd_RecursiveDeref(dd,old[1]);
	    old[1] = res[1];
	    sizeOld = sizeNew;
	} else {
	    Cudd_RecursiveDeref(dd,res[0]);
	    Cudd_RecursiveDeref(dd,res[1]);
	    break;
	}

    } while (1);

    /* Refine the first factor by minimization. If h turns out to
    ** be f, this step guarantees that g will be 1. */
    superset1 = Cudd_bddLICompaction(dd,old[0],old[1]);
    if (superset1 == NULL) {
	Cudd_RecursiveDeref(dd,old[0]);
	Cudd_RecursiveDeref(dd,old[1]);
	return(0);
    }
    cuddRef(superset1);
    Cudd_RecursiveDeref(dd,old[0]);
    old[0] = superset1;

    if (old[0] != DD_ONE(dd)) {
	if (old[1] != DD_ONE(dd)) {
	    *conjuncts = ALLOC(DdNode *,2);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,old[0]);
		Cudd_RecursiveDeref(dd,old[1]);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = old[0];
	    (*conjuncts)[1] = old[1];
	    return(2);
	} else {
	    Cudd_RecursiveDeref(dd,old[1]);
	    *conjuncts = ALLOC(DdNode *,1);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,old[0]);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = old[0];
	    return(1);
	}
    } else {
	Cudd_RecursiveDeref(dd,old[0]);
	*conjuncts = ALLOC(DdNode *,1);
	if (*conjuncts == NULL) {
	    Cudd_RecursiveDeref(dd,old[1]);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	(*conjuncts)[0] = old[1];
	return(1);
    }

} /* end of Cudd_bddIterConjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way disjunctive decomposition of a BDD.]

  Description [Performs two-way disjunctive decomposition of a BDD.
  Returns the number of disjuncts produced, that is, 2 if successful;
  1 if no meaningful decomposition was found; 0 otherwise. The
  disjuncts produced by this procedure tend to be imbalanced.]

  SideEffects [The two disjuncts are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the disjuncts are already
  referenced. If the function returns 0, the array for the disjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddIterConjDecomp Cudd_bddApproxDisjDecomp
  Cudd_bddGenDisjDecomp Cudd_bddVarDisjDecomp]

******************************************************************************/
int
Cudd_bddIterDisjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** disjuncts /* address of the array of the disjuncts */)
{
    int result, i;

    result = Cudd_bddIterConjDecomp(dd,Cudd_Not(f),disjuncts);
    for (i = 0; i < result; i++) {
	(*disjuncts)[i] = Cudd_Not((*disjuncts)[i]);
    }
    return(result);

} /* end of Cudd_bddIterDisjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way conjunctive decomposition of a BDD.]

  Description [Performs two-way conjunctive decomposition of a
  BDD. This procedure owes its name to the fact tht it generalizes the
  decomposition based on the cofactors with respect to one
  variable. Returns the number of conjuncts produced, that is, 2 if
  successful; 1 if no meaningful decomposition was found; 0
  otherwise. The conjuncts produced by this procedure tend to be
  balanced.]

  SideEffects [The two factors are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the conjuncts are already
  referenced. If the function returns 0, the array for the conjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddGenDisjDecomp Cudd_bddApproxConjDecomp
  Cudd_bddIterConjDecomp Cudd_bddVarConjDecomp]

******************************************************************************/
int
Cudd_bddGenConjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** conjuncts /* address of the array of conjuncts */)
{
    int result;
    DdNode *glocal, *hlocal;

    one = DD_ONE(dd);
    zero = Cudd_Not(one);
    
    do {
	dd->reordered = 0;
	result = cuddConjunctsAux(dd, f, &glocal, &hlocal);
    } while (dd->reordered == 1);

    if (result == 0) {
	return(0);
    }

    if (glocal != one) {
	if (hlocal != one) {
	    *conjuncts = ALLOC(DdNode *,2);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,glocal);
		Cudd_RecursiveDeref(dd,hlocal);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = glocal;
	    (*conjuncts)[1] = hlocal;
	    return(2);
	} else {
	    Cudd_RecursiveDeref(dd,hlocal);
	    *conjuncts = ALLOC(DdNode *,1);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,glocal);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = glocal;
	    return(1);
	}
    } else {
	Cudd_RecursiveDeref(dd,glocal);
	*conjuncts = ALLOC(DdNode *,1);
	if (*conjuncts == NULL) {
	    Cudd_RecursiveDeref(dd,hlocal);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	(*conjuncts)[0] = hlocal;
	return(1);
    }

} /* end of Cudd_bddGenConjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way disjunctive decomposition of a BDD.]

  Description [Performs two-way disjunctive decomposition of a BDD.
  Returns the number of disjuncts produced, that is, 2 if successful;
  1 if no meaningful decomposition was found; 0 otherwise. The
  disjuncts produced by this procedure tend to be balanced.]

  SideEffects [The two disjuncts are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the disjuncts are already
  referenced. If the function returns 0, the array for the disjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddGenConjDecomp Cudd_bddApproxDisjDecomp
  Cudd_bddIterDisjDecomp Cudd_bddVarDisjDecomp]

******************************************************************************/
int
Cudd_bddGenDisjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** disjuncts /* address of the array of the disjuncts */)
{
    int result, i;

    result = Cudd_bddGenConjDecomp(dd,Cudd_Not(f),disjuncts);
    for (i = 0; i < result; i++) {
	(*disjuncts)[i] = Cudd_Not((*disjuncts)[i]);
    }
    return(result);

} /* end of Cudd_bddGenDisjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way conjunctive decomposition of a BDD.]

  Description [Conjunctively decomposes one BDD according to a
  variable.  If <code>f</code> is the function of the BDD and
  <code>x</code> is the variable, the decomposition is
  <code>(f+x)(f+x')</code>.  The variable is chosen so as to balance
  the sizes of the two conjuncts and to keep them small.  Returns the
  number of conjuncts produced, that is, 2 if successful; 1 if no
  meaningful decomposition was found; 0 otherwise.]

  SideEffects [The two factors are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the conjuncts are already
  referenced. If the function returns 0, the array for the conjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddVarDisjDecomp Cudd_bddGenConjDecomp
  Cudd_bddApproxConjDecomp Cudd_bddIterConjDecomp]

*****************************************************************************/
int
Cudd_bddVarConjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** conjuncts /* address of the array of conjuncts */)
{
    int best;
    int min;
    DdNode *support, *scan, *var, *glocal, *hlocal;

    /* Find best cofactoring variable. */
    support = Cudd_Support(dd,f);
    if (support == NULL) return(0);
    if (Cudd_IsConstant(support)) {
	*conjuncts = ALLOC(DdNode *,1);
	if (*conjuncts == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	(*conjuncts)[0] = f;
	cuddRef((*conjuncts)[0]);
	return(1);
    }
    cuddRef(support);
    min = 1000000000;
    best = -1;
    scan = support;
    while (!Cudd_IsConstant(scan)) {
	int i = scan->index;
	int est1 = Cudd_EstimateCofactor(dd,f,i,1);
	int est0 = Cudd_EstimateCofactor(dd,f,i,0);
	/* Minimize the size of the larger of the two cofactors. */
	int est = (est1 > est0) ? est1 : est0;
	if (est < min) {
	    min = est;
	    best = i;
	}
	scan = cuddT(scan);
    }
#ifdef DD_DEBUG
    assert(best >= 0 && best < dd->size);
#endif
    Cudd_RecursiveDeref(dd,support);

    var = Cudd_bddIthVar(dd,best);
    glocal = Cudd_bddOr(dd,f,var);
    if (glocal == NULL) {
	return(0);
    }
    cuddRef(glocal);
    hlocal = Cudd_bddOr(dd,f,Cudd_Not(var));
    if (hlocal == NULL) {
	Cudd_RecursiveDeref(dd,glocal);
	return(0);
    }
    cuddRef(hlocal);

    if (glocal != DD_ONE(dd)) {
	if (hlocal != DD_ONE(dd)) {
	    *conjuncts = ALLOC(DdNode *,2);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,glocal);
		Cudd_RecursiveDeref(dd,hlocal);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = glocal;
	    (*conjuncts)[1] = hlocal;
	    return(2);
	} else {
	    Cudd_RecursiveDeref(dd,hlocal);
	    *conjuncts = ALLOC(DdNode *,1);
	    if (*conjuncts == NULL) {
		Cudd_RecursiveDeref(dd,glocal);
		dd->errorCode = CUDD_MEMORY_OUT;
		return(0);
	    }
	    (*conjuncts)[0] = glocal;
	    return(1);
	}
    } else {
	Cudd_RecursiveDeref(dd,glocal);
	*conjuncts = ALLOC(DdNode *,1);
	if (*conjuncts == NULL) {
	    Cudd_RecursiveDeref(dd,hlocal);
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	(*conjuncts)[0] = hlocal;
	return(1);
    }

} /* end of Cudd_bddVarConjDecomp */


/**Function********************************************************************

  Synopsis    [Performs two-way disjunctive decomposition of a BDD.]

  Description [Performs two-way disjunctive decomposition of a BDD
  according to a variable. If <code>f</code> is the function of the
  BDD and <code>x</code> is the variable, the decomposition is
  <code>f*x + f*x'</code>.  The variable is chosen so as to balance
  the sizes of the two disjuncts and to keep them small.  Returns the
  number of disjuncts produced, that is, 2 if successful; 1 if no
  meaningful decomposition was found; 0 otherwise.]

  SideEffects [The two disjuncts are returned in an array as side effects.
  The array is allocated by this function. It is the caller's responsibility
  to free it. On successful completion, the disjuncts are already
  referenced. If the function returns 0, the array for the disjuncts is
  not allocated. If the function returns 1, the only factor equals the
  function to be decomposed.]

  SeeAlso     [Cudd_bddVarConjDecomp Cudd_bddApproxDisjDecomp
  Cudd_bddIterDisjDecomp Cudd_bddGenDisjDecomp]

******************************************************************************/
int
Cudd_bddVarDisjDecomp(
  DdManager * dd /* manager */,
  DdNode * f /* function to be decomposed */,
  DdNode *** disjuncts /* address of the array of the disjuncts */)
{
    int result, i;

    result = Cudd_bddVarConjDecomp(dd,Cudd_Not(f),disjuncts);
    for (i = 0; i < result; i++) {
	(*disjuncts)[i] = Cudd_Not((*disjuncts)[i]);
    }
    return(result);

} /* end of Cudd_bddVarDisjDecomp */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Get longest distance of node from constant.]

  Description [Get longest distance of node from constant. Returns the
  distance of the root from the constant if successful; CUDD_OUT_OF_MEM
  otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static NodeStat *
CreateBotDist(
  DdNode * node,
  st_table * distanceTable)
{
    DdNode *N, *Nv, *Nnv;
    int distance, distanceNv, distanceNnv;
    NodeStat *nodeStat, *nodeStatNv, *nodeStatNnv;

#if 0
    if (Cudd_IsConstant(node)) {
	return(0);
    }
#endif
    
    /* Return the entry in the table if found. */
    N = Cudd_Regular(node);
    if (st_lookup(distanceTable, N, &nodeStat)) {
	nodeStat->localRef++;
	return(nodeStat);
    }

    Nv = cuddT(N);
    Nnv = cuddE(N);
    Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
    Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

    /* Recur on the children. */
    nodeStatNv = CreateBotDist(Nv, distanceTable);
    if (nodeStatNv == NULL) return(NULL);
    distanceNv = nodeStatNv->distance;

    nodeStatNnv = CreateBotDist(Nnv, distanceTable);
    if (nodeStatNnv == NULL) return(NULL);
    distanceNnv = nodeStatNnv->distance;
    /* Store max distance from constant; note sometimes this distance
    ** may be to 0.
    */
    distance = (distanceNv > distanceNnv) ? (distanceNv+1) : (distanceNnv + 1);

    nodeStat = ALLOC(NodeStat, 1);
    if (nodeStat == NULL) {
	return(0);
    }
    nodeStat->distance = distance;
    nodeStat->localRef = 1;
    
    if (st_insert(distanceTable, (char *)N, (char *)nodeStat) ==
	ST_OUT_OF_MEM) {
	return(0);

    }
    return(nodeStat);

} /* end of CreateBotDist */


/**Function********************************************************************

  Synopsis    [Count the number of minterms of each node ina a BDD and
  store it in a hash table.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static double
CountMinterms(
  DdNode * node,
  double  max,
  st_table * mintermTable,
  FILE *fp)
{
    DdNode *N, *Nv, *Nnv;
    double min, minNv, minNnv;
    double *dummy;

    N = Cudd_Regular(node);

    if (cuddIsConstant(N)) {
	if (node == zero) {
	    return(0);
	} else {
	    return(max);
	}
    }

    /* Return the entry in the table if found. */
    if (st_lookup(mintermTable, node, &dummy)) {
	min = *dummy;
	return(min);
    }

    Nv = cuddT(N);
    Nnv = cuddE(N);
    Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
    Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

    /* Recur on the children. */
    minNv = CountMinterms(Nv, max, mintermTable, fp);
    if (minNv == -1.0) return(-1.0);
    minNnv = CountMinterms(Nnv, max, mintermTable, fp);
    if (minNnv == -1.0) return(-1.0);
    min = minNv / 2.0 + minNnv / 2.0;
    /* store 
     */

    dummy = ALLOC(double, 1);
    if (dummy == NULL) return(-1.0);
    *dummy = min;
    if (st_insert(mintermTable, (char *)node, (char *)dummy) == ST_OUT_OF_MEM) {
	(void) fprintf(fp, "st table insert failed\n");
    }
    return(min);

} /* end of CountMinterms */


/**Function********************************************************************

  Synopsis    [Free factors structure]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
ConjunctsFree(
  DdManager * dd,
  Conjuncts * factors)
{
    Cudd_RecursiveDeref(dd, factors->g);
    Cudd_RecursiveDeref(dd, factors->h);
    FREE(factors);
    return;

} /* end of ConjunctsFree */


/**Function********************************************************************

  Synopsis    [Check whether the given pair is in the tables.]

  Description [.Check whether the given pair is in the tables.  gTable
  and hTable are combined.
  absence in both is indicated by 0,
  presence in gTable is indicated by 1,
  presence in hTable by 2 and
  presence in both by 3.
  The values returned by this function are PAIR_ST,
  PAIR_CR, G_ST, G_CR, H_ST, H_CR, BOTH_G, BOTH_H, NONE.
  PAIR_ST implies g in gTable and h in hTable
  PAIR_CR implies g in hTable and h in gTable
  G_ST implies g in gTable and h not in any table
  G_CR implies g in hTable and h not in any table
  H_ST implies h in hTable and g not in any table
  H_CR implies h in gTable and g not in any table
  BOTH_G implies both in gTable
  BOTH_H implies both in hTable
  NONE implies none in table; ]

  SideEffects []

  SeeAlso     [CheckTablesCacheAndReturn CheckInTables]

******************************************************************************/
static int
PairInTables(
  DdNode * g,
  DdNode * h,
  st_table * ghTable)
{
    int valueG, valueH, gPresent, hPresent;

    valueG = valueH = gPresent = hPresent = 0;
    
    gPresent = st_lookup_int(ghTable, (char *)Cudd_Regular(g), &valueG);
    hPresent = st_lookup_int(ghTable, (char *)Cudd_Regular(h), &valueH);

    if (!gPresent && !hPresent) return(NONE);

    if (!hPresent) {
	if (valueG & 1) return(G_ST);
	if (valueG & 2) return(G_CR);
    }
    if (!gPresent) {
	if (valueH & 1) return(H_CR);
	if (valueH & 2) return(H_ST);
    }
    /* both in tables */
    if ((valueG & 1) && (valueH & 2)) return(PAIR_ST);
    if ((valueG & 2) && (valueH & 1)) return(PAIR_CR);
    
    if (valueG & 1) {
	return(BOTH_G);
    } else {
	return(BOTH_H);
    }
    
} /* end of PairInTables */


/**Function********************************************************************

  Synopsis    [Check the tables for the existence of pair and return one
  combination, cache the result.]

  Description [Check the tables for the existence of pair and return
  one combination, cache the result. The assumption is that one of the
  conjuncts is already in the tables.]

  SideEffects [g and h referenced for the cache]

  SeeAlso     [ZeroCase]

******************************************************************************/
static Conjuncts *
CheckTablesCacheAndReturn(
  DdNode * node,
  DdNode * g,
  DdNode * h,
  st_table * ghTable,
  st_table * cacheTable)
{
    int pairValue;
    int value;
    Conjuncts *factors;
    
    value = 0;
    /* check tables */
    pairValue = PairInTables(g, h, ghTable);
    assert(pairValue != NONE);
    /* if both dont exist in table, we know one exists(either g or h).
     * Therefore store the other and proceed
     */
    factors = ALLOC(Conjuncts, 1);
    if (factors == NULL) return(NULL);
    if ((pairValue == BOTH_H) || (pairValue == H_ST)) {
	if (g != one) {
	    value = 0;
	    if (st_lookup_int(ghTable, (char *)Cudd_Regular(g), &value)) {
		value |= 1;
	    } else {
		value = 1;
	    }
	    if (st_insert(ghTable, (char *)Cudd_Regular(g),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		return(NULL);
	    }
	}
	factors->g = g;
	factors->h = h;
    } else  if ((pairValue == BOTH_G) || (pairValue == G_ST)) {
	if (h != one) {
	    value = 0;
	    if (st_lookup_int(ghTable, (char *)Cudd_Regular(h), &value)) {
		value |= 2;
	    } else {
		value = 2;
	    }
	    if (st_insert(ghTable, (char *)Cudd_Regular(h),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		return(NULL);
	    }
	}
	factors->g = g;
	factors->h = h;
    } else if (pairValue == H_CR) {
	if (g != one) {
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		return(NULL);
	    }
	}
	factors->g = h;
	factors->h = g;
    } else if (pairValue == G_CR) {
	if (h != one) {
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		return(NULL);
	    }
	}
	factors->g = h;
	factors->h = g;
    } else if (pairValue == PAIR_CR) {
    /* pair exists in table */
	factors->g = h;
	factors->h = g;
    } else if (pairValue == PAIR_ST) {
	factors->g = g;
	factors->h = h;
    }
	    
    /* cache the result for this node */
    if (st_insert(cacheTable, (char *)node, (char *)factors) == ST_OUT_OF_MEM) {
	FREE(factors);
	return(NULL);
    }

    return(factors);

} /* end of CheckTablesCacheAndReturn */
	
/**Function********************************************************************

  Synopsis    [Check the tables for the existence of pair and return one
  combination, store in cache.]

  Description [Check the tables for the existence of pair and return
  one combination, store in cache. The pair that has more pointers to
  it is picked. An approximation of the number of local pointers is
  made by taking the reference count of the pairs sent. ]

  SideEffects []

  SeeAlso     [ZeroCase BuildConjuncts]

******************************************************************************/
static Conjuncts *
PickOnePair(
  DdNode * node,
  DdNode * g1,
  DdNode * h1,
  DdNode * g2,
  DdNode * h2,
  st_table * ghTable,
  st_table * cacheTable)
{
    int value;
    Conjuncts *factors;
    int oneRef, twoRef;
    
    factors = ALLOC(Conjuncts, 1);
    if (factors == NULL) return(NULL);

    /* count the number of pointers to pair 2 */
    if (h2 == one) {
	twoRef = (Cudd_Regular(g2))->ref;
    } else if (g2 == one) {
	twoRef = (Cudd_Regular(h2))->ref;
    } else {
	twoRef = ((Cudd_Regular(g2))->ref + (Cudd_Regular(h2))->ref)/2;
    }

    /* count the number of pointers to pair 1 */
    if (h1 == one) {
	oneRef  = (Cudd_Regular(g1))->ref;
    } else if (g1 == one) {
	oneRef  = (Cudd_Regular(h1))->ref;
    } else {
	oneRef = ((Cudd_Regular(g1))->ref + (Cudd_Regular(h1))->ref)/2;
    }

    /* pick the pair with higher reference count */
    if (oneRef >= twoRef) {
	factors->g = g1;
	factors->h = h1;
    } else {
	factors->g = g2;
	factors->h = h2;
    }
    
    /*
     * Store computed factors in respective tables to encourage
     * recombination.
     */
    if (factors->g != one) {
	/* insert g in htable */
	value = 0;
	if (st_lookup_int(ghTable, (char *)Cudd_Regular(factors->g), &value)) {
	    if (value == 2) {
		value |= 1;
		if (st_insert(ghTable, (char *)Cudd_Regular(factors->g),
			      (char *)(long)value) == ST_OUT_OF_MEM) {
		    FREE(factors);
		    return(NULL);
		}
	    }
	} else {
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(factors->g),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		FREE(factors);
		return(NULL);
	    }
	}
    }

    if (factors->h != one) {
	/* insert h in htable */
	value = 0;
	if (st_lookup_int(ghTable, (char *)Cudd_Regular(factors->h), &value)) {
	    if (value == 1) {
		value |= 2;
		if (st_insert(ghTable, (char *)Cudd_Regular(factors->h),
			      (char *)(long)value) == ST_OUT_OF_MEM) {
		    FREE(factors);
		    return(NULL);
		}
	    }	    
	} else {
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(factors->h),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		FREE(factors);
		return(NULL);
	    }
	}
    }
    
    /* Store factors in cache table for later use. */
    if (st_insert(cacheTable, (char *)node, (char *)factors) ==
	    ST_OUT_OF_MEM) {
	FREE(factors);
	return(NULL);
    }

    return(factors);

} /* end of PickOnePair */


/**Function********************************************************************

  Synopsis [Check if the two pairs exist in the table, If any of the
  conjuncts do exist, store in the cache and return the corresponding pair.]

  Description [Check if the two pairs exist in the table. If any of
  the conjuncts do exist, store in the cache and return the
  corresponding pair.]

  SideEffects []

  SeeAlso     [ZeroCase BuildConjuncts]

******************************************************************************/
static Conjuncts *
CheckInTables(
  DdNode * node,
  DdNode * g1,
  DdNode * h1,
  DdNode * g2,
  DdNode * h2,
  st_table * ghTable,
  st_table * cacheTable,
  int * outOfMem)
{
    int pairValue1,  pairValue2;
    Conjuncts *factors;
    int value;
    
    *outOfMem = 0;

    /* check existence of pair in table */
    pairValue1 = PairInTables(g1, h1, ghTable);
    pairValue2 = PairInTables(g2, h2, ghTable);

    /* if none of the 4 exist in the gh tables, return NULL */
    if ((pairValue1 == NONE) && (pairValue2 == NONE)) {
	return NULL;
    }
    
    factors = ALLOC(Conjuncts, 1);
    if (factors == NULL) {
	*outOfMem = 1;
	return NULL;
    }

    /* pairs that already exist in the table get preference. */
    if (pairValue1 == PAIR_ST) {
	factors->g = g1;
	factors->h = h1;
    } else if (pairValue2 == PAIR_ST) {
	factors->g = g2;
	factors->h = h2;
    } else if (pairValue1 == PAIR_CR) {
	factors->g = h1;
	factors->h = g1;
    } else if (pairValue2 == PAIR_CR) {
	factors->g = h2;
	factors->h = g2;
    } else if (pairValue1 == G_ST) {
	/* g exists in the table, h is not found in either table */
	factors->g = g1;
	factors->h = h1;
	if (h1 != one) {
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h1),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue1 == BOTH_G) {
	/* g and h are  found in the g table */
	factors->g = g1;
	factors->h = h1;
	if (h1 != one) {
	    value = 3;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h1),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue1 == H_ST) {
	/* h exists in the table, g is not found in either table */
	factors->g = g1;
	factors->h = h1;
	if (g1 != one) {
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g1),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue1 == BOTH_H) {
	/* g and h are  found in the h table */
	factors->g = g1;
	factors->h = h1;
	if (g1 != one) {
	    value = 3;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g1),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue2 == G_ST) {
	/* g exists in the table, h is not found in either table */
	factors->g = g2;
	factors->h = h2;
	if (h2 != one) {
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h2),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if  (pairValue2 == BOTH_G) {
	/* g and h are  found in the g table */
	factors->g = g2;
	factors->h = h2;
	if (h2 != one) {
	    value = 3;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h2),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue2 == H_ST) { 
	/* h exists in the table, g is not found in either table */
	factors->g = g2;
	factors->h = h2;
	if (g2 != one) {
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g2),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue2 == BOTH_H) {
	/* g and h are  found in the h table */
	factors->g = g2;
	factors->h = h2;
	if (g2 != one) {
	    value = 3;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g2),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue1 == G_CR) {
	/* g found in h table and h in none */
	factors->g = h1;
	factors->h = g1;
	if (h1 != one) {
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h1),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue1 == H_CR) {
	/* h found in g table and g in none */
	factors->g = h1;
	factors->h = g1;
	if (g1 != one) {
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g1),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue2 == G_CR) {
	/* g found in h table and h in none */
	factors->g = h2;
	factors->h = g2;
	if (h2 != one) {
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(h2),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    } else if (pairValue2 == H_CR) {
	/* h found in g table and g in none */
	factors->g = h2;
	factors->h = g2;
	if (g2 != one) {
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(g2),
			  (char *)(long)value) == ST_OUT_OF_MEM) {
		*outOfMem = 1;
		FREE(factors);
		return(NULL);
	    }
	}
    }
    
    /* Store factors in cache table for later use. */
    if (st_insert(cacheTable, (char *)node, (char *)factors) ==
	    ST_OUT_OF_MEM) {
	*outOfMem = 1;
	FREE(factors);
	return(NULL);
    }
    return factors;
} /* end of CheckInTables */



/**Function********************************************************************

  Synopsis    [If one child is zero, do explicitly what Restrict does or better]

  Description [If one child is zero, do explicitly what Restrict does or better.
  First separate a variable and its child in the base case. In case of a cube
  times a function, separate the cube and function. As a last resort, look in
  tables.]

  SideEffects [Frees the BDDs in factorsNv. factorsNv itself is not freed
  because it is freed above.]

  SeeAlso     [BuildConjuncts]

******************************************************************************/
static Conjuncts *
ZeroCase(
  DdManager * dd,
  DdNode * node,
  Conjuncts * factorsNv,
  st_table * ghTable,
  st_table * cacheTable,
  int switched)
{
    int topid;
    DdNode *g, *h, *g1, *g2, *h1, *h2, *x, *N, *G, *H, *Gv, *Gnv;
    DdNode *Hv, *Hnv;
    int value;
    int outOfMem;
    Conjuncts *factors;
    
    /* get var at this node */
    N = Cudd_Regular(node);
    topid = N->index;
    x = dd->vars[topid];
    x = (switched) ? Cudd_Not(x): x;
    cuddRef(x);

    /* Seprate variable and child */
    if (factorsNv->g == one) {
	Cudd_RecursiveDeref(dd, factorsNv->g);
	factors = ALLOC(Conjuncts, 1);
	if (factors == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    Cudd_RecursiveDeref(dd, factorsNv->h);
	    Cudd_RecursiveDeref(dd, x);
	    return(NULL);
	}
	factors->g = x;
	factors->h = factorsNv->h;
	/* cache the result*/
	if (st_insert(cacheTable, (char *)node, (char *)factors) == ST_OUT_OF_MEM) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    Cudd_RecursiveDeref(dd, factorsNv->h); 
	    Cudd_RecursiveDeref(dd, x);
	    FREE(factors);
	    return NULL;
	}
	
	/* store  x in g table, the other node is already in the table */
	if (st_lookup_int(ghTable, (char *)Cudd_Regular(x), &value)) {
	    value |= 1;
	} else {
	    value = 1;
	}
	if (st_insert(ghTable, (char *)Cudd_Regular(x), (char *)(long)value) == ST_OUT_OF_MEM) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return NULL;
	}
	return(factors);
    }
    
    /* Seprate variable and child */
    if (factorsNv->h == one) {
	Cudd_RecursiveDeref(dd, factorsNv->h);
	factors = ALLOC(Conjuncts, 1);
	if (factors == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    Cudd_RecursiveDeref(dd, factorsNv->g);
	    Cudd_RecursiveDeref(dd, x);
	    return(NULL);
	}
	factors->g = factorsNv->g;
	factors->h = x;
	/* cache the result. */
 	if (st_insert(cacheTable, (char *)node, (char *)factors) == ST_OUT_OF_MEM) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    Cudd_RecursiveDeref(dd, factorsNv->g);
	    Cudd_RecursiveDeref(dd, x);
	    FREE(factors);
	    return(NULL);
	}
	/* store x in h table,  the other node is already in the table */
	if (st_lookup_int(ghTable, (char *)Cudd_Regular(x), &value)) {
	    value |= 2;
	} else {
	    value = 2;
	}
	if (st_insert(ghTable, (char *)Cudd_Regular(x), (char *)(long)value) == ST_OUT_OF_MEM) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return NULL;
	}
	return(factors);
    }

    G = Cudd_Regular(factorsNv->g);
    Gv = cuddT(G);
    Gnv = cuddE(G);
    Gv = Cudd_NotCond(Gv, Cudd_IsComplement(node));
    Gnv = Cudd_NotCond(Gnv, Cudd_IsComplement(node));
    /* if the child below is a variable */
    if ((Gv == zero) || (Gnv == zero)) {
	h = factorsNv->h;
	g = cuddBddAndRecur(dd, x, factorsNv->g);
	if (g != NULL) 	cuddRef(g);
	Cudd_RecursiveDeref(dd, factorsNv->g); 
	Cudd_RecursiveDeref(dd, x);
	if (g == NULL) {
	    Cudd_RecursiveDeref(dd, factorsNv->h); 
	    return NULL;
	}
	/* CheckTablesCacheAndReturn responsible for allocating
	 * factors structure., g,h referenced for cache store  the
	 */
	factors = CheckTablesCacheAndReturn(node,
					    g,
					    h,
					    ghTable,
					    cacheTable);
	if (factors == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    Cudd_RecursiveDeref(dd, g);
	    Cudd_RecursiveDeref(dd, h);
	}
	return(factors); 
    }

    H = Cudd_Regular(factorsNv->h);
    Hv = cuddT(H);
    Hnv = cuddE(H);
    Hv = Cudd_NotCond(Hv, Cudd_IsComplement(node));
    Hnv = Cudd_NotCond(Hnv, Cudd_IsComplement(node));
    /* if the child below is a variable */
    if ((Hv == zero) || (Hnv == zero)) {
	g = factorsNv->g;
	h = cuddBddAndRecur(dd, x, factorsNv->h);
	if (h!= NULL) cuddRef(h);
	Cudd_RecursiveDeref(dd, factorsNv->h);
	Cudd_RecursiveDeref(dd, x);
	if (h == NULL) {
	    Cudd_RecursiveDeref(dd, factorsNv->g);
	    return NULL;
	}
	/* CheckTablesCacheAndReturn responsible for allocating
	 * factors structure.g,h referenced for table store 
	 */
	factors = CheckTablesCacheAndReturn(node,
					    g,
					    h,
					    ghTable,
					    cacheTable);
	if (factors == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    Cudd_RecursiveDeref(dd, g);
	    Cudd_RecursiveDeref(dd, h);
	}
	return(factors); 
    }

    /* build g1 = x*g; h1 = h */
    /* build g2 = g; h2 = x*h */
    Cudd_RecursiveDeref(dd, x);
    h1 = factorsNv->h;
    g1 = cuddBddAndRecur(dd, x, factorsNv->g);
    if (g1 != NULL) cuddRef(g1);
    if (g1 == NULL) {
	Cudd_RecursiveDeref(dd, factorsNv->g); 
	Cudd_RecursiveDeref(dd, factorsNv->h);
	return NULL;
    }
    
    g2 = factorsNv->g;
    h2 = cuddBddAndRecur(dd, x, factorsNv->h);
    if (h2 != NULL) cuddRef(h2);
    if (h2 == NULL) {
	Cudd_RecursiveDeref(dd, factorsNv->h);
	Cudd_RecursiveDeref(dd, factorsNv->g);
	return NULL;
    }

    /* check whether any pair is in tables */
    factors = CheckInTables(node, g1, h1, g2, h2, ghTable, cacheTable, &outOfMem);
    if (outOfMem) {
	dd->errorCode = CUDD_MEMORY_OUT;
	Cudd_RecursiveDeref(dd, g1);
	Cudd_RecursiveDeref(dd, h1);
	Cudd_RecursiveDeref(dd, g2);
	Cudd_RecursiveDeref(dd, h2);
	return NULL;
    }
    if (factors != NULL) {
	if ((factors->g == g1) || (factors->g == h1)) {
	    Cudd_RecursiveDeref(dd, g2);
	    Cudd_RecursiveDeref(dd, h2);
	} else {
	    Cudd_RecursiveDeref(dd, g1);
	    Cudd_RecursiveDeref(dd, h1);
	}
	return factors;
    }

    /* check for each pair in tables and choose one */
    factors = PickOnePair(node,g1, h1, g2, h2, ghTable, cacheTable);
    if (factors == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	Cudd_RecursiveDeref(dd, g1);
	Cudd_RecursiveDeref(dd, h1);
	Cudd_RecursiveDeref(dd, g2);
	Cudd_RecursiveDeref(dd, h2);
    } else {
	/* now free what was created and not used */
	if ((factors->g == g1) || (factors->g == h1)) {
	    Cudd_RecursiveDeref(dd, g2);
	    Cudd_RecursiveDeref(dd, h2);
	} else {
	    Cudd_RecursiveDeref(dd, g1);
	    Cudd_RecursiveDeref(dd, h1);
	}
    }
	
    return(factors);
} /* end of ZeroCase */


/**Function********************************************************************

  Synopsis    [Builds the conjuncts recursively, bottom up.]

  Description [Builds the conjuncts recursively, bottom up. Constants
  are returned as (f, f). The cache is checked for previously computed
  result. The decomposition points are determined by the local
  reference count of this node and the longest distance from the
  constant. At the decomposition point, the factors returned are (f,
  1). Recur on the two children. The order is determined by the
  heavier branch. Combine the factors of the two children and pick the
  one that already occurs in the gh table. Occurence in g is indicated
  by value 1, occurence in h by 2, occurence in both 3.]

  SideEffects []

  SeeAlso     [cuddConjunctsAux]

******************************************************************************/
static Conjuncts *
BuildConjuncts(
  DdManager * dd,
  DdNode * node,
  st_table * distanceTable,
  st_table * cacheTable,
  int approxDistance,
  int maxLocalRef,
  st_table * ghTable,
  st_table * mintermTable)
{
    int topid, distance;
    Conjuncts *factorsNv, *factorsNnv, *factors;
    Conjuncts *dummy;
    DdNode *N, *Nv, *Nnv, *temp, *g1, *g2, *h1, *h2, *topv;
    double minNv = 0.0, minNnv = 0.0;
    double *doubleDummy;
    int switched =0;
    int outOfMem;
    int freeNv = 0, freeNnv = 0, freeTemp;
    NodeStat *nodeStat;
    int value;

    /* if f is constant, return (f,f) */
    if (Cudd_IsConstant(node)) {
	factors = ALLOC(Conjuncts, 1);
	if (factors == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(NULL);
	}
	factors->g = node;
	factors->h = node;
	return(FactorsComplement(factors));
    }

    /* If result (a pair of conjuncts) in cache, return the factors. */
    if (st_lookup(cacheTable, node, &dummy)) {
	factors = dummy;
	return(factors);
    }
    
    /* check distance and local reference count of this node */
    N = Cudd_Regular(node);
    if (!st_lookup(distanceTable, N, &nodeStat)) {
	(void) fprintf(dd->err, "Not in table, Something wrong\n");
	dd->errorCode = CUDD_INTERNAL_ERROR;
	return(NULL);
    }
    distance = nodeStat->distance;

    /* at or below decomposition point, return (f, 1) */
    if (((nodeStat->localRef > maxLocalRef*2/3) &&
	 (distance < approxDistance*2/3)) ||
	    (distance <= approxDistance/4)) {
	factors = ALLOC(Conjuncts, 1);
	if (factors == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(NULL);
	}
	/* alternate assigning (f,1) */
	value = 0;
	if (st_lookup_int(ghTable, (char *)Cudd_Regular(node), &value)) {
	    if (value == 3) {
		if (!lastTimeG) {
		    factors->g = node;
		    factors->h = one;
		    lastTimeG = 1;
		} else {
		    factors->g = one;
		    factors->h = node;
		    lastTimeG = 0; 
		}
	    } else if (value == 1) {
		factors->g = node;
		factors->h = one;
	    } else {
		factors->g = one;
		factors->h = node;
	    }
	} else if (!lastTimeG) {
	    factors->g = node;
	    factors->h = one;
	    lastTimeG = 1;
	    value = 1;
	    if (st_insert(ghTable, (char *)Cudd_Regular(node), (char *)(long)value) == ST_OUT_OF_MEM) {
		dd->errorCode = CUDD_MEMORY_OUT;
		FREE(factors);
		return NULL;
	    }
	} else {
	    factors->g = one;
	    factors->h = node;
	    lastTimeG = 0;
	    value = 2;
	    if (st_insert(ghTable, (char *)Cudd_Regular(node), (char *)(long)value) == ST_OUT_OF_MEM) {
		dd->errorCode = CUDD_MEMORY_OUT;
		FREE(factors);
		return NULL;
	    }
	}
	return(FactorsComplement(factors));
    }
    
    /* get the children and recur */
    Nv = cuddT(N);
    Nnv = cuddE(N);
    Nv = Cudd_NotCond(Nv, Cudd_IsComplement(node));
    Nnv = Cudd_NotCond(Nnv, Cudd_IsComplement(node));

    /* Choose which subproblem to solve first based on the number of
     * minterms. We go first where there are more minterms.
     */
    if (!Cudd_IsConstant(Nv)) {
	if (!st_lookup(mintermTable, Nv, &doubleDummy)) {
	    (void) fprintf(dd->err, "Not in table: Something wrong\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
	}
	minNv = *doubleDummy;
    }
    
    if (!Cudd_IsConstant(Nnv)) {
	if (!st_lookup(mintermTable, Nnv, &doubleDummy)) {
	    (void) fprintf(dd->err, "Not in table: Something wrong\n");
	    dd->errorCode = CUDD_INTERNAL_ERROR;
	    return(NULL);
	}
	minNnv = *doubleDummy;
    }
    
    if (minNv < minNnv) {
	temp = Nv;
	Nv = Nnv;
	Nnv = temp;
	switched = 1;
    }

    /* build gt, ht recursively */
    if (Nv != zero) {
	factorsNv = BuildConjuncts(dd, Nv, distanceTable,
				   cacheTable, approxDistance, maxLocalRef, 
				   ghTable, mintermTable);
	if (factorsNv == NULL) return(NULL);
	freeNv = FactorsNotStored(factorsNv);
	factorsNv = (freeNv) ? FactorsUncomplement(factorsNv) : factorsNv;
	cuddRef(factorsNv->g);
	cuddRef(factorsNv->h);
	
	/* Deal with the zero case */
	if (Nnv == zero) {
	    /* is responsible for freeing factorsNv */
	    factors = ZeroCase(dd, node, factorsNv, ghTable,
			       cacheTable, switched);
	    if (freeNv) FREE(factorsNv);
	    return(factors);
	}
    }

    /* build ge, he recursively */
    if (Nnv != zero) {
	factorsNnv = BuildConjuncts(dd, Nnv, distanceTable,
				    cacheTable, approxDistance, maxLocalRef,
				    ghTable, mintermTable);
	if (factorsNnv == NULL) {
	    Cudd_RecursiveDeref(dd, factorsNv->g);
	    Cudd_RecursiveDeref(dd, factorsNv->h);
	    if (freeNv) FREE(factorsNv);
	    return(NULL);
	}
	freeNnv = FactorsNotStored(factorsNnv);
	factorsNnv = (freeNnv) ? FactorsUncomplement(factorsNnv) : factorsNnv;
	cuddRef(factorsNnv->g);
	cuddRef(factorsNnv->h);
	
	/* Deal with the zero case */
	if (Nv == zero) {
	    /* is responsible for freeing factorsNv */
	    factors = ZeroCase(dd, node, factorsNnv, ghTable,
			       cacheTable, switched);
	    if (freeNnv) FREE(factorsNnv);
	    return(factors);
	}
    }

    /* construct the 2 pairs */
    /* g1 = x*gt + x'*ge; h1 = x*ht + x'*he; */
    /* g2 = x*gt + x'*he; h2 = x*ht + x'*ge */
    if (switched) {
	factors = factorsNnv;
	factorsNnv = factorsNv;
	factorsNv = factors;
	freeTemp = freeNv;
	freeNv = freeNnv;
	freeNnv = freeTemp;
    }

    /* Build the factors for this node. */
    topid = N->index;
    topv = dd->vars[topid];
    
    g1 = cuddBddIteRecur(dd, topv, factorsNv->g, factorsNnv->g);
    if (g1 == NULL) {
	Cudd_RecursiveDeref(dd, factorsNv->g);
	Cudd_RecursiveDeref(dd, factorsNv->h);
	Cudd_RecursiveDeref(dd, factorsNnv->g);
	Cudd_RecursiveDeref(dd, factorsNnv->h);
	if (freeNv) FREE(factorsNv);
	if (freeNnv) FREE(factorsNnv);
	return(NULL);
    }

    cuddRef(g1);

    h1 = cuddBddIteRecur(dd, topv, factorsNv->h, factorsNnv->h);
    if (h1 == NULL) {
	Cudd_RecursiveDeref(dd, factorsNv->g);
	Cudd_RecursiveDeref(dd, factorsNv->h);
	Cudd_RecursiveDeref(dd, factorsNnv->g);
	Cudd_RecursiveDeref(dd, factorsNnv->h);
	Cudd_RecursiveDeref(dd, g1);
	if (freeNv) FREE(factorsNv);
	if (freeNnv) FREE(factorsNnv);
	return(NULL);
    }

    cuddRef(h1);

    g2 = cuddBddIteRecur(dd, topv, factorsNv->g, factorsNnv->h);
    if (g2 == NULL) {
	Cudd_RecursiveDeref(dd, factorsNv->h);
	Cudd_RecursiveDeref(dd, factorsNv->g);
	Cudd_RecursiveDeref(dd, factorsNnv->g);
	Cudd_RecursiveDeref(dd, factorsNnv->h);
	Cudd_RecursiveDeref(dd, g1);
	Cudd_RecursiveDeref(dd, h1);
	if (freeNv) FREE(factorsNv);
	if (freeNnv) FREE(factorsNnv);
	return(NULL);
    }
    cuddRef(g2);
    Cudd_RecursiveDeref(dd, factorsNv->g);
    Cudd_RecursiveDeref(dd, factorsNnv->h);

    h2 = cuddBddIteRecur(dd, topv, factorsNv->h, factorsNnv->g);
    if (h2 == NULL) {
	Cudd_RecursiveDeref(dd, factorsNv->g);
	Cudd_RecursiveDeref(dd, factorsNv->h);
	Cudd_RecursiveDeref(dd, factorsNnv->g);
	Cudd_RecursiveDeref(dd, factorsNnv->h);
	Cudd_RecursiveDeref(dd, g1);
	Cudd_RecursiveDeref(dd, h1);
	Cudd_RecursiveDeref(dd, g2);
	if (freeNv) FREE(factorsNv);
	if (freeNnv) FREE(factorsNnv);
	return(NULL);
    }
    cuddRef(h2);
    Cudd_RecursiveDeref(dd, factorsNv->h);
    Cudd_RecursiveDeref(dd, factorsNnv->g);
    if (freeNv) FREE(factorsNv);
    if (freeNnv) FREE(factorsNnv);

    /* check for each pair in tables and choose one */
    factors = CheckInTables(node, g1, h1, g2, h2, ghTable, cacheTable, &outOfMem);
    if (outOfMem) {
	dd->errorCode = CUDD_MEMORY_OUT;
	Cudd_RecursiveDeref(dd, g1);
	Cudd_RecursiveDeref(dd, h1);
	Cudd_RecursiveDeref(dd, g2);
	Cudd_RecursiveDeref(dd, h2);
	return(NULL);
    }
    if (factors != NULL) {
	if ((factors->g == g1) || (factors->g == h1)) {
	    Cudd_RecursiveDeref(dd, g2);
	    Cudd_RecursiveDeref(dd, h2);
	} else {
	    Cudd_RecursiveDeref(dd, g1);
	    Cudd_RecursiveDeref(dd, h1);
	}
	return(factors);
    }

    /* if not in tables, pick one pair */
    factors = PickOnePair(node,g1, h1, g2, h2, ghTable, cacheTable);
    if (factors == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	Cudd_RecursiveDeref(dd, g1);
	Cudd_RecursiveDeref(dd, h1);
	Cudd_RecursiveDeref(dd, g2);
	Cudd_RecursiveDeref(dd, h2);
    } else {
	/* now free what was created and not used */
	if ((factors->g == g1) || (factors->g == h1)) {
	    Cudd_RecursiveDeref(dd, g2);
	    Cudd_RecursiveDeref(dd, h2);
	} else {
	    Cudd_RecursiveDeref(dd, g1);
	    Cudd_RecursiveDeref(dd, h1);
	}
    }
	
    return(factors);
    
} /* end of BuildConjuncts */


/**Function********************************************************************

  Synopsis    [Procedure to compute two conjunctive factors of f and place in *c1 and *c2.]

  Description [Procedure to compute two conjunctive factors of f and
  place in *c1 and *c2. Sets up the required data - table of distances
  from the constant and local reference count. Also minterm table. ]

  SideEffects []

  SeeAlso     []

******************************************************************************/
static int
cuddConjunctsAux(
  DdManager * dd,
  DdNode * f,
  DdNode ** c1,
  DdNode ** c2)
{
    st_table *distanceTable = NULL;
    st_table *cacheTable = NULL;
    st_table *mintermTable = NULL;
    st_table *ghTable = NULL;
    st_generator *stGen;
    char *key, *value;
    Conjuncts *factors;
    int distance, approxDistance;
    double max, minterms;
    int freeFactors;
    NodeStat *nodeStat;
    int maxLocalRef;
    
    /* initialize */
    *c1 = NULL;
    *c2 = NULL;

    /* initialize distances table */
    distanceTable = st_init_table(st_ptrcmp,st_ptrhash);
    if (distanceTable == NULL) goto outOfMem;
    
    /* make the entry for the constant */
    nodeStat = ALLOC(NodeStat, 1);
    if (nodeStat == NULL) goto outOfMem;
    nodeStat->distance = 0;
    nodeStat->localRef = 1;
    if (st_insert(distanceTable, (char *)one, (char *)nodeStat) == ST_OUT_OF_MEM) {
	goto outOfMem;
    }

    /* Count node distances from constant. */
    nodeStat = CreateBotDist(f, distanceTable);
    if (nodeStat == NULL) goto outOfMem;

    /* set the distance for the decomposition points */
    approxDistance = (DEPTH < nodeStat->distance) ? nodeStat->distance : DEPTH;
    distance = nodeStat->distance;

    if (distance < approxDistance) {
	/* Too small to bother. */
	*c1 = f;
	*c2 = DD_ONE(dd);
	cuddRef(*c1); cuddRef(*c2);
	stGen = st_init_gen(distanceTable);
	if (stGen == NULL) goto outOfMem;
	while(st_gen(stGen, (char **)&key, (char **)&value)) {
	    FREE(value);
	}
	st_free_gen(stGen); stGen = NULL;
	st_free_table(distanceTable);
	return(1);
    }

    /* record the maximum local reference count */
    maxLocalRef = 0;
    stGen = st_init_gen(distanceTable);
    if (stGen == NULL) goto outOfMem;
    while(st_gen(stGen, (char **)&key, (char **)&value)) {
	nodeStat = (NodeStat *)value;
	maxLocalRef = (nodeStat->localRef > maxLocalRef) ?
	    nodeStat->localRef : maxLocalRef;
    }
    st_free_gen(stGen); stGen = NULL;

	    
    /* Count minterms for each node. */
    max = pow(2.0, (double)Cudd_SupportSize(dd,f)); /* potential overflow */
    mintermTable = st_init_table(st_ptrcmp,st_ptrhash);
    if (mintermTable == NULL) goto outOfMem;
    minterms = CountMinterms(f, max, mintermTable, dd->err);
    if (minterms == -1.0) goto outOfMem;
    
    lastTimeG = Cudd_Random() & 1;
    cacheTable = st_init_table(st_ptrcmp, st_ptrhash);
    if (cacheTable == NULL) goto outOfMem;
    ghTable = st_init_table(st_ptrcmp, st_ptrhash);
    if (ghTable == NULL) goto outOfMem;

    /* Build conjuncts. */
    factors = BuildConjuncts(dd, f, distanceTable, cacheTable,
			     approxDistance, maxLocalRef, ghTable, mintermTable);
    if (factors == NULL) goto outOfMem;

    /* free up tables */
    stGen = st_init_gen(distanceTable);
    if (stGen == NULL) goto outOfMem;
    while(st_gen(stGen, (char **)&key, (char **)&value)) {
	FREE(value);
    }
    st_free_gen(stGen); stGen = NULL;
    st_free_table(distanceTable); distanceTable = NULL;
    st_free_table(ghTable); ghTable = NULL;
    
    stGen = st_init_gen(mintermTable);
    if (stGen == NULL) goto outOfMem;
    while(st_gen(stGen, (char **)&key, (char **)&value)) {
	FREE(value);
    }
    st_free_gen(stGen); stGen = NULL;
    st_free_table(mintermTable); mintermTable = NULL;

    freeFactors = FactorsNotStored(factors);
    factors = (freeFactors) ? FactorsUncomplement(factors) : factors;
    if (factors != NULL) {
	*c1 = factors->g;
	*c2 = factors->h;
	cuddRef(*c1);
	cuddRef(*c2);
	if (freeFactors) FREE(factors);
	
#if 0    
	if ((*c1 == f) && (!Cudd_IsConstant(f))) {
	    assert(*c2 == one);
	}
	if ((*c2 == f) && (!Cudd_IsConstant(f))) {
	    assert(*c1 == one);
	}
	
	if ((*c1 != one) && (!Cudd_IsConstant(f))) {
	    assert(!Cudd_bddLeq(dd, *c2, *c1));
	}
	if ((*c2 != one) && (!Cudd_IsConstant(f))) {
	    assert(!Cudd_bddLeq(dd, *c1, *c2));
	}
#endif
    }

    stGen = st_init_gen(cacheTable);
    if (stGen == NULL) goto outOfMem;
    while(st_gen(stGen, (char **)&key, (char **)&value)) {
	ConjunctsFree(dd, (Conjuncts *)value);
    }
    st_free_gen(stGen); stGen = NULL;

    st_free_table(cacheTable); cacheTable = NULL;

    return(1);

outOfMem:
    if (distanceTable != NULL) {
	stGen = st_init_gen(distanceTable);
	if (stGen == NULL) goto outOfMem;
	while(st_gen(stGen, (char **)&key, (char **)&value)) {
	    FREE(value);
	}
	st_free_gen(stGen); stGen = NULL;
	st_free_table(distanceTable); distanceTable = NULL;
    }
    if (mintermTable != NULL) {
	stGen = st_init_gen(mintermTable);
	if (stGen == NULL) goto outOfMem;
	while(st_gen(stGen, (char **)&key, (char **)&value)) {
	    FREE(value);
	}
	st_free_gen(stGen); stGen = NULL;
	st_free_table(mintermTable); mintermTable = NULL;
    }
    if (ghTable != NULL) st_free_table(ghTable);
    if (cacheTable != NULL) {
	stGen = st_init_gen(cacheTable);
	if (stGen == NULL) goto outOfMem;
	while(st_gen(stGen, (char **)&key, (char **)&value)) {
	    ConjunctsFree(dd, (Conjuncts *)value);
	}
	st_free_gen(stGen); stGen = NULL;
	st_free_table(cacheTable); cacheTable = NULL;
    }
    dd->errorCode = CUDD_MEMORY_OUT;
    return(0);

} /* end of cuddConjunctsAux */
