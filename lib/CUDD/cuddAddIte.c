/**CFile***********************************************************************

  FileName    [cuddAddIte.c]

  PackageName [cudd]

  Synopsis    [ADD ITE function and satellites.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_addIte()
		<li> Cudd_addIteConstant()
		<li> Cudd_addEvalConst()
		<li> Cudd_addCmpl()
		<li> Cudd_addLeq()
		</ul>
	Internal procedures included in this module:
		<ul>
		<li> cuddAddIteRecur()
		<li> cuddAddCmplRecur()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> addVarToConst()
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
static char rcsid[] DD_UNUSED = "$Id: cuddAddIte.c,v 1.16 2012/02/05 01:07:18 fabio Exp $";
#endif


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void addVarToConst (DdNode *f, DdNode **gp, DdNode **hp, DdNode *one, DdNode *zero);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements ITE(f,g,h).]

  Description [Implements ITE(f,g,h). This procedure assumes that f is
  a 0-1 ADD.  Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addIteConstant Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addIte(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddAddIteRecur(dd,f,g,h);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addIte */


/**Function********************************************************************

  Synopsis    [Implements ITEconstant for ADDs.]

  Description [Implements ITEconstant for ADDs.  f must be a 0-1 ADD.
  Returns a pointer to the resulting ADD (which may or may not be
  constant) or DD_NON_CONSTANT. No new nodes are created. This function
  can be used, for instance, to check that g has a constant value
  (specified by h) whenever f is 1. If the constant value is unknown,
  then one should use Cudd_addEvalConst.]

  SideEffects [None]

  SeeAlso     [Cudd_addIte Cudd_addEvalConst Cudd_bddIteConstant]

******************************************************************************/
DdNode *
Cudd_addIteConstant(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode *one,*zero;
    DdNode *Fv,*Fnv,*Gv,*Gnv,*Hv,*Hnv,*r,*t,*e;
    unsigned int topf,topg,toph,v;

    statLine(dd);
    /* Trivial cases. */
    if (f == (one = DD_ONE(dd))) {	/* ITE(1,G,H) = G */
        return(g);
    }
    if (f == (zero = DD_ZERO(dd))) {	/* ITE(0,G,H) = H */
        return(h);
    }

    /* From now on, f is known not to be a constant. */
    addVarToConst(f,&g,&h,one,zero);

    /* Check remaining one variable cases. */
    if (g == h) { 			/* ITE(F,G,G) = G */
        return(g);
    }
    if (cuddIsConstant(g) && cuddIsConstant(h)) {
        return(DD_NON_CONSTANT);
    }

    topf = cuddI(dd,f->index);
    topg = cuddI(dd,g->index);
    toph = cuddI(dd,h->index);
    v = ddMin(topg,toph);

    /* ITE(F,G,H) = (x,G,H) (non constant) if F = (x,1,0), x < top(G,H). */
    if (topf < v && cuddIsConstant(cuddT(f)) && cuddIsConstant(cuddE(f))) {
	return(DD_NON_CONSTANT);
    }

    /* Check cache. */
    r = cuddConstantLookup(dd,DD_ADD_ITE_CONSTANT_TAG,f,g,h);
    if (r != NULL) {
        return(r);
    }

    /* Compute cofactors. */
    if (topf <= v) {
	v = ddMin(topf,v);	/* v = top_var(F,G,H) */
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
        Hv = cuddT(h); Hnv = cuddE(h);
    } else {
        Hv = Hnv = h;
    }
    
    /* Recursive step. */
    t = Cudd_addIteConstant(dd,Fv,Gv,Hv);
    if (t == DD_NON_CONSTANT || !cuddIsConstant(t)) {
	cuddCacheInsert(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
	return(DD_NON_CONSTANT);
    }
    e = Cudd_addIteConstant(dd,Fnv,Gnv,Hnv);
    if (e == DD_NON_CONSTANT || !cuddIsConstant(e) || t != e) {
	cuddCacheInsert(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
	return(DD_NON_CONSTANT);
    }
    cuddCacheInsert(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h, t);
    return(t);

} /* end of Cudd_addIteConstant */


/**Function********************************************************************

  Synopsis    [Checks whether ADD g is constant whenever ADD f is 1.]

  Description [Checks whether ADD g is constant whenever ADD f is 1.  f
  must be a 0-1 ADD.  Returns a pointer to the resulting ADD (which may
  or may not be constant) or DD_NON_CONSTANT. If f is identically 0,
  the check is assumed to be successful, and the background value is
  returned. No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_addIteConstant Cudd_addLeq]

******************************************************************************/
DdNode *
Cudd_addEvalConst(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *zero;
    DdNode *Fv,*Fnv,*Gv,*Gnv,*r,*t,*e;
    unsigned int topf,topg;

#ifdef DD_DEBUG
    assert(!Cudd_IsComplement(f));
#endif

    statLine(dd);
    /* Terminal cases. */
    if (f == DD_ONE(dd) || cuddIsConstant(g)) {
        return(g);
    }
    if (f == (zero = DD_ZERO(dd))) {
        return(dd->background);
    }

#ifdef DD_DEBUG
    assert(!cuddIsConstant(f));
#endif
    /* From now on, f and g are known not to be constants. */

    topf = cuddI(dd,f->index);
    topg = cuddI(dd,g->index);

    /* Check cache. */
    r = cuddConstantLookup(dd,DD_ADD_EVAL_CONST_TAG,f,g,g);
    if (r != NULL) {
        return(r);
    }

    /* Compute cofactors. */
    if (topf <= topg) {
        Fv = cuddT(f); Fnv = cuddE(f);
    } else {
        Fv = Fnv = f;
    }
    if (topg <= topf) {
        Gv = cuddT(g); Gnv = cuddE(g);
    } else {
        Gv = Gnv = g;
    }
    
    /* Recursive step. */
    if (Fv != zero) {
	t = Cudd_addEvalConst(dd,Fv,Gv);
	if (t == DD_NON_CONSTANT || !cuddIsConstant(t)) {
	    cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, DD_NON_CONSTANT);
	    return(DD_NON_CONSTANT);
	}
	if (Fnv != zero) {
	    e = Cudd_addEvalConst(dd,Fnv,Gnv);
	    if (e == DD_NON_CONSTANT || !cuddIsConstant(e) || t != e) {
		cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, DD_NON_CONSTANT);
		return(DD_NON_CONSTANT);
	    }
	}
	cuddCacheInsert2(dd,Cudd_addEvalConst,f,g,t);
	return(t);
    } else { /* Fnv must be != zero */
	e = Cudd_addEvalConst(dd,Fnv,Gnv);
	cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, e);
	return(e);
    }

} /* end of Cudd_addEvalConst */


/**Function********************************************************************

  Synopsis    [Computes the complement of an ADD a la C language.]

  Description [Computes the complement of an ADD a la C language: The
  complement of 0 is 1 and the complement of everything else is 0.
  Returns a pointer to the resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addNegate]

******************************************************************************/
DdNode *
Cudd_addCmpl(
  DdManager * dd,
  DdNode * f)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddAddCmplRecur(dd,f);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addCmpl */


/**Function********************************************************************

  Synopsis    [Determines whether f is less than or equal to g.]

  Description [Returns 1 if f is less than or equal to g; 0 otherwise.
  No new nodes are created. This procedure works for arbitrary ADDs.
  For 0-1 ADDs Cudd_addEvalConst is more efficient.]

  SideEffects [None]

  SeeAlso     [Cudd_addIteConstant Cudd_addEvalConst Cudd_bddLeq]

******************************************************************************/
int
Cudd_addLeq(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *tmp, *fv, *fvn, *gv, *gvn;
    unsigned int topf, topg, res;

    /* Terminal cases. */
    if (f == g) return(1);

    statLine(dd);
    if (cuddIsConstant(f)) {
	if (cuddIsConstant(g)) return(cuddV(f) <= cuddV(g));
	if (f == DD_MINUS_INFINITY(dd)) return(1);
	if (f == DD_PLUS_INFINITY(dd)) return(0); /* since f != g */
    }
    if (g == DD_PLUS_INFINITY(dd)) return(1);
    if (g == DD_MINUS_INFINITY(dd)) return(0); /* since f != g */

    /* Check cache. */
    tmp = cuddCacheLookup2(dd,(DD_CTFP)Cudd_addLeq,f,g);
    if (tmp != NULL) {
	return(tmp == DD_ONE(dd));
    }

    /* Compute cofactors. One of f and g is not constant. */
    topf = cuddI(dd,f->index);
    topg = cuddI(dd,g->index);
    if (topf <= topg) {
	fv = cuddT(f); fvn = cuddE(f);
    } else {
	fv = fvn = f;
    }
    if (topg <= topf) {
	gv = cuddT(g); gvn = cuddE(g);
    } else {
	gv = gvn = g;
    }

    res = Cudd_addLeq(dd,fvn,gvn) && Cudd_addLeq(dd,fv,gv);

    /* Store result in cache and return. */
    cuddCacheInsert2(dd,(DD_CTFP) Cudd_addLeq,f,g,
		     Cudd_NotCond(DD_ONE(dd),res==0));
    return(res);

} /* end of Cudd_addLeq */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_addIte(f,g,h).]

  Description [Implements the recursive step of Cudd_addIte(f,g,h).
  Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addIte]

******************************************************************************/
DdNode *
cuddAddIteRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode *one,*zero;
    DdNode *r,*Fv,*Fnv,*Gv,*Gnv,*Hv,*Hnv,*t,*e;
    unsigned int topf,topg,toph,v;
    int index;

    statLine(dd);
    /* Trivial cases. */

    /* One variable cases. */
    if (f == (one = DD_ONE(dd))) {	/* ITE(1,G,H) = G */
        return(g);
    }
    if (f == (zero = DD_ZERO(dd))) {	/* ITE(0,G,H) = H */
        return(h);
    }

    /* From now on, f is known to not be a constant. */
    addVarToConst(f,&g,&h,one,zero);

    /* Check remaining one variable cases. */
    if (g == h) {			/* ITE(F,G,G) = G */
        return(g);
    }

    if (g == one) {			/* ITE(F,1,0) = F */
        if (h == zero) return(f);
    }

    topf = cuddI(dd,f->index);
    topg = cuddI(dd,g->index);
    toph = cuddI(dd,h->index);
    v = ddMin(topg,toph);

    /* A shortcut: ITE(F,G,H) = (x,G,H) if F=(x,1,0), x < top(G,H). */
    if (topf < v && cuddT(f) == one && cuddE(f) == zero) {
	r = cuddUniqueInter(dd,(int)f->index,g,h);
	return(r);
    }
    if (topf < v && cuddT(f) == zero && cuddE(f) == one) {
	r = cuddUniqueInter(dd,(int)f->index,h,g);
	return(r);
    }

    /* Check cache. */
    r = cuddCacheLookup(dd,DD_ADD_ITE_TAG,f,g,h);
    if (r != NULL) {
        return(r);
    }

    /* Compute cofactors. */
    if (topf <= v) {
	v = ddMin(topf,v);	/* v = top_var(F,G,H) */
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
	index = h->index;
        Hv = cuddT(h); Hnv = cuddE(h);
    } else {
        Hv = Hnv = h;
    }
    
    /* Recursive step. */
    t = cuddAddIteRecur(dd,Fv,Gv,Hv);
    if (t == NULL) return(NULL);
    cuddRef(t);

    e = cuddAddIteRecur(dd,Fnv,Gnv,Hnv);
    if (e == NULL) {
	Cudd_RecursiveDeref(dd,t);
	return(NULL);
    }
    cuddRef(e);

    r = (t == e) ? t : cuddUniqueInter(dd,index,t,e);
    if (r == NULL) {
	Cudd_RecursiveDeref(dd,t);
	Cudd_RecursiveDeref(dd,e);
	return(NULL);
    }
    cuddDeref(t);
    cuddDeref(e);

    cuddCacheInsert(dd,DD_ADD_ITE_TAG,f,g,h,r);

    return(r);

} /* end of cuddAddIteRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addCmpl.]

  Description [Performs the recursive step of Cudd_addCmpl. Returns a
  pointer to the resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addCmpl]

******************************************************************************/
DdNode *
cuddAddCmplRecur(
  DdManager * dd,
  DdNode * f)
{
    DdNode *one,*zero;
    DdNode *r,*Fv,*Fnv,*t,*e;

    statLine(dd);
    one = DD_ONE(dd);
    zero = DD_ZERO(dd); 

    if (cuddIsConstant(f)) {
        if (f == zero) {
	    return(one);
	} else {
	    return(zero);
	}
    }
    r = cuddCacheLookup1(dd,Cudd_addCmpl,f);
    if (r != NULL) {
	return(r);
    }
    Fv = cuddT(f);
    Fnv = cuddE(f);
    t = cuddAddCmplRecur(dd,Fv);
    if (t == NULL) return(NULL);
    cuddRef(t);
    e = cuddAddCmplRecur(dd,Fnv);
    if (e == NULL) {
	Cudd_RecursiveDeref(dd,t);
	return(NULL);
    }
    cuddRef(e);
    r = (t == e) ? t : cuddUniqueInter(dd,(int)f->index,t,e);
    if (r == NULL) {
	Cudd_RecursiveDeref(dd, t);
	Cudd_RecursiveDeref(dd, e);
	return(NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
    cuddCacheInsert1(dd,Cudd_addCmpl,f,r);
    return(r);

} /* end of cuddAddCmplRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Replaces variables with constants if possible (part of
  canonical form).]

  Description []

  SideEffects [None]

******************************************************************************/
static void
addVarToConst(
  DdNode * f,
  DdNode ** gp,
  DdNode ** hp,
  DdNode * one,
  DdNode * zero)
{
    DdNode *g = *gp;
    DdNode *h = *hp;

    if (f == g) { /* ITE(F,F,H) = ITE(F,1,H) = F + H */
	*gp = one;
    }

    if (f == h) { /* ITE(F,G,F) = ITE(F,G,0) = F * G */
	*hp = zero;
    }

} /* end of addVarToConst */
