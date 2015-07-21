/**CFile***********************************************************************

  FileName    [cuddBddIte.c]

  PackageName [cudd]

  Synopsis    [BDD ITE function and satellites.]

  Description [External procedures included in this module:
		<ul>
                <li> Cudd_bddIte()
                <li> Cudd_bddIteLimit()
       	        <li> Cudd_bddIteConstant()
		<li> Cudd_bddIntersect()
		<li> Cudd_bddAnd()
		<li> Cudd_bddAndLimit()
		<li> Cudd_bddOr()
		<li> Cudd_bddOrLimit()
		<li> Cudd_bddNand()
		<li> Cudd_bddNor()
		<li> Cudd_bddXor()
		<li> Cudd_bddXnor()
		<li> Cudd_bddXnorLimit()
		<li> Cudd_bddLeq()
		</ul>
       Internal procedures included in this module:
		<ul>
		<li> cuddBddIteRecur()
		<li> cuddBddIntersectRecur()
		<li> cuddBddAndRecur()
		<li> cuddBddXorRecur()
		</ul>
       Static procedures included in this module:
		<ul>
       	        <li> bddVarToConst()
       	        <li> bddVarToCanonical()
       	        <li> bddVarToCanonicalSimple()
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
static char rcsid[] DD_UNUSED = "$Id: cuddBddIte.c,v 1.26 2012/02/05 01:07:18 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void bddVarToConst (DdNode *f, DdNode **gp, DdNode **hp, DdNode *one);
static int bddVarToCanonical (DdManager *dd, DdNode **fp, DdNode **gp, DdNode **hp, unsigned int *topfp, unsigned int *topgp, unsigned int *tophp);
static int bddVarToCanonicalSimple (DdManager *dd, DdNode **fp, DdNode **gp, DdNode **hp, unsigned int *topfp, unsigned int *topgp, unsigned int *tophp);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements ITE(f,g,h).]

  Description [Implements ITE(f,g,h). Returns a pointer to the
  resulting BDD if successful; NULL if the intermediate result blows
  up.]

  SideEffects [None]

  SeeAlso     [Cudd_addIte Cudd_bddIteConstant Cudd_bddIntersect]

******************************************************************************/
DdNode *
Cudd_bddIte(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddIteRecur(dd,f,g,h);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddIte */


/**Function********************************************************************

  Synopsis    [Implements ITE(f,g,h).  Returns
  NULL if too many nodes are required.]

  Description [Implements ITE(f,g,h).  Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up or more new nodes than <code>limit</code> are
  required.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte]

******************************************************************************/
DdNode *
Cudd_bddIteLimit(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h,
  unsigned int limit)
{
    DdNode *res;
    unsigned int saveLimit = dd->maxLive;

    dd->maxLive = (dd->keys - dd->dead) + (dd->keysZ - dd->deadZ) + limit;
    do {
	dd->reordered = 0;
	res = cuddBddIteRecur(dd,f,g,h);
    } while (dd->reordered == 1);
    dd->maxLive = saveLimit;
    return(res);

} /* end of Cudd_bddIteLimit */


/**Function********************************************************************

  Synopsis    [Implements ITEconstant(f,g,h).]

  Description [Implements ITEconstant(f,g,h). Returns a pointer to the
  resulting BDD (which may or may not be constant) or DD_NON_CONSTANT.
  No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_bddIntersect Cudd_bddLeq Cudd_addIteConstant]

******************************************************************************/
DdNode *
Cudd_bddIteConstant(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode	 *r, *Fv, *Fnv, *Gv, *Gnv, *H, *Hv, *Hnv, *t, *e;
    DdNode	 *one = DD_ONE(dd);
    DdNode	 *zero = Cudd_Not(one);
    int		 comple;
    unsigned int topf, topg, toph, v;

    statLine(dd);
    /* Trivial cases. */
    if (f == one) 			/* ITE(1,G,H) => G */
	return(g);
    
    if (f == zero)			/* ITE(0,G,H) => H */
	return(h);
    
    /* f now not a constant. */
    bddVarToConst(f, &g, &h, one);	/* possibly convert g or h */
					/* to constants */

    if (g == h) 			/* ITE(F,G,G) => G */
	return(g);

    if (Cudd_IsConstant(g) && Cudd_IsConstant(h)) 
	return(DD_NON_CONSTANT);	/* ITE(F,1,0) or ITE(F,0,1) */
					/* => DD_NON_CONSTANT */
    
    if (g == Cudd_Not(h))
	return(DD_NON_CONSTANT);	/* ITE(F,G,G') => DD_NON_CONSTANT */
					/* if F != G and F != G' */
    
    comple = bddVarToCanonical(dd, &f, &g, &h, &topf, &topg, &toph);

    /* Cache lookup. */
    r = cuddConstantLookup(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h);
    if (r != NULL) {
	return(Cudd_NotCond(r,comple && r != DD_NON_CONSTANT));
    }

    v = ddMin(topg, toph);

    /* ITE(F,G,H) = (v,G,H) (non constant) if F = (v,1,0), v < top(G,H). */
    if (topf < v && cuddT(f) == one && cuddE(f) == zero) {
	return(DD_NON_CONSTANT);
    }

    /* Compute cofactors. */
    if (topf <= v) {
	v = ddMin(topf, v);		/* v = top_var(F,G,H) */
	Fv = cuddT(f); Fnv = cuddE(f);
    } else {
	Fv = Fnv = f;
    }

    if (topg == v) {
	Gv = cuddT(g); Gnv = cuddE(g);
    } else {
	Gv = Gnv = g;
    }

    if (toph == v) {
	H = Cudd_Regular(h);
	Hv = cuddT(H); Hnv = cuddE(H);
	if (Cudd_IsComplement(h)) {
	    Hv = Cudd_Not(Hv);
	    Hnv = Cudd_Not(Hnv);
	}
    } else {
	Hv = Hnv = h;
    }

    /* Recursion. */
    t = Cudd_bddIteConstant(dd, Fv, Gv, Hv);
    if (t == DD_NON_CONSTANT || !Cudd_IsConstant(t)) {
	cuddCacheInsert(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
	return(DD_NON_CONSTANT);
    }
    e = Cudd_bddIteConstant(dd, Fnv, Gnv, Hnv);
    if (e == DD_NON_CONSTANT || !Cudd_IsConstant(e) || t != e) {
	cuddCacheInsert(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
	return(DD_NON_CONSTANT);
    }
    cuddCacheInsert(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h, t);
    return(Cudd_NotCond(t,comple));

} /* end of Cudd_bddIteConstant */


/**Function********************************************************************

  Synopsis    [Returns a function included in the intersection of f and g.]

  Description [Computes a function included in the intersection of f and
  g. (That is, a witness that the intersection is not empty.)
  Cudd_bddIntersect tries to build as few new nodes as possible. If the
  only result of interest is whether f and g intersect,
  Cudd_bddLeq should be used instead.]

  SideEffects [None]

  SeeAlso     [Cudd_bddLeq Cudd_bddIteConstant]

******************************************************************************/
DdNode *
Cudd_bddIntersect(
  DdManager * dd /* manager */,
  DdNode * f /* first operand */,
  DdNode * g /* second operand */)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddIntersectRecur(dd,f,g);
    } while (dd->reordered == 1);

    return(res);

} /* end of Cudd_bddIntersect */


/**Function********************************************************************

  Synopsis    [Computes the conjunction of two BDDs f and g.]

  Description [Computes the conjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAndAbstract Cudd_bddIntersect
  Cudd_bddOr Cudd_bddNand Cudd_bddNor Cudd_bddXor Cudd_bddXnor]

******************************************************************************/
DdNode *
Cudd_bddAnd(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddAndRecur(dd,f,g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddAnd */


/**Function********************************************************************

  Synopsis    [Computes the conjunction of two BDDs f and g.  Returns
  NULL if too many nodes are required.]

  Description [Computes the conjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up or more new nodes than <code>limit</code> are
  required.]

  SideEffects [None]

  SeeAlso     [Cudd_bddAnd]

******************************************************************************/
DdNode *
Cudd_bddAndLimit(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  unsigned int limit)
{
    DdNode *res;
    unsigned int saveLimit = dd->maxLive;

    dd->maxLive = (dd->keys - dd->dead) + (dd->keysZ - dd->deadZ) + limit;
    do {
	dd->reordered = 0;
	res = cuddBddAndRecur(dd,f,g);
    } while (dd->reordered == 1);
    dd->maxLive = saveLimit;
    return(res);

} /* end of Cudd_bddAndLimit */


/**Function********************************************************************

  Synopsis    [Computes the disjunction of two BDDs f and g.]

  Description [Computes the disjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddNand Cudd_bddNor
  Cudd_bddXor Cudd_bddXnor]

******************************************************************************/
DdNode *
Cudd_bddOr(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddAndRecur(dd,Cudd_Not(f),Cudd_Not(g));
    } while (dd->reordered == 1);
    res = Cudd_NotCond(res,res != NULL);
    return(res);

} /* end of Cudd_bddOr */


/**Function********************************************************************

  Synopsis    [Computes the disjunction of two BDDs f and g.  Returns
  NULL if too many nodes are required.]

  Description [Computes the disjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up or more new nodes than <code>limit</code> are
  required.]

  SideEffects [None]

  SeeAlso     [Cudd_bddOr]

******************************************************************************/
DdNode *
Cudd_bddOrLimit(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  unsigned int limit)
{
    DdNode *res;
    unsigned int saveLimit = dd->maxLive;

    dd->maxLive = (dd->keys - dd->dead) + (dd->keysZ - dd->deadZ) + limit;
    do {
	dd->reordered = 0;
	res = cuddBddAndRecur(dd,Cudd_Not(f),Cudd_Not(g));
    } while (dd->reordered == 1);
    dd->maxLive = saveLimit;
    res = Cudd_NotCond(res,res != NULL);
    return(res);

} /* end of Cudd_bddOrLimit */


/**Function********************************************************************

  Synopsis    [Computes the NAND of two BDDs f and g.]

  Description [Computes the NAND of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddOr Cudd_bddNor
  Cudd_bddXor Cudd_bddXnor]

******************************************************************************/
DdNode *
Cudd_bddNand(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddAndRecur(dd,f,g);
    } while (dd->reordered == 1);
    res = Cudd_NotCond(res,res != NULL);
    return(res);

} /* end of Cudd_bddNand */


/**Function********************************************************************

  Synopsis    [Computes the NOR of two BDDs f and g.]

  Description [Computes the NOR of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddOr Cudd_bddNand
  Cudd_bddXor Cudd_bddXnor]

******************************************************************************/
DdNode *
Cudd_bddNor(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddAndRecur(dd,Cudd_Not(f),Cudd_Not(g));
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddNor */


/**Function********************************************************************

  Synopsis    [Computes the exclusive OR of two BDDs f and g.]

  Description [Computes the exclusive OR of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddOr
  Cudd_bddNand Cudd_bddNor Cudd_bddXnor]

******************************************************************************/
DdNode *
Cudd_bddXor(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddXorRecur(dd,f,g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddXor */


/**Function********************************************************************

  Synopsis    [Computes the exclusive NOR of two BDDs f and g.]

  Description [Computes the exclusive NOR of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddOr
  Cudd_bddNand Cudd_bddNor Cudd_bddXor]

******************************************************************************/
DdNode *
Cudd_bddXnor(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddXorRecur(dd,f,Cudd_Not(g));
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddXnor */


/**Function********************************************************************

  Synopsis    [Computes the exclusive NOR of two BDDs f and g.  Returns
  NULL if too many nodes are required.]

  Description [Computes the exclusive NOR of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up or more new nodes than <code>limit</code> are
  required.]

  SideEffects [None]

  SeeAlso     [Cudd_bddXnor]

******************************************************************************/
DdNode *
Cudd_bddXnorLimit(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  unsigned int limit)
{
    DdNode *res;
    unsigned int saveLimit = dd->maxLive;

    dd->maxLive = (dd->keys - dd->dead) + (dd->keysZ - dd->deadZ) + limit;
    do {
	dd->reordered = 0;
	res = cuddBddXorRecur(dd,f,Cudd_Not(g));
    } while (dd->reordered == 1);
    dd->maxLive = saveLimit;
    return(res);

} /* end of Cudd_bddXnorLimit */


/**Function********************************************************************

  Synopsis    [Determines whether f is less than or equal to g.]

  Description [Returns 1 if f is less than or equal to g; 0 otherwise.
  No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIteConstant Cudd_addEvalConst]

******************************************************************************/
int
Cudd_bddLeq(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *one, *zero, *tmp, *F, *fv, *fvn, *gv, *gvn;
    unsigned int topf, topg, res;

    statLine(dd);
    /* Terminal cases and normalization. */
    if (f == g) return(1);

    if (Cudd_IsComplement(g)) {
	/* Special case: if f is regular and g is complemented,
	** f(1,...,1) = 1 > 0 = g(1,...,1).
	*/
	if (!Cudd_IsComplement(f)) return(0);
	/* Both are complemented: Swap and complement because
	** f <= g <=> g' <= f' and we want the second argument to be regular.
	*/
	tmp = g;
	g = Cudd_Not(f);
	f = Cudd_Not(tmp);
    } else if (Cudd_IsComplement(f) && g < f) {
	tmp = g;
	g = Cudd_Not(f);
	f = Cudd_Not(tmp);
    }

    /* Now g is regular and, if f is not regular, f < g. */
    one = DD_ONE(dd);
    if (g == one) return(1);	/* no need to test against zero */
    if (f == one) return(0);	/* since at this point g != one */
    if (Cudd_Not(f) == g) return(0); /* because neither is constant */
    zero = Cudd_Not(one);
    if (f == zero) return(1);

    /* Here neither f nor g is constant. */

    /* Check cache. */
    tmp = cuddCacheLookup2(dd,(DD_CTFP)Cudd_bddLeq,f,g);
    if (tmp != NULL) {
	return(tmp == one);
    }

    /* Compute cofactors. */
    F = Cudd_Regular(f);
    topf = dd->perm[F->index];
    topg = dd->perm[g->index];
    if (topf <= topg) {
	fv = cuddT(F); fvn = cuddE(F);
	if (f != F) {
	    fv = Cudd_Not(fv);
	    fvn = Cudd_Not(fvn);
	}
    } else {
	fv = fvn = f;
    }
    if (topg <= topf) {
	gv = cuddT(g); gvn = cuddE(g);
    } else {
	gv = gvn = g;
    }

    /* Recursive calls. Since we want to maximize the probability of
    ** the special case f(1,...,1) > g(1,...,1), we consider the negative
    ** cofactors first. Indeed, the complementation parity of the positive
    ** cofactors is the same as the one of the parent functions.
    */
    res = Cudd_bddLeq(dd,fvn,gvn) && Cudd_bddLeq(dd,fv,gv);

    /* Store result in cache and return. */
    cuddCacheInsert2(dd,(DD_CTFP)Cudd_bddLeq,f,g,(res ? one : zero));
    return(res);

} /* end of Cudd_bddLeq */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_bddIte.]

  Description [Implements the recursive step of Cudd_bddIte. Returns a
  pointer to the resulting BDD. NULL if the intermediate result blows
  up or if reordering occurs.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddBddIteRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode	 *one, *zero, *res;
    DdNode	 *r, *Fv, *Fnv, *Gv, *Gnv, *H, *Hv, *Hnv, *t, *e;
    unsigned int topf, topg, toph, v;
    int		 index;
    int		 comple;

    statLine(dd);
    /* Terminal cases. */

    /* One variable cases. */
    if (f == (one = DD_ONE(dd))) 	/* ITE(1,G,H) = G */
	return(g);
    
    if (f == (zero = Cudd_Not(one))) 	/* ITE(0,G,H) = H */
	return(h);
    
    /* From now on, f is known not to be a constant. */
    if (g == one || f == g) {	/* ITE(F,F,H) = ITE(F,1,H) = F + H */
	if (h == zero) {	/* ITE(F,1,0) = F */
	    return(f);
	} else {
	    res = cuddBddAndRecur(dd,Cudd_Not(f),Cudd_Not(h));
	    return(Cudd_NotCond(res,res != NULL));
	}
    } else if (g == zero || f == Cudd_Not(g)) { /* ITE(F,!F,H) = ITE(F,0,H) = !F * H */
	if (h == one) {		/* ITE(F,0,1) = !F */
	    return(Cudd_Not(f));
	} else {
	    res = cuddBddAndRecur(dd,Cudd_Not(f),h);
	    return(res);
	}
    }
    if (h == zero || f == h) {    /* ITE(F,G,F) = ITE(F,G,0) = F * G */
	res = cuddBddAndRecur(dd,f,g);
	return(res);
    } else if (h == one || f == Cudd_Not(h)) { /* ITE(F,G,!F) = ITE(F,G,1) = !F + G */
	res = cuddBddAndRecur(dd,f,Cudd_Not(g));
	return(Cudd_NotCond(res,res != NULL));
    }

    /* Check remaining one variable case. */
    if (g == h) { 		/* ITE(F,G,G) = G */
	return(g);
    } else if (g == Cudd_Not(h)) { /* ITE(F,G,!G) = F <-> G */
	res = cuddBddXorRecur(dd,f,h);
	return(res);
    }
    
    /* From here, there are no constants. */
    comple = bddVarToCanonicalSimple(dd, &f, &g, &h, &topf, &topg, &toph);

    /* f & g are now regular pointers */

    v = ddMin(topg, toph);

    /* A shortcut: ITE(F,G,H) = (v,G,H) if F = (v,1,0), v < top(G,H). */
    if (topf < v && cuddT(f) == one && cuddE(f) == zero) {
	r = cuddUniqueInter(dd, (int) f->index, g, h);
	return(Cudd_NotCond(r,comple && r != NULL));
    }

    /* Check cache. */
    r = cuddCacheLookup(dd, DD_BDD_ITE_TAG, f, g, h);
    if (r != NULL) {
	return(Cudd_NotCond(r,comple));
    }

    /* Compute cofactors. */
    if (topf <= v) {
	v = ddMin(topf, v);	/* v = top_var(F,G,H) */
	index = f->index;
	Fv = cuddT(f); Fnv = cuddE(f);
    } else {
	Fv = Fnv = f;
    }
    if (topg == v) {
	index = g->index;
	Gv = cuddT(g); Gnv = cuddE(g);
    } else {
	Gv = Gnv = g;
    }
    if (toph == v) {
	H = Cudd_Regular(h);
	index = H->index;
	Hv = cuddT(H); Hnv = cuddE(H);
	if (Cudd_IsComplement(h)) {
	    Hv = Cudd_Not(Hv);
	    Hnv = Cudd_Not(Hnv);
	}
    } else {
	Hv = Hnv = h;
    }

    /* Recursive step. */
    t = cuddBddIteRecur(dd,Fv,Gv,Hv);
    if (t == NULL) return(NULL);
    cuddRef(t);

    e = cuddBddIteRecur(dd,Fnv,Gnv,Hnv);
    if (e == NULL) {
	Cudd_IterDerefBdd(dd,t);
	return(NULL);
    }
    cuddRef(e);

    r = (t == e) ? t : cuddUniqueInter(dd,index,t,e);
    if (r == NULL) {
	Cudd_IterDerefBdd(dd,t);
	Cudd_IterDerefBdd(dd,e);
	return(NULL);
    }
    cuddDeref(t);
    cuddDeref(e);

    cuddCacheInsert(dd, DD_BDD_ITE_TAG, f, g, h, r);
    return(Cudd_NotCond(r,comple));

} /* end of cuddBddIteRecur */


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_bddIntersect.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_bddIntersect]

******************************************************************************/
DdNode *
cuddBddIntersectRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;
    DdNode *F, *G, *t, *e;
    DdNode *fv, *fnv, *gv, *gnv;
    DdNode *one, *zero;
    unsigned int index, topf, topg;

    statLine(dd);
    one = DD_ONE(dd);
    zero = Cudd_Not(one);

    /* Terminal cases. */
    if (f == zero || g == zero || f == Cudd_Not(g)) return(zero);
    if (f == g || g == one) return(f);
    if (f == one) return(g);

    /* At this point f and g are not constant. */
    if (f > g) { DdNode *tmp = f; f = g; g = tmp; }
    res = cuddCacheLookup2(dd,Cudd_bddIntersect,f,g);
    if (res != NULL) return(res);

    /* Find splitting variable. Here we can skip the use of cuddI,
    ** because the operands are known to be non-constant.
    */
    F = Cudd_Regular(f);
    topf = dd->perm[F->index];
    G = Cudd_Regular(g);
    topg = dd->perm[G->index];

    /* Compute cofactors. */
    if (topf <= topg) {
	index = F->index;
	fv = cuddT(F);
	fnv = cuddE(F);
	if (Cudd_IsComplement(f)) {
	    fv = Cudd_Not(fv);
	    fnv = Cudd_Not(fnv);
	}
    } else {
	index = G->index;
	fv = fnv = f;
    }

    if (topg <= topf) {
	gv = cuddT(G);
	gnv = cuddE(G);
	if (Cudd_IsComplement(g)) {
	    gv = Cudd_Not(gv);
	    gnv = Cudd_Not(gnv);
	}
    } else {
	gv = gnv = g;
    }

    /* Compute partial results. */
    t = cuddBddIntersectRecur(dd,fv,gv);
    if (t == NULL) return(NULL);
    cuddRef(t);
    if (t != zero) {
	e = zero;
    } else {
	e = cuddBddIntersectRecur(dd,fnv,gnv);
	if (e == NULL) {
	    Cudd_IterDerefBdd(dd, t);
	    return(NULL);
	}
    }
    cuddRef(e);

    if (t == e) { /* both equal zero */
	res = t;
    } else if (Cudd_IsComplement(t)) {
	res = cuddUniqueInter(dd,(int)index,Cudd_Not(t),Cudd_Not(e));
	if (res == NULL) {
	    Cudd_IterDerefBdd(dd, t);
	    Cudd_IterDerefBdd(dd, e);
	    return(NULL);
	}
	res = Cudd_Not(res);
    } else {
	res = cuddUniqueInter(dd,(int)index,t,e);
	if (res == NULL) {
	    Cudd_IterDerefBdd(dd, t);
	    Cudd_IterDerefBdd(dd, e);
	    return(NULL);
	}
    }
    cuddDeref(e);
    cuddDeref(t);

    cuddCacheInsert2(dd,Cudd_bddIntersect,f,g,res);

    return(res);

} /* end of cuddBddIntersectRecur */


/**Function********************************************************************

  Synopsis [Implements the recursive step of Cudd_bddAnd.]

  Description [Implements the recursive step of Cudd_bddAnd by taking
  the conjunction of two BDDs.  Returns a pointer to the result is
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddAnd]

******************************************************************************/
DdNode *
cuddBddAndRecur(
  DdManager * manager,
  DdNode * f,
  DdNode * g)
{
    DdNode *F, *fv, *fnv, *G, *gv, *gnv;
    DdNode *one, *r, *t, *e;
    unsigned int topf, topg, index;

    statLine(manager);
    one = DD_ONE(manager);

    /* Terminal cases. */
    F = Cudd_Regular(f);
    G = Cudd_Regular(g);
    if (F == G) {
	if (f == g) return(f);
	else return(Cudd_Not(one));
    }
    if (F == one) {
	if (f == one) return(g);
	else return(f);
    }
    if (G == one) {
	if (g == one) return(f);
	else return(g);
    }

    /* At this point f and g are not constant. */
    if (f > g) { /* Try to increase cache efficiency. */
	DdNode *tmp = f;
	f = g;
	g = tmp;
	F = Cudd_Regular(f);
	G = Cudd_Regular(g);
    }

    /* Check cache. */
    if (F->ref != 1 || G->ref != 1) {
	r = cuddCacheLookup2(manager, Cudd_bddAnd, f, g);
	if (r != NULL) return(r);
    }

    /* Here we can skip the use of cuddI, because the operands are known
    ** to be non-constant.
    */
    topf = manager->perm[F->index];
    topg = manager->perm[G->index];

    /* Compute cofactors. */
    if (topf <= topg) {
	index = F->index;
	fv = cuddT(F);
	fnv = cuddE(F);
	if (Cudd_IsComplement(f)) {
	    fv = Cudd_Not(fv);
	    fnv = Cudd_Not(fnv);
	}
    } else {
	index = G->index;
	fv = fnv = f;
    }

    if (topg <= topf) {
	gv = cuddT(G);
	gnv = cuddE(G);
	if (Cudd_IsComplement(g)) {
	    gv = Cudd_Not(gv);
	    gnv = Cudd_Not(gnv);
	}
    } else {
	gv = gnv = g;
    }

    t = cuddBddAndRecur(manager, fv, gv);
    if (t == NULL) return(NULL);
    cuddRef(t);

    e = cuddBddAndRecur(manager, fnv, gnv);
    if (e == NULL) {
	Cudd_IterDerefBdd(manager, t);
	return(NULL);
    }
    cuddRef(e);

    if (t == e) {
	r = t;
    } else {
	if (Cudd_IsComplement(t)) {
	    r = cuddUniqueInter(manager,(int)index,Cudd_Not(t),Cudd_Not(e));
	    if (r == NULL) {
		Cudd_IterDerefBdd(manager, t);
		Cudd_IterDerefBdd(manager, e);
		return(NULL);
	    }
	    r = Cudd_Not(r);
	} else {
	    r = cuddUniqueInter(manager,(int)index,t,e);
	    if (r == NULL) {
		Cudd_IterDerefBdd(manager, t);
		Cudd_IterDerefBdd(manager, e);
		return(NULL);
	    }
	}
    }
    cuddDeref(e);
    cuddDeref(t);
    if (F->ref != 1 || G->ref != 1)
	cuddCacheInsert2(manager, Cudd_bddAnd, f, g, r);
    return(r);

} /* end of cuddBddAndRecur */


/**Function********************************************************************

  Synopsis [Implements the recursive step of Cudd_bddXor.]

  Description [Implements the recursive step of Cudd_bddXor by taking
  the exclusive OR of two BDDs.  Returns a pointer to the result is
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddXor]

******************************************************************************/
DdNode *
cuddBddXorRecur(
  DdManager * manager,
  DdNode * f,
  DdNode * g)
{
    DdNode *fv, *fnv, *G, *gv, *gnv;
    DdNode *one, *zero, *r, *t, *e;
    unsigned int topf, topg, index;

    statLine(manager);
    one = DD_ONE(manager);
    zero = Cudd_Not(one);

    /* Terminal cases. */
    if (f == g) return(zero);
    if (f == Cudd_Not(g)) return(one);
    if (f > g) { /* Try to increase cache efficiency and simplify tests. */
	DdNode *tmp = f;
	f = g;
	g = tmp;
    }
    if (g == zero) return(f);
    if (g == one) return(Cudd_Not(f));
    if (Cudd_IsComplement(f)) {
	f = Cudd_Not(f);
	g = Cudd_Not(g);
    }
    /* Now the first argument is regular. */
    if (f == one) return(Cudd_Not(g));

    /* At this point f and g are not constant. */

    /* Check cache. */
    r = cuddCacheLookup2(manager, Cudd_bddXor, f, g);
    if (r != NULL) return(r);

    /* Here we can skip the use of cuddI, because the operands are known
    ** to be non-constant.
    */
    topf = manager->perm[f->index];
    G = Cudd_Regular(g);
    topg = manager->perm[G->index];

    /* Compute cofactors. */
    if (topf <= topg) {
	index = f->index;
	fv = cuddT(f);
	fnv = cuddE(f);
    } else {
	index = G->index;
	fv = fnv = f;
    }

    if (topg <= topf) {
	gv = cuddT(G);
	gnv = cuddE(G);
	if (Cudd_IsComplement(g)) {
	    gv = Cudd_Not(gv);
	    gnv = Cudd_Not(gnv);
	}
    } else {
	gv = gnv = g;
    }

    t = cuddBddXorRecur(manager, fv, gv);
    if (t == NULL) return(NULL);
    cuddRef(t);

    e = cuddBddXorRecur(manager, fnv, gnv);
    if (e == NULL) {
	Cudd_IterDerefBdd(manager, t);
	return(NULL);
    }
    cuddRef(e);

    if (t == e) {
	r = t;
    } else {
	if (Cudd_IsComplement(t)) {
	    r = cuddUniqueInter(manager,(int)index,Cudd_Not(t),Cudd_Not(e));
	    if (r == NULL) {
		Cudd_IterDerefBdd(manager, t);
		Cudd_IterDerefBdd(manager, e);
		return(NULL);
	    }
	    r = Cudd_Not(r);
	} else {
	    r = cuddUniqueInter(manager,(int)index,t,e);
	    if (r == NULL) {
		Cudd_IterDerefBdd(manager, t);
		Cudd_IterDerefBdd(manager, e);
		return(NULL);
	    }
	}
    }
    cuddDeref(e);
    cuddDeref(t);
    cuddCacheInsert2(manager, Cudd_bddXor, f, g, r);
    return(r);

} /* end of cuddBddXorRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Replaces variables with constants if possible.]

  Description [This function performs part of the transformation to
  standard form by replacing variables with constants if possible.]

  SideEffects [None]

  SeeAlso     [bddVarToCanonical bddVarToCanonicalSimple]

******************************************************************************/
static void
bddVarToConst(
  DdNode * f,
  DdNode ** gp,
  DdNode ** hp,
  DdNode * one)
{
    DdNode *g = *gp;
    DdNode *h = *hp;

    if (f == g) {    /* ITE(F,F,H) = ITE(F,1,H) = F + H */
	*gp = one;
    } else if (f == Cudd_Not(g)) {    /* ITE(F,!F,H) = ITE(F,0,H) = !F * H */
	*gp = Cudd_Not(one);
    }
    if (f == h) {    /* ITE(F,G,F) = ITE(F,G,0) = F * G */
	*hp = Cudd_Not(one);
    } else if (f == Cudd_Not(h)) {    /* ITE(F,G,!F) = ITE(F,G,1) = !F + G */
	*hp = one;
    }

} /* end of bddVarToConst */


/**Function********************************************************************

  Synopsis [Picks unique member from equiv expressions.]

  Description [Reduces 2 variable expressions to canonical form.]

  SideEffects [None]

  SeeAlso     [bddVarToConst bddVarToCanonicalSimple]

******************************************************************************/
static int
bddVarToCanonical(
  DdManager * dd,
  DdNode ** fp,
  DdNode ** gp,
  DdNode ** hp,
  unsigned int * topfp,
  unsigned int * topgp,
  unsigned int * tophp)
{
    register DdNode		*F, *G, *H, *r, *f, *g, *h;
    register unsigned int	topf, topg, toph;
    DdNode			*one = dd->one;
    int				comple, change;

    f = *fp;
    g = *gp;
    h = *hp;
    F = Cudd_Regular(f);
    G = Cudd_Regular(g);
    H = Cudd_Regular(h);
    topf = cuddI(dd,F->index);
    topg = cuddI(dd,G->index);
    toph = cuddI(dd,H->index);

    change = 0;

    if (G == one) {			/* ITE(F,c,H) */
	if ((topf > toph) || (topf == toph && f > h)) {
	    r = h;
	    h = f;
	    f = r;			/* ITE(F,1,H) = ITE(H,1,F) */
	    if (g != one) {	/* g == zero */
		f = Cudd_Not(f);		/* ITE(F,0,H) = ITE(!H,0,!F) */
		h = Cudd_Not(h);
	    }
	    change = 1;
	}
    } else if (H == one) {		/* ITE(F,G,c) */
	if ((topf > topg) || (topf == topg && f > g)) {
	    r = g;
	    g = f;
	    f = r;			/* ITE(F,G,0) = ITE(G,F,0) */
	    if (h == one) {
		f = Cudd_Not(f);		/* ITE(F,G,1) = ITE(!G,!F,1) */
		g = Cudd_Not(g);
	    }
	    change = 1;
	}
    } else if (g == Cudd_Not(h)) {	/* ITE(F,G,!G) = ITE(G,F,!F) */
	if ((topf > topg) || (topf == topg && f > g)) {
	    r = f;
	    f = g;
	    g = r;
	    h = Cudd_Not(r);
	    change = 1;
	}
    }
    /* adjust pointers so that the first 2 arguments to ITE are regular */
    if (Cudd_IsComplement(f) != 0) {	/* ITE(!F,G,H) = ITE(F,H,G) */
	f = Cudd_Not(f);
	r = g;
	g = h;
	h = r;
	change = 1;
    }
    comple = 0;
    if (Cudd_IsComplement(g) != 0) {	/* ITE(F,!G,H) = !ITE(F,G,!H) */
	g = Cudd_Not(g);
	h = Cudd_Not(h);
	change = 1;
	comple = 1;
    }
    if (change != 0) {
	*fp = f;
	*gp = g;
	*hp = h;
    }
    *topfp = cuddI(dd,f->index);
    *topgp = cuddI(dd,g->index);
    *tophp = cuddI(dd,Cudd_Regular(h)->index);

    return(comple);

} /* end of bddVarToCanonical */


/**Function********************************************************************

  Synopsis [Picks unique member from equiv expressions.]

  Description [Makes sure the first two pointers are regular.  This
  mat require the complementation of the result, which is signaled by
  returning 1 instead of 0.  This function is simpler than the general
  case because it assumes that no two arguments are the same or
  complementary, and no argument is constant.]

  SideEffects [None]

  SeeAlso     [bddVarToConst bddVarToCanonical]

******************************************************************************/
static int
bddVarToCanonicalSimple(
  DdManager * dd,
  DdNode ** fp,
  DdNode ** gp,
  DdNode ** hp,
  unsigned int * topfp,
  unsigned int * topgp,
  unsigned int * tophp)
{
    register DdNode		*r, *f, *g, *h;
    int				comple, change;

    f = *fp;
    g = *gp;
    h = *hp;

    change = 0;

    /* adjust pointers so that the first 2 arguments to ITE are regular */
    if (Cudd_IsComplement(f)) {	/* ITE(!F,G,H) = ITE(F,H,G) */
	f = Cudd_Not(f);
	r = g;
	g = h;
	h = r;
	change = 1;
    }
    comple = 0;
    if (Cudd_IsComplement(g)) {	/* ITE(F,!G,H) = !ITE(F,G,!H) */
	g = Cudd_Not(g);
	h = Cudd_Not(h);
	change = 1;
	comple = 1;
    }
    if (change) {
	*fp = f;
	*gp = g;
	*hp = h;
    }

    /* Here we can skip the use of cuddI, because the operands are known
    ** to be non-constant.
    */
    *topfp = dd->perm[f->index];
    *topgp = dd->perm[g->index];
    *tophp = dd->perm[Cudd_Regular(h)->index];

    return(comple);

} /* end of bddVarToCanonicalSimple */

