/**CFile***********************************************************************

  FileName    [cuddEssent.c]

  PackageName [cudd]

  Synopsis    [Functions for the detection of essential variables.]

  Description [External procedures included in this file:
		<ul>
		<li> Cudd_FindEssential()
		<li> Cudd_bddIsVarEssential()
		<li> Cudd_FindTwoLiteralClauses()
		<li> Cudd_ReadIthClause()
		<li> Cudd_PrintTwoLiteralClauses()
		<li> Cudd_tlcInfoFree()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> ddFindEssentialRecur()
		<li> ddFindTwoLiteralClausesRecur()
		<li> computeClauses()
		<li> computeClausesWithUniverse()
		<li> emptyClauseSet()
		<li> sentinelp()
		<li> equalp()
		<li> beforep()
		<li> oneliteralp()
		<li> impliedp()
		<li> bitVectorAlloc()
		<li> bitVectorClear()
		<li> bitVectorFree()
		<li> bitVectorRead()
		<li> bitVectorSet()
		<li> tlcInfoAlloc()
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

/* These definitions are for the bit vectors. */
#if SIZEOF_LONG == 8
#define BPL 64
#define LOGBPL 6
#else
#define BPL 32
#define LOGBPL 5
#endif

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/* This structure holds the set of clauses for a node.  Each clause consists
** of two literals.  For one-literal clauses, the second lietral is FALSE.
** Each literal is composed of a variable and a phase.  A variable is a node
** index, and requires sizeof(DdHalfWord) bytes.  The constant literals use
** CUDD_MAXINDEX as variable indicator.  Each phase is a bit: 0 for positive
** phase, and 1 for negative phase.
** Variables and phases are stored separately for the sake of compactness.
** The variables are stored in an array of DdHalfWord's terminated by a
** sentinel (a pair of zeroes).  The phases are stored in a bit vector.
** The cnt field holds, at the end, the number of clauses.
** The clauses of the set are kept sorted.  For each clause, the first literal
** is the one of least index.  So, the clause with literals +2 and -4 is stored
** as (+2,-4).  A one-literal clause with literal +3 is stored as
** (+3,-CUDD_MAXINDEX).  Clauses are sorted in decreasing order as follows:
**      (+5,-7)
**      (+5,+6)
**      (-5,+7)
**      (-4,FALSE)
**      (-4,+8)
**      ...
** That is, one first looks at the variable of the first literal, then at the
** phase of the first litral, then at the variable of the second literal,
** and finally at the phase of the second literal.
*/
struct DdTlcInfo {
    DdHalfWord *vars;
    long *phases;
    DdHalfWord cnt;
};

/* This structure is for temporary representation of sets of clauses.  It is
** meant to be used in link lists, when the number of clauses is not yet
** known. The encoding of a clause is the same as in DdTlcInfo, though
** the phase information is not stored in a bit array. */
struct TlClause {
    DdHalfWord v1, v2;
    short p1, p2;
    struct TlClause *next;
};

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

typedef long BitVector;
typedef struct TlClause TlClause;

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddEssent.c,v 1.25 2012/02/05 01:07:18 fabio Exp $";
#endif

static BitVector *Tolv;
static BitVector *Tolp;
static BitVector *Eolv;
static BitVector *Eolp;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdNode * ddFindEssentialRecur (DdManager *dd, DdNode *f);
static DdTlcInfo * ddFindTwoLiteralClausesRecur (DdManager * dd, DdNode * f, st_table *table);
static DdTlcInfo * computeClauses (DdTlcInfo *Tres, DdTlcInfo *Eres, DdHalfWord label, int size);
static DdTlcInfo * computeClausesWithUniverse (DdTlcInfo *Cres, DdHalfWord label, short phase);
static DdTlcInfo * emptyClauseSet (void);
static int sentinelp (DdHalfWord var1, DdHalfWord var2);
static int equalp (DdHalfWord var1a, short phase1a, DdHalfWord var1b, short phase1b, DdHalfWord var2a, short phase2a, DdHalfWord var2b, short phase2b);
static int beforep (DdHalfWord var1a, short phase1a, DdHalfWord var1b, short phase1b, DdHalfWord var2a, short phase2a, DdHalfWord var2b, short phase2b);
static int oneliteralp (DdHalfWord var);
static int impliedp (DdHalfWord var1, short phase1, DdHalfWord var2, short phase2, BitVector *olv, BitVector *olp);
static BitVector * bitVectorAlloc (int size);
DD_INLINE static void bitVectorClear (BitVector *vector, int size);
static void bitVectorFree (BitVector *vector);
DD_INLINE static short bitVectorRead (BitVector *vector, int i);
DD_INLINE static void bitVectorSet (BitVector * vector, int i, short val);
static DdTlcInfo * tlcInfoAlloc (void);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Finds the essential variables of a DD.]

  Description [Returns the cube of the essential variables. A positive
  literal means that the variable must be set to 1 for the function to be
  1. A negative literal means that the variable must be set to 0 for the
  function to be 1. Returns a pointer to the cube BDD if successful;
  NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIsVarEssential]

******************************************************************************/
DdNode *
Cudd_FindEssential(
  DdManager * dd,
  DdNode * f)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = ddFindEssentialRecur(dd,f);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_FindEssential */


/**Function********************************************************************

  Synopsis    [Determines whether a given variable is essential with a
  given phase in a BDD.]

  Description [Determines whether a given variable is essential with a
  given phase in a BDD. Uses Cudd_bddIteConstant. Returns 1 if phase == 1
  and f-->x_id, or if phase == 0 and f-->x_id'.]

  SideEffects [None]

  SeeAlso     [Cudd_FindEssential]

******************************************************************************/
int
Cudd_bddIsVarEssential(
  DdManager * manager,
  DdNode * f,
  int  id,
  int  phase)
{
    DdNode	*var;
    int		res;

    var = Cudd_bddIthVar(manager, id);

    var = Cudd_NotCond(var,phase == 0);

    res = Cudd_bddLeq(manager, f, var);

    return(res);

} /* end of Cudd_bddIsVarEssential */


/**Function********************************************************************

  Synopsis    [Finds the two literal clauses of a DD.]

  Description [Returns the one- and two-literal clauses of a DD.
  Returns a pointer to the structure holding the clauses if
  successful; NULL otherwise.  For a constant DD, the empty set of clauses
  is returned.  This is obviously correct for a non-zero constant.  For the
  constant zero, it is based on the assumption that only those clauses
  containing variables in the support of the function are considered.  Since
  the support of a constant function is empty, no clauses are returned.]

  SideEffects [None]

  SeeAlso     [Cudd_FindEssential]

******************************************************************************/
DdTlcInfo *
Cudd_FindTwoLiteralClauses(
  DdManager * dd,
  DdNode * f)
{
    DdTlcInfo *res;
    st_table *table;
    st_generator *gen;
    DdTlcInfo *tlc;
    DdNode *node;
    int size = dd->size;

    if (Cudd_IsConstant(f)) {
	res = emptyClauseSet();
	return(res);
    }
    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) return(NULL);
    Tolv = bitVectorAlloc(size);
    if (Tolv == NULL) {
	st_free_table(table);
	return(NULL);
    }
    Tolp = bitVectorAlloc(size);
    if (Tolp == NULL) {
	st_free_table(table);
	bitVectorFree(Tolv);
	return(NULL);
    }
    Eolv = bitVectorAlloc(size);
    if (Eolv == NULL) {
	st_free_table(table);
	bitVectorFree(Tolv);
	bitVectorFree(Tolp);
	return(NULL);
    }
    Eolp = bitVectorAlloc(size);
    if (Eolp == NULL) {
	st_free_table(table);
	bitVectorFree(Tolv);
	bitVectorFree(Tolp);
	bitVectorFree(Eolv);
	return(NULL);
    }

    res = ddFindTwoLiteralClausesRecur(dd,f,table);
    /* Dispose of table contents and free table. */
    st_foreach_item(table, gen, &node, &tlc) {
	if (node != f) {
	    Cudd_tlcInfoFree(tlc);
	}
    }
    st_free_table(table);
    bitVectorFree(Tolv);
    bitVectorFree(Tolp);
    bitVectorFree(Eolv);
    bitVectorFree(Eolp);

    if (res != NULL) {
	int i;
	for (i = 0; !sentinelp(res->vars[i], res->vars[i+1]); i += 2);
	res->cnt = i >> 1;
    }

    return(res);

} /* end of Cudd_FindTwoLiteralClauses */


/**Function********************************************************************

  Synopsis    [Accesses the i-th clause of a DD.]

  Description [Accesses the i-th clause of a DD given the clause set which
  must be already computed.  Returns 1 if successful; 0 if i is out of range,
  or in case of error.]

  SideEffects [the four components of a clause are returned as side effects.]

  SeeAlso     [Cudd_FindTwoLiteralClauses]

******************************************************************************/
int
Cudd_ReadIthClause(
  DdTlcInfo * tlc,
  int i,
  DdHalfWord *var1,
  DdHalfWord *var2,
  int *phase1,
  int *phase2)
{
    if (tlc == NULL) return(0);
    if (tlc->vars == NULL || tlc->phases == NULL) return(0);
    if (i < 0 || (unsigned) i >= tlc->cnt) return(0);
    *var1 = tlc->vars[2*i];
    *var2 = tlc->vars[2*i+1];
    *phase1 = (int) bitVectorRead(tlc->phases, 2*i);
    *phase2 = (int) bitVectorRead(tlc->phases, 2*i+1);
    return(1);

} /* end of Cudd_ReadIthClause */


/**Function********************************************************************

  Synopsis    [Prints the two literal clauses of a DD.]

  Description [Prints the one- and two-literal clauses. Returns 1 if
  successful; 0 otherwise.  The argument "names" can be NULL, in which case
  the variable indices are printed.]

  SideEffects [None]

  SeeAlso     [Cudd_FindTwoLiteralClauses]

******************************************************************************/
int
Cudd_PrintTwoLiteralClauses(
  DdManager * dd,
  DdNode * f,
  char **names,
  FILE *fp)
{
    DdHalfWord *vars;
    BitVector *phases;
    int i;
    DdTlcInfo *res = Cudd_FindTwoLiteralClauses(dd, f);
    FILE *ifp = fp == NULL ? dd->out : fp;

    if (res == NULL) return(0);
    vars = res->vars;
    phases = res->phases;
    for (i = 0; !sentinelp(vars[i], vars[i+1]); i += 2) {
	if (names != NULL) {
	    if (vars[i+1] == CUDD_MAXINDEX) {
		(void) fprintf(ifp, "%s%s\n",
			       bitVectorRead(phases, i) ? "~" : " ",
			       names[vars[i]]);
	    } else {
		(void) fprintf(ifp, "%s%s | %s%s\n",
			       bitVectorRead(phases, i) ? "~" : " ",
			       names[vars[i]],
			       bitVectorRead(phases, i+1) ? "~" : " ",
			       names[vars[i+1]]);
	    }
	} else {
	    if (vars[i+1] == CUDD_MAXINDEX) {
		(void) fprintf(ifp, "%s%d\n",
			       bitVectorRead(phases, i) ? "~" : " ",
			       (int) vars[i]);
	    } else {
		(void) fprintf(ifp, "%s%d | %s%d\n",
			       bitVectorRead(phases, i) ? "~" : " ",
			       (int) vars[i],
			       bitVectorRead(phases, i+1) ? "~" : " ",
			       (int) vars[i+1]);
	    }
	}
    }
    Cudd_tlcInfoFree(res);

    return(1);

} /* end of Cudd_PrintTwoLiteralClauses */


/**Function********************************************************************

  Synopsis    [Frees a DdTlcInfo Structure.]

  Description [Frees a DdTlcInfo Structure as well as the memory pointed
  by it.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
Cudd_tlcInfoFree(
  DdTlcInfo * t)
{
    if (t->vars != NULL) FREE(t->vars);
    if (t->phases != NULL) FREE(t->phases);
    FREE(t);

} /* end of Cudd_tlcInfoFree */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_FindEssential.]

  Description [Implements the recursive step of Cudd_FindEssential.
  Returns a pointer to the cube BDD if successful; NULL otherwise.]

  SideEffects [None]

******************************************************************************/
static DdNode *
ddFindEssentialRecur(
  DdManager * dd,
  DdNode * f)
{
    DdNode	*T, *E, *F;
    DdNode	*essT, *essE, *res;
    int		index;
    DdNode	*one, *lzero, *azero;

    one = DD_ONE(dd);
    F = Cudd_Regular(f);
    /* If f is constant the set of essential variables is empty. */
    if (cuddIsConstant(F)) return(one);

    res = cuddCacheLookup1(dd,Cudd_FindEssential,f);
    if (res != NULL) {
	return(res);
    }

    lzero = Cudd_Not(one);
    azero = DD_ZERO(dd);
    /* Find cofactors: here f is non-constant. */
    T = cuddT(F);
    E = cuddE(F);
    if (Cudd_IsComplement(f)) {
	T = Cudd_Not(T); E = Cudd_Not(E);
    }

    index = F->index;
    if (Cudd_IsConstant(T) && T != lzero && T != azero) {
	/* if E is zero, index is essential, otherwise there are no
	** essentials, because index is not essential and no other variable
	** can be, since setting index = 1 makes the function constant and
	** different from 0.
	*/
	if (E == lzero || E == azero) {
	    res = dd->vars[index];
	} else {
	    res = one;
	}
    } else if (T == lzero || T == azero) {
	if (Cudd_IsConstant(E)) { /* E cannot be zero here */
	    res = Cudd_Not(dd->vars[index]);
	} else { /* E == non-constant */
	    /* find essentials in the else branch */
	    essE = ddFindEssentialRecur(dd,E);
	    if (essE == NULL) {
		return(NULL);
	    }
	    cuddRef(essE);

	    /* add index to the set with negative phase */
	    res = cuddUniqueInter(dd,index,one,Cudd_Not(essE));
	    if (res == NULL) {
		Cudd_RecursiveDeref(dd,essE);
		return(NULL);
	    }
	    res = Cudd_Not(res);
	    cuddDeref(essE);
	}
    } else { /* T == non-const */
	if (E == lzero || E == azero) {
	    /* find essentials in the then branch */
	    essT = ddFindEssentialRecur(dd,T);
	    if (essT == NULL) {
		return(NULL);
	    }
	    cuddRef(essT);

	    /* add index to the set with positive phase */
	    /* use And because essT may be complemented */
	    res = cuddBddAndRecur(dd,dd->vars[index],essT);
	    if (res == NULL) {
		Cudd_RecursiveDeref(dd,essT);
		return(NULL);
	    }
	    cuddDeref(essT);
	} else if (!Cudd_IsConstant(E)) {
	    /* if E is a non-zero constant there are no essentials
	    ** because T is non-constant.
	    */
	    essT = ddFindEssentialRecur(dd,T);
	    if (essT == NULL) {
		return(NULL);
	    }
	    if (essT == one) {
		res = one;
	    } else {
		cuddRef(essT);
		essE = ddFindEssentialRecur(dd,E);
		if (essE == NULL) {
		    Cudd_RecursiveDeref(dd,essT);
		    return(NULL);
		}
		cuddRef(essE);

		/* res = intersection(essT, essE) */
		res = cuddBddLiteralSetIntersectionRecur(dd,essT,essE);
		if (res == NULL) {
		    Cudd_RecursiveDeref(dd,essT);
		    Cudd_RecursiveDeref(dd,essE);
		    return(NULL);
		}
		cuddRef(res);
		Cudd_RecursiveDeref(dd,essT);
		Cudd_RecursiveDeref(dd,essE);
		cuddDeref(res);
	    }
	} else {	/* E is a non-zero constant */
	    res = one;
	}
    }

    cuddCacheInsert1(dd,Cudd_FindEssential, f, res);
    return(res);

} /* end of ddFindEssentialRecur */


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_FindTwoLiteralClauses.]

  Description [Implements the recursive step of
  Cudd_FindTwoLiteralClauses.  The DD node is assumed to be not
  constant.  Returns a pointer to a set of clauses if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_FindTwoLiteralClauses]

******************************************************************************/
static DdTlcInfo *
ddFindTwoLiteralClausesRecur(
  DdManager * dd,
  DdNode * f,
  st_table *table)
{
    DdNode *T, *E, *F;
    DdNode *one, *lzero, *azero;
    DdTlcInfo *res, *Tres, *Eres;
    DdHalfWord index;

    F = Cudd_Regular(f);

    assert(!cuddIsConstant(F));

    /* Check computed table.  Separate entries are necessary for
    ** a node and its complement.  We should update the counter here. */
    if (st_lookup(table, f, &res)) {
	return(res);
    }

    /* Easy access to the constants for BDDs and ADDs. */
    one = DD_ONE(dd);
    lzero = Cudd_Not(one);
    azero = DD_ZERO(dd);

    /* Find cofactors and variable labeling the top node. */
    T = cuddT(F); E = cuddE(F);
    if (Cudd_IsComplement(f)) {
	T = Cudd_Not(T); E = Cudd_Not(E);
    }
    index = F->index;

    if (Cudd_IsConstant(T) && T != lzero && T != azero) {
	/* T is a non-zero constant.  If E is zero, then this node's index
	** is a one-literal clause.  Otherwise, if E is a non-zero
	** constant, there are no clauses for this node.  Finally,
	** if E is not constant, we recursively compute its clauses, and then
	** merge using the empty set for T. */
	if (E == lzero || E == azero) {
	    /* Create the clause (index + 0). */
	    res = tlcInfoAlloc();
	    if (res == NULL) return(NULL);
	    res->vars = ALLOC(DdHalfWord,4);
	    if (res->vars == NULL) {
		FREE(res);
		return(NULL);
	    }
	    res->phases = bitVectorAlloc(2);
	    if (res->phases == NULL) {
		FREE(res->vars);
		FREE(res);
		return(NULL);
	    }
	    res->vars[0] = index;
	    res->vars[1] = CUDD_MAXINDEX;
	    res->vars[2] = 0;
	    res->vars[3] = 0;
	    bitVectorSet(res->phases, 0, 0); /* positive phase */
	    bitVectorSet(res->phases, 1, 1); /* negative phase */
	} else if (Cudd_IsConstant(E)) {
	    /* If E is a non-zero constant, no clauses. */
	    res = emptyClauseSet();
	} else {
	    /* E is non-constant */
	    Tres = emptyClauseSet();
	    if (Tres == NULL) return(NULL);
	    Eres = ddFindTwoLiteralClausesRecur(dd, E, table);
	    if (Eres == NULL) {
		Cudd_tlcInfoFree(Tres);
		return(NULL);
	    }
	    res = computeClauses(Tres, Eres, index, dd->size);
	    Cudd_tlcInfoFree(Tres);
	}
    } else if (T == lzero || T == azero) {
	/* T is zero.  If E is a non-zero constant, then the
	** complement of this node's index is a one-literal clause.
	** Otherwise, if E is not constant, we recursively compute its
	** clauses, and then merge using the universal set for T. */
	if (Cudd_IsConstant(E)) { /* E cannot be zero here */
	    /* Create the clause (!index + 0). */
	    res = tlcInfoAlloc();
	    if (res == NULL) return(NULL);
	    res->vars = ALLOC(DdHalfWord,4);
	    if (res->vars == NULL) {
		FREE(res);
		return(NULL);
	    }
	    res->phases = bitVectorAlloc(2);
	    if (res->phases == NULL) {
		FREE(res->vars);
		FREE(res);
		return(NULL);
	    }
	    res->vars[0] = index;
	    res->vars[1] = CUDD_MAXINDEX;
	    res->vars[2] = 0;
	    res->vars[3] = 0;
	    bitVectorSet(res->phases, 0, 1); /* negative phase */
	    bitVectorSet(res->phases, 1, 1); /* negative phase */
	} else { /* E == non-constant */
	    Eres = ddFindTwoLiteralClausesRecur(dd, E, table);
	    if (Eres == NULL) return(NULL);
	    res = computeClausesWithUniverse(Eres, index, 1);
	}
    } else { /* T == non-const */
	Tres = ddFindTwoLiteralClausesRecur(dd, T, table);
	if (Tres == NULL) return(NULL);
	if (Cudd_IsConstant(E)) {
	    if (E == lzero || E == azero) {
		res = computeClausesWithUniverse(Tres, index, 0);
	    } else {
		Eres = emptyClauseSet();
		if (Eres == NULL) return(NULL);
		res = computeClauses(Tres, Eres, index, dd->size);
		Cudd_tlcInfoFree(Eres);
	    }
	} else {
	    Eres = ddFindTwoLiteralClausesRecur(dd, E, table);
	    if (Eres == NULL) return(NULL);
	    res = computeClauses(Tres, Eres, index, dd->size);
	}
    }

    /* Cache results. */
    if (st_add_direct(table, (char *)f, (char *)res) == ST_OUT_OF_MEM) {
	FREE(res);
	return(NULL);
    }
    return(res);

} /* end of ddFindTwoLiteralClausesRecur */


/**Function********************************************************************

  Synopsis    [Computes the two-literal clauses for a node.]

  Description [Computes the two-literal clauses for a node given the
  clauses for its children and the label of the node.  Returns a
  pointer to a TclInfo structure if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [computeClausesWithUniverse]

******************************************************************************/
static DdTlcInfo *
computeClauses(
  DdTlcInfo *Tres /* list of clauses for T child */,
  DdTlcInfo *Eres /* list of clauses for E child */,
  DdHalfWord label /* variable labeling the current node */,
  int size /* number of variables in the manager */)
{
    DdHalfWord *Tcv = Tres->vars; /* variables of clauses for the T child */
    BitVector *Tcp = Tres->phases; /* phases of clauses for the T child */
    DdHalfWord *Ecv = Eres->vars; /* variables of clauses for the E child */
    BitVector *Ecp = Eres->phases; /* phases of clauses for the E child */
    DdHalfWord *Vcv = NULL; /* pointer to variables of the clauses for v */
    BitVector *Vcp = NULL; /* pointer to phases of the clauses for v */
    DdTlcInfo *res = NULL; /* the set of clauses to be returned */
    int pt = 0; /* index in the list of clauses of T */
    int pe = 0; /* index in the list of clauses of E */
    int cv = 0; /* counter of the clauses for this node */
    TlClause *iclauses = NULL; /* list of inherited clauses */
    TlClause *tclauses = NULL; /* list of 1-literal clauses of T */
    TlClause *eclauses = NULL; /* list of 1-literal clauses of E */
    TlClause *nclauses = NULL; /* list of new (non-inherited) clauses */
    TlClause *lnclause = NULL; /* pointer to last new clause */
    TlClause *newclause; /* temporary pointer to new clauses */

    /* Initialize sets of one-literal clauses.  The one-literal clauses
    ** are stored redundantly.  These sets allow constant-time lookup, which
    ** we need when we check for implication of a two-literal clause by a
    ** one-literal clause.  The linked lists allow fast sequential
    ** processing. */
    bitVectorClear(Tolv, size);
    bitVectorClear(Tolp, size);
    bitVectorClear(Eolv, size);
    bitVectorClear(Eolp, size);

    /* Initialize result structure. */
    res = tlcInfoAlloc();
    if (res == NULL) goto cleanup;

    /* Scan the two input list.  Extract inherited two-literal clauses
    ** and set aside one-literal clauses from each list.  The incoming lists
    ** are sorted in the order defined by beforep.  The three linked list
    ** produced by this loop are sorted in the reverse order because we
    ** always append to the front of the lists.
    ** The inherited clauses are those clauses (both one- and two-literal)
    ** that are common to both children; and the two-literal clauses of
    ** one child that are implied by a one-literal clause of the other
    ** child. */
    while (!sentinelp(Tcv[pt], Tcv[pt+1]) || !sentinelp(Ecv[pe], Ecv[pe+1])) {
	if (equalp(Tcv[pt], bitVectorRead(Tcp, pt),
		   Tcv[pt+1], bitVectorRead(Tcp, pt+1),
		   Ecv[pe], bitVectorRead(Ecp, pe),
		   Ecv[pe+1], bitVectorRead(Ecp, pe+1))) {
	    /* Add clause to inherited list. */
	    newclause = ALLOC(TlClause,1);
	    if (newclause == NULL) goto cleanup;
	    newclause->v1 = Tcv[pt];
	    newclause->v2 = Tcv[pt+1];
	    newclause->p1 = bitVectorRead(Tcp, pt);
	    newclause->p2 = bitVectorRead(Tcp, pt+1);
	    newclause->next = iclauses;
	    iclauses = newclause;
	    pt += 2; pe += 2; cv++;
	} else if (beforep(Tcv[pt], bitVectorRead(Tcp, pt),
		   Tcv[pt+1], bitVectorRead(Tcp, pt+1),
		   Ecv[pe], bitVectorRead(Ecp, pe),
		   Ecv[pe+1], bitVectorRead(Ecp, pe+1))) {
	    if (oneliteralp(Tcv[pt+1])) {
		/* Add this one-literal clause to the T set. */
		newclause = ALLOC(TlClause,1);
		if (newclause == NULL) goto cleanup;
		newclause->v1 = Tcv[pt];
		newclause->v2 = CUDD_MAXINDEX;
		newclause->p1 = bitVectorRead(Tcp, pt);
		newclause->p2 = 1;
		newclause->next = tclauses;
		tclauses = newclause;
		bitVectorSet(Tolv, Tcv[pt], 1);
		bitVectorSet(Tolp, Tcv[pt], bitVectorRead(Tcp, pt));
	    } else {
		if (impliedp(Tcv[pt], bitVectorRead(Tcp, pt),
			     Tcv[pt+1], bitVectorRead(Tcp, pt+1),
			     Eolv, Eolp)) {
		    /* Add clause to inherited list. */
		    newclause = ALLOC(TlClause,1);
		    if (newclause == NULL) goto cleanup;
		    newclause->v1 = Tcv[pt];
		    newclause->v2 = Tcv[pt+1];
		    newclause->p1 = bitVectorRead(Tcp, pt);
		    newclause->p2 = bitVectorRead(Tcp, pt+1);
		    newclause->next = iclauses;
		    iclauses = newclause;
		    cv++;
		}
	    }
	    pt += 2;
	} else { /* !beforep() */
	    if (oneliteralp(Ecv[pe+1])) {
		/* Add this one-literal clause to the E set. */
		newclause = ALLOC(TlClause,1);
		if (newclause == NULL) goto cleanup;
		newclause->v1 = Ecv[pe];
		newclause->v2 = CUDD_MAXINDEX;
		newclause->p1 = bitVectorRead(Ecp, pe);
		newclause->p2 = 1;
		newclause->next = eclauses;
		eclauses = newclause;
		bitVectorSet(Eolv, Ecv[pe], 1);
		bitVectorSet(Eolp, Ecv[pe], bitVectorRead(Ecp, pe));
	    } else {
		if (impliedp(Ecv[pe], bitVectorRead(Ecp, pe),
			     Ecv[pe+1], bitVectorRead(Ecp, pe+1),
			     Tolv, Tolp)) {
		    /* Add clause to inherited list. */
		    newclause = ALLOC(TlClause,1);
		    if (newclause == NULL) goto cleanup;
		    newclause->v1 = Ecv[pe];
		    newclause->v2 = Ecv[pe+1];
		    newclause->p1 = bitVectorRead(Ecp, pe);
		    newclause->p2 = bitVectorRead(Ecp, pe+1);
		    newclause->next = iclauses;
		    iclauses = newclause;
		    cv++;
		}
	    }
	    pe += 2;
	}
    }

    /* Add one-literal clauses for the label variable to the front of
    ** the two lists. */
    newclause = ALLOC(TlClause,1);
    if (newclause == NULL) goto cleanup;
    newclause->v1 = label;
    newclause->v2 = CUDD_MAXINDEX;
    newclause->p1 = 0;
    newclause->p2 = 1;
    newclause->next = tclauses;
    tclauses = newclause;
    newclause = ALLOC(TlClause,1);
    if (newclause == NULL) goto cleanup;
    newclause->v1 = label;
    newclause->v2 = CUDD_MAXINDEX;
    newclause->p1 = 1;
    newclause->p2 = 1;
    newclause->next = eclauses;
    eclauses = newclause;

    /* Produce the non-inherited clauses.  We preserve the "reverse"
    ** order of the two input lists by appending to the end of the
    ** list.  In this way, iclauses and nclauses are consistent. */
    while (tclauses != NULL && eclauses != NULL) {
	if (beforep(eclauses->v1, eclauses->p1, eclauses->v2, eclauses->p2,
		    tclauses->v1, tclauses->p1, tclauses->v2, tclauses->p2)) {
	    TlClause *nextclause = tclauses->next;
	    TlClause *otherclauses = eclauses;
	    while (otherclauses != NULL) {
		if (tclauses->v1 != otherclauses->v1) {
		    newclause = ALLOC(TlClause,1);
		    if (newclause == NULL) goto cleanup;
		    newclause->v1 = tclauses->v1;
		    newclause->v2 = otherclauses->v1;
		    newclause->p1 = tclauses->p1;
		    newclause->p2 = otherclauses->p1;
		    newclause->next = NULL;
		    if (nclauses == NULL) {
			nclauses = newclause;
			lnclause = newclause;
		    } else {
			lnclause->next = newclause;
			lnclause = newclause;
		    }
		    cv++;
		}
		otherclauses = otherclauses->next;
	    }
	    FREE(tclauses);
	    tclauses = nextclause;
	} else {
	    TlClause *nextclause = eclauses->next;
	    TlClause *otherclauses = tclauses;
	    while (otherclauses != NULL) {
		if (eclauses->v1 != otherclauses->v1) {
		    newclause = ALLOC(TlClause,1);
		    if (newclause == NULL) goto cleanup;
		    newclause->v1 = eclauses->v1;
		    newclause->v2 = otherclauses->v1;
		    newclause->p1 = eclauses->p1;
		    newclause->p2 = otherclauses->p1;
		    newclause->next = NULL;
		    if (nclauses == NULL) {
			nclauses = newclause;
			lnclause = newclause;
		    } else {
			lnclause->next = newclause;
			lnclause = newclause;
		    }
		    cv++;
		}
		otherclauses = otherclauses->next;
	    }
	    FREE(eclauses);
	    eclauses = nextclause;
	}
    }
    while (tclauses != NULL) {
	TlClause *nextclause = tclauses->next;
	FREE(tclauses);
	tclauses = nextclause;
    }
    while (eclauses != NULL) {
	TlClause *nextclause = eclauses->next;
	FREE(eclauses);
	eclauses = nextclause;
    }

    /* Merge inherited and non-inherited clauses.  Now that we know the
    ** total number, we allocate the arrays, and we fill them bottom-up
    ** to restore the proper ordering. */
    Vcv = ALLOC(DdHalfWord, 2*(cv+1));
    if (Vcv == NULL) goto cleanup;
    if (cv > 0) {
	Vcp = bitVectorAlloc(2*cv);
	if (Vcp == NULL) goto cleanup;
    } else {
	Vcp = NULL;
    }
    res->vars = Vcv;
    res->phases = Vcp;
    /* Add sentinel. */
    Vcv[2*cv] = 0;
    Vcv[2*cv+1] = 0;
    while (iclauses != NULL || nclauses != NULL) {
	TlClause *nextclause;
	cv--;
	if (nclauses == NULL || (iclauses != NULL &&
	    beforep(nclauses->v1, nclauses->p1, nclauses->v2, nclauses->p2,
		    iclauses->v1, iclauses->p1, iclauses->v2, iclauses->p2))) {
	    Vcv[2*cv] = iclauses->v1;
	    Vcv[2*cv+1] = iclauses->v2;
	    bitVectorSet(Vcp, 2*cv, iclauses->p1);
	    bitVectorSet(Vcp, 2*cv+1, iclauses->p2);
	    nextclause = iclauses->next;
	    FREE(iclauses);
	    iclauses = nextclause;
	} else {
	    Vcv[2*cv] = nclauses->v1;
	    Vcv[2*cv+1] = nclauses->v2;
	    bitVectorSet(Vcp, 2*cv, nclauses->p1);
	    bitVectorSet(Vcp, 2*cv+1, nclauses->p2);
	    nextclause = nclauses->next;
	    FREE(nclauses);
	    nclauses = nextclause;
	}
    }
    assert(cv == 0);

    return(res);

 cleanup:
    if (res != NULL) Cudd_tlcInfoFree(res);
    while (iclauses != NULL) {
	TlClause *nextclause = iclauses->next;
	FREE(iclauses);
	iclauses = nextclause;
    }
    while (nclauses != NULL) {
	TlClause *nextclause = nclauses->next;
	FREE(nclauses);
	nclauses = nextclause;
    }
    while (tclauses != NULL) {
	TlClause *nextclause = tclauses->next;
	FREE(tclauses);
	tclauses = nextclause;
    }
    while (eclauses != NULL) {
	TlClause *nextclause = eclauses->next;
	FREE(eclauses);
	eclauses = nextclause;
    }

    return(NULL);

} /* end of computeClauses */


/**Function********************************************************************

  Synopsis    [Computes the two-literal clauses for a node.]

  Description [Computes the two-literal clauses for a node with a zero
  child, given the clauses for its other child and the label of the
  node.  Returns a pointer to a TclInfo structure if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [computeClauses]

******************************************************************************/
static DdTlcInfo *
computeClausesWithUniverse(
  DdTlcInfo *Cres /* list of clauses for child */,
  DdHalfWord label /* variable labeling the current node */,
  short phase /* 0 if E child is zero; 1 if T child is zero */)
{
    DdHalfWord *Ccv = Cres->vars; /* variables of clauses for child */
    BitVector *Ccp = Cres->phases; /* phases of clauses for child */
    DdHalfWord *Vcv = NULL; /* pointer to the variables of the clauses for v */
    BitVector *Vcp = NULL; /* pointer to the phases of the clauses for v */
    DdTlcInfo *res = NULL; /* the set of clauses to be returned */
    int i;

    /* Initialize result. */
    res = tlcInfoAlloc();
    if (res == NULL) goto cleanup;
    /* Count entries for new list and allocate accordingly. */
    for (i = 0; !sentinelp(Ccv[i], Ccv[i+1]); i += 2);
    /* At this point, i is twice the number of clauses in the child's
    ** list.  We need four more entries for this node: 2 for the one-literal
    ** clause for the label, and 2 for the sentinel. */
    Vcv = ALLOC(DdHalfWord,i+4);
    if (Vcv == NULL) goto cleanup;
    Vcp = bitVectorAlloc(i+4);
    if (Vcp == NULL) goto cleanup;
    res->vars = Vcv;
    res->phases = Vcp;
    /* Copy old list into new. */
    for (i = 0; !sentinelp(Ccv[i], Ccv[i+1]); i += 2) {
	Vcv[i] = Ccv[i];
	Vcv[i+1] = Ccv[i+1];
	bitVectorSet(Vcp, i, bitVectorRead(Ccp, i));
	bitVectorSet(Vcp, i+1, bitVectorRead(Ccp, i+1));
    }
    /* Add clause corresponding to label. */
    Vcv[i] = label;
    bitVectorSet(Vcp, i, phase);
    i++;
    Vcv[i] = CUDD_MAXINDEX;
    bitVectorSet(Vcp, i, 1);
    i++;
    /* Add sentinel. */
    Vcv[i] = 0;
    Vcv[i+1] = 0;
    bitVectorSet(Vcp, i, 0);
    bitVectorSet(Vcp, i+1, 0);

    return(res);

 cleanup:
    /* Vcp is guaranteed to be NULL here.  Hence, we do not try to free it. */
    if (Vcv != NULL) FREE(Vcv);
    if (res != NULL) Cudd_tlcInfoFree(res);

    return(NULL);

} /* end of computeClausesWithUniverse */


/**Function********************************************************************

  Synopsis    [Returns an enpty set of clauses.]

  Description [Returns a pointer to an empty set of clauses if
  successful; NULL otherwise.  No bit vector for the phases is
  allocated.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdTlcInfo *
emptyClauseSet(void)
{
    DdTlcInfo *eset;

    eset = ALLOC(DdTlcInfo,1);
    if (eset == NULL) return(NULL);
    eset->vars = ALLOC(DdHalfWord,2);
    if (eset->vars == NULL) {
	FREE(eset);
	return(NULL);
    }
    /* Sentinel */
    eset->vars[0] = 0;
    eset->vars[1] = 0;
    eset->phases = NULL; /* does not matter */
    eset->cnt = 0;
    return(eset);

} /* end of emptyClauseSet */


/**Function********************************************************************

  Synopsis    [Returns true iff the argument is the sentinel clause.]

  Description [Returns true iff the argument is the sentinel clause.
  A sentinel clause has both variables equal to 0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
sentinelp(
  DdHalfWord var1,
  DdHalfWord var2)
{
    return(var1 == 0 && var2 == 0);

} /* end of sentinelp */


/**Function********************************************************************

  Synopsis    [Returns true iff the two arguments are identical clauses.]

  Description [Returns true iff the two arguments are identical
  clauses.  Since literals are sorted, we only need to compare
  literals in the same position.]

  SideEffects [None]

  SeeAlso     [beforep]

******************************************************************************/
static int
equalp(
  DdHalfWord var1a,
  short phase1a,
  DdHalfWord var1b,
  short phase1b,
  DdHalfWord var2a,
  short phase2a,
  DdHalfWord var2b,
  short phase2b)
{
    return(var1a == var2a && phase1a == phase2a &&
	   var1b == var2b && phase1b == phase2b);

} /* end of equalp */


/**Function********************************************************************

  Synopsis    [Returns true iff the first argument precedes the second in
  the clause order.]

  Description [Returns true iff the first argument precedes the second
  in the clause order.  A clause precedes another if its first lieral
  precedes the first literal of the other, or if the first literals
  are the same, and its second literal precedes the second literal of
  the other clause.  A literal precedes another if it has a higher
  index, of if it has the same index, but it has lower phase.  Phase 0
  is the positive phase, and it is lower than Phase 1 (negative
  phase).]

  SideEffects [None]

  SeeAlso     [equalp]

******************************************************************************/
static int
beforep(
  DdHalfWord var1a,
  short phase1a,
  DdHalfWord var1b,
  short phase1b,
  DdHalfWord var2a,
  short phase2a,
  DdHalfWord var2b,
  short phase2b)
{
    return(var1a > var2a || (var1a == var2a &&
	   (phase1a < phase2a || (phase1a == phase2a &&
	    (var1b > var2b || (var1b == var2b && phase1b < phase2b))))));

} /* end of beforep */


/**Function********************************************************************

  Synopsis    [Returns true iff the argument is a one-literal clause.]

  Description [Returns true iff the argument is a one-literal clause.
  A one-litaral clause has the constant FALSE as second literal.
  Since the constant TRUE is never used, it is sufficient to test for
  a constant.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
oneliteralp(
  DdHalfWord var)
{
    return(var == CUDD_MAXINDEX);

} /* end of oneliteralp */


/**Function********************************************************************

  Synopsis [Returns true iff either literal of a clause is in a set of
  literals.]

  Description [Returns true iff either literal of a clause is in a set
  of literals.  The first four arguments specify the clause.  The
  remaining two arguments specify the literal set.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
impliedp(
  DdHalfWord var1,
  short phase1,
  DdHalfWord var2,
  short phase2,
  BitVector *olv,
  BitVector *olp)
{
    return((bitVectorRead(olv, var1) &&
	    bitVectorRead(olp, var1) == phase1) ||
	   (bitVectorRead(olv, var2) &&
	    bitVectorRead(olp, var2) == phase2));

} /* end of impliedp */


/**Function********************************************************************

  Synopsis    [Allocates a bit vector.]

  Description [Allocates a bit vector.  The parameter size gives the
  number of bits.  This procedure allocates enough long's to hold the
  specified number of bits.  Returns a pointer to the allocated vector
  if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [bitVectorClear bitVectorFree]

******************************************************************************/
static BitVector *
bitVectorAlloc(
  int size)
{
    int allocSize;
    BitVector *vector;

    /* Find out how many long's we need.
    ** There are sizeof(long) * 8 bits in a long.
    ** The ceiling of the ratio of two integers m and n is given
    ** by ((n-1)/m)+1.  Putting all this together, we get... */
    allocSize = ((size - 1) / (sizeof(BitVector) * 8)) + 1;
    vector = ALLOC(BitVector, allocSize);
    if (vector == NULL) return(NULL);
    /* Clear the whole array. */
    (void) memset(vector, 0, allocSize * sizeof(BitVector));
    return(vector);

} /* end of bitVectorAlloc */


/**Function********************************************************************

  Synopsis    [Clears a bit vector.]

  Description [Clears a bit vector.  The parameter size gives the
  number of bits.]

  SideEffects [None]

  SeeAlso     [bitVectorAlloc]

******************************************************************************/
DD_INLINE
static void
bitVectorClear(
  BitVector *vector,
  int size)
{
    int allocSize;

    /* Find out how many long's we need.
    ** There are sizeof(long) * 8 bits in a long.
    ** The ceiling of the ratio of two integers m and n is given
    ** by ((n-1)/m)+1.  Putting all this together, we get... */
    allocSize = ((size - 1) / (sizeof(BitVector) * 8)) + 1;
    /* Clear the whole array. */
    (void) memset(vector, 0, allocSize * sizeof(BitVector));
    return;

} /* end of bitVectorClear */


/**Function********************************************************************

  Synopsis    [Frees a bit vector.]

  Description [Frees a bit vector.]

  SideEffects [None]

  SeeAlso     [bitVectorAlloc]

******************************************************************************/
static void
bitVectorFree(
  BitVector *vector)
{
    FREE(vector);

} /* end of bitVectorFree */


/**Function********************************************************************

  Synopsis    [Returns the i-th entry of a bit vector.]

  Description [Returns the i-th entry of a bit vector.]

  SideEffects [None]

  SeeAlso     [bitVectorSet]

******************************************************************************/
DD_INLINE
static short
bitVectorRead(
  BitVector *vector,
  int i)
{
    int word, bit;
    short result;

    if (vector == NULL) return((short) 0);

    word = i >> LOGBPL;
    bit = i & (BPL - 1);
    result = (short) ((vector[word] >> bit) & 1L);
    return(result);

} /* end of bitVectorRead */


/**Function********************************************************************

  Synopsis    [Sets the i-th entry of a bit vector to a value.]

  Description [Sets the i-th entry of a bit vector to a value.]

  SideEffects [None]

  SeeAlso     [bitVectorRead]

******************************************************************************/
DD_INLINE
static void
bitVectorSet(
  BitVector * vector,
  int i,
  short val)
{
    int word, bit;

    word = i >> LOGBPL;
    bit = i & (BPL - 1);
    vector[word] &= ~(1L << bit);
    vector[word] |= (((long) val) << bit);

} /* end of bitVectorSet */


/**Function********************************************************************

  Synopsis    [Allocates a DdTlcInfo Structure.]

  Description [Returns a pointer to a DdTlcInfo Structure if successful;
  NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_tlcInfoFree]

******************************************************************************/
static DdTlcInfo *
tlcInfoAlloc(void)
{
    DdTlcInfo *res = ALLOC(DdTlcInfo,1);
    if (res == NULL) return(NULL);
    res->vars = NULL;
    res->phases = NULL;
    res->cnt = 0;
    return(res);

} /* end of tlcInfoAlloc */
