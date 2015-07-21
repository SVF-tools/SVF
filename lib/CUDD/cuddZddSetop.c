/**CFile***********************************************************************

  FileName    [cuddZddSetop.c]

  PackageName [cudd]

  Synopsis    [Set operations on ZDDs.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_zddIte()
		    <li> Cudd_zddUnion()
		    <li> Cudd_zddIntersect()
		    <li> Cudd_zddDiff()
		    <li> Cudd_zddDiffConst()
		    <li> Cudd_zddSubset1()
		    <li> Cudd_zddSubset0()
		    <li> Cudd_zddChange()
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    <li> cuddZddIte()
		    <li> cuddZddUnion()
		    <li> cuddZddIntersect()
		    <li> cuddZddDiff()
		    <li> cuddZddChangeAux()
		    <li> cuddZddSubset1()
		    <li> cuddZddSubset0()
		    </ul>
	       Static procedures included in this module:
		    <ul>
		    <li> zdd_subset1_aux()
		    <li> zdd_subset0_aux()
		    <li> zddVarToConst()
		    </ul>
	      ]

  SeeAlso     []

  Author      [Hyong-Kyoon Shin, In-Ho Moon]

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddSetop.c,v 1.26 2012/02/05 01:07:19 fabio Exp $";
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

static DdNode * zdd_subset1_aux (DdManager *zdd, DdNode *P, DdNode *zvar);
static DdNode * zdd_subset0_aux (DdManager *zdd, DdNode *P, DdNode *zvar);
static void zddVarToConst (DdNode *f, DdNode **gp, DdNode **hp, DdNode *base, DdNode *empty);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Computes the ITE of three ZDDs.]

  Description [Computes the ITE of three ZDDs. Returns a pointer to the
  result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_zddIte(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddZddIte(dd, f, g, h);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddIte */


/**Function********************************************************************

  Synopsis [Computes the union of two ZDDs.]

  Description [Computes the union of two ZDDs. Returns a pointer to the
  result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_zddUnion(
  DdManager * dd,
  DdNode * P,
  DdNode * Q)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddZddUnion(dd, P, Q);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddUnion */


/**Function********************************************************************

  Synopsis [Computes the intersection of two ZDDs.]

  Description [Computes the intersection of two ZDDs. Returns a pointer to
  the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_zddIntersect(
  DdManager * dd,
  DdNode * P,
  DdNode * Q)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddZddIntersect(dd, P, Q);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddIntersect */


/**Function********************************************************************

  Synopsis [Computes the difference of two ZDDs.]

  Description [Computes the difference of two ZDDs. Returns a pointer to the
  result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddDiffConst]

******************************************************************************/
DdNode *
Cudd_zddDiff(
  DdManager * dd,
  DdNode * P,
  DdNode * Q)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddZddDiff(dd, P, Q);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddDiff */


/**Function********************************************************************

  Synopsis [Performs the inclusion test for ZDDs (P implies Q).]

  Description [Inclusion test for ZDDs (P implies Q). No new nodes are
  generated by this procedure. Returns empty if true;
  a valid pointer different from empty or DD_NON_CONSTANT otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddDiff]

******************************************************************************/
DdNode *
Cudd_zddDiffConst(
  DdManager * zdd,
  DdNode * P,
  DdNode * Q)
{
    int		p_top, q_top;
    DdNode	*empty = DD_ZERO(zdd), *t, *res;
    DdManager	*table = zdd;

    statLine(zdd);
    if (P == empty)
	return(empty);
    if (Q == empty)
	return(P);
    if (P == Q)
	return(empty);

    /* Check cache.  The cache is shared by cuddZddDiff(). */
    res = cuddCacheLookup2Zdd(table, cuddZddDiff, P, Q);
    if (res != NULL)
	return(res);

    if (cuddIsConstant(P))
	p_top = P->index;
    else
	p_top = zdd->permZ[P->index];
    if (cuddIsConstant(Q))
	q_top = Q->index;
    else
	q_top = zdd->permZ[Q->index];
    if (p_top < q_top) {
	res = DD_NON_CONSTANT;
    } else if (p_top > q_top) {
	res = Cudd_zddDiffConst(zdd, P, cuddE(Q));
    } else {
	t = Cudd_zddDiffConst(zdd, cuddT(P), cuddT(Q));
	if (t != empty)
	    res = DD_NON_CONSTANT;
	else
	    res = Cudd_zddDiffConst(zdd, cuddE(P), cuddE(Q));
    }

    cuddCacheInsert2(table, cuddZddDiff, P, Q, res);

    return(res);

} /* end of Cudd_zddDiffConst */


/**Function********************************************************************

  Synopsis [Computes the positive cofactor of a ZDD w.r.t. a variable.]

  Description [Computes the positive cofactor of a ZDD w.r.t. a
  variable. In terms of combinations, the result is the set of all
  combinations in which the variable is asserted. Returns a pointer to
  the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddSubset0]

******************************************************************************/
DdNode *
Cudd_zddSubset1(
  DdManager * dd,
  DdNode * P,
  int  var)
{
    DdNode	*r;

    do {
	dd->reordered = 0;
	r = cuddZddSubset1(dd, P, var);
    } while (dd->reordered == 1);

    return(r);

} /* end of Cudd_zddSubset1 */


/**Function********************************************************************

  Synopsis [Computes the negative cofactor of a ZDD w.r.t. a variable.]

  Description [Computes the negative cofactor of a ZDD w.r.t. a
  variable. In terms of combinations, the result is the set of all
  combinations in which the variable is negated. Returns a pointer to
  the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddSubset1]

******************************************************************************/
DdNode *
Cudd_zddSubset0(
  DdManager * dd,
  DdNode * P,
  int  var)
{
    DdNode	*r;

    do {
	dd->reordered = 0;
	r = cuddZddSubset0(dd, P, var);
    } while (dd->reordered == 1);

    return(r);

} /* end of Cudd_zddSubset0 */


/**Function********************************************************************

  Synopsis [Substitutes a variable with its complement in a ZDD.]

  Description [Substitutes a variable with its complement in a ZDD.
  returns a pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_zddChange(
  DdManager * dd,
  DdNode * P,
  int  var)
{
    DdNode	*res;

    if ((unsigned int) var >= CUDD_MAXINDEX - 1) return(NULL);
    
    do {
	dd->reordered = 0;
	res = cuddZddChange(dd, P, var);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddChange */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddIte.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddZddIte(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    DdNode *tautology, *empty;
    DdNode *r,*Gv,*Gvn,*Hv,*Hvn,*t,*e;
    unsigned int topf,topg,toph,v,top;
    int index;

    statLine(dd);
    /* Trivial cases. */
    /* One variable cases. */
    if (f == (empty = DD_ZERO(dd))) {	/* ITE(0,G,H) = H */
	return(h);
    }
    topf = cuddIZ(dd,f->index);
    topg = cuddIZ(dd,g->index);
    toph = cuddIZ(dd,h->index);
    v = ddMin(topg,toph);
    top  = ddMin(topf,v);

    tautology = (top == CUDD_MAXINDEX) ? DD_ONE(dd) : dd->univ[top];
    if (f == tautology) {			/* ITE(1,G,H) = G */
    	return(g);
    }

    /* From now on, f is known to not be a constant. */
    zddVarToConst(f,&g,&h,tautology,empty);

    /* Check remaining one variable cases. */
    if (g == h) {			/* ITE(F,G,G) = G */
	return(g);
    }

    if (g == tautology) {			/* ITE(F,1,0) = F */
	if (h == empty) return(f);
    }

    /* Check cache. */
    r = cuddCacheLookupZdd(dd,DD_ZDD_ITE_TAG,f,g,h);
    if (r != NULL) {
	return(r);
    }

    /* Recompute these because they may have changed in zddVarToConst. */
    topg = cuddIZ(dd,g->index);
    toph = cuddIZ(dd,h->index);
    v = ddMin(topg,toph);

    if (topf < v) {
	r = cuddZddIte(dd,cuddE(f),g,h);
	if (r == NULL) return(NULL);
    } else if (topf > v) {
	if (topg > v) {
	    Gvn = g;
	    index = h->index;
	} else {
	    Gvn = cuddE(g);
	    index = g->index;
	}
	if (toph > v) {
	    Hv = empty; Hvn = h;
	} else {
	    Hv = cuddT(h); Hvn = cuddE(h);
	}
	e = cuddZddIte(dd,f,Gvn,Hvn);
	if (e == NULL) return(NULL);
	cuddRef(e);
	r = cuddZddGetNode(dd,index,Hv,e);
	if (r == NULL) {
	    Cudd_RecursiveDerefZdd(dd,e);
	    return(NULL);
	}
	cuddDeref(e);
    } else {
	index = f->index;
	if (topg > v) {
	    Gv = empty; Gvn = g;
	} else {
	    Gv = cuddT(g); Gvn = cuddE(g);
	}
	if (toph > v) {
	    Hv = empty; Hvn = h;
	} else {
	    Hv = cuddT(h); Hvn = cuddE(h);
	}
	e = cuddZddIte(dd,cuddE(f),Gvn,Hvn);
	if (e == NULL) return(NULL);
	cuddRef(e);
	t = cuddZddIte(dd,cuddT(f),Gv,Hv);
	if (t == NULL) {
	    Cudd_RecursiveDerefZdd(dd,e);
	    return(NULL);
	}
	cuddRef(t);
	r = cuddZddGetNode(dd,index,t,e);
	if (r == NULL) {
	    Cudd_RecursiveDerefZdd(dd,e);
	    Cudd_RecursiveDerefZdd(dd,t);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert(dd,DD_ZDD_ITE_TAG,f,g,h,r);

    return(r);

} /* end of cuddZddIte */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddUnion.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddZddUnion(
  DdManager * zdd,
  DdNode * P,
  DdNode * Q)
{
    int		p_top, q_top;
    DdNode	*empty = DD_ZERO(zdd), *t, *e, *res;
    DdManager	*table = zdd;

    statLine(zdd);
    if (P == empty)
	return(Q);
    if (Q == empty)
	return(P);
    if (P == Q)
	return(P);

    /* Check cache */
    res = cuddCacheLookup2Zdd(table, cuddZddUnion, P, Q);
    if (res != NULL)
	return(res);

    if (cuddIsConstant(P))
	p_top = P->index;
    else
	p_top = zdd->permZ[P->index];
    if (cuddIsConstant(Q))
	q_top = Q->index;
    else
	q_top = zdd->permZ[Q->index];
    if (p_top < q_top) {
	e = cuddZddUnion(zdd, cuddE(P), Q);
	if (e == NULL) return (NULL);
	cuddRef(e);
	res = cuddZddGetNode(zdd, P->index, cuddT(P), e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(table, e);
	    return(NULL);
	}
	cuddDeref(e);
    } else if (p_top > q_top) {
	e = cuddZddUnion(zdd, P, cuddE(Q));
	if (e == NULL) return(NULL);
	cuddRef(e);
	res = cuddZddGetNode(zdd, Q->index, cuddT(Q), e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(table, e);
	    return(NULL);
	}
	cuddDeref(e);
    } else {
	t = cuddZddUnion(zdd, cuddT(P), cuddT(Q));
	if (t == NULL) return(NULL);
	cuddRef(t);
	e = cuddZddUnion(zdd, cuddE(P), cuddE(Q));
	if (e == NULL) {
	    Cudd_RecursiveDerefZdd(table, t);
	    return(NULL);
	}
	cuddRef(e);
	res = cuddZddGetNode(zdd, P->index, t, e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(table, t);
	    Cudd_RecursiveDerefZdd(table, e);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(table, cuddZddUnion, P, Q, res);

    return(res);

} /* end of cuddZddUnion */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddIntersect.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddZddIntersect(
  DdManager * zdd,
  DdNode * P,
  DdNode * Q)
{
    int		p_top, q_top;
    DdNode	*empty = DD_ZERO(zdd), *t, *e, *res;
    DdManager	*table = zdd;

    statLine(zdd);
    if (P == empty)
	return(empty);
    if (Q == empty)
	return(empty);
    if (P == Q)
	return(P);

    /* Check cache. */
    res = cuddCacheLookup2Zdd(table, cuddZddIntersect, P, Q);
    if (res != NULL)
	return(res);

    if (cuddIsConstant(P))
	p_top = P->index;
    else
	p_top = zdd->permZ[P->index];
    if (cuddIsConstant(Q))
	q_top = Q->index;
    else
	q_top = zdd->permZ[Q->index];
    if (p_top < q_top) {
	res = cuddZddIntersect(zdd, cuddE(P), Q);
	if (res == NULL) return(NULL);
    } else if (p_top > q_top) {
	res = cuddZddIntersect(zdd, P, cuddE(Q));
	if (res == NULL) return(NULL);
    } else {
	t = cuddZddIntersect(zdd, cuddT(P), cuddT(Q));
	if (t == NULL) return(NULL);
	cuddRef(t);
	e = cuddZddIntersect(zdd, cuddE(P), cuddE(Q));
	if (e == NULL) {
	    Cudd_RecursiveDerefZdd(table, t);
	    return(NULL);
	}
	cuddRef(e);
	res = cuddZddGetNode(zdd, P->index, t, e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(table, t);
	    Cudd_RecursiveDerefZdd(table, e);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(table, cuddZddIntersect, P, Q, res);

    return(res);

} /* end of cuddZddIntersect */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddDiff.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddZddDiff(
  DdManager * zdd,
  DdNode * P,
  DdNode * Q)
{
    int		p_top, q_top;
    DdNode	*empty = DD_ZERO(zdd), *t, *e, *res;
    DdManager	*table = zdd;

    statLine(zdd);
    if (P == empty)
	return(empty);
    if (Q == empty)
	return(P);
    if (P == Q)
	return(empty);

    /* Check cache.  The cache is shared by Cudd_zddDiffConst(). */
    res = cuddCacheLookup2Zdd(table, cuddZddDiff, P, Q);
    if (res != NULL && res != DD_NON_CONSTANT)
	return(res);

    if (cuddIsConstant(P))
	p_top = P->index;
    else
	p_top = zdd->permZ[P->index];
    if (cuddIsConstant(Q))
	q_top = Q->index;
    else
	q_top = zdd->permZ[Q->index];
    if (p_top < q_top) {
	e = cuddZddDiff(zdd, cuddE(P), Q);
	if (e == NULL) return(NULL);
	cuddRef(e);
	res = cuddZddGetNode(zdd, P->index, cuddT(P), e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(table, e);
	    return(NULL);
	}
	cuddDeref(e);
    } else if (p_top > q_top) {
	res = cuddZddDiff(zdd, P, cuddE(Q));
	if (res == NULL) return(NULL);
    } else {
	t = cuddZddDiff(zdd, cuddT(P), cuddT(Q));
	if (t == NULL) return(NULL);
	cuddRef(t);
	e = cuddZddDiff(zdd, cuddE(P), cuddE(Q));
	if (e == NULL) {
	    Cudd_RecursiveDerefZdd(table, t);
	    return(NULL);
	}
	cuddRef(e);
	res = cuddZddGetNode(zdd, P->index, t, e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(table, t);
	    Cudd_RecursiveDerefZdd(table, e);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(table, cuddZddDiff, P, Q, res);

    return(res);

} /* end of cuddZddDiff */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddChange.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddZddChangeAux(
  DdManager * zdd,
  DdNode * P,
  DdNode * zvar)
{
    int		top_var, level;
    DdNode	*res, *t, *e;
    DdNode	*base = DD_ONE(zdd);
    DdNode	*empty = DD_ZERO(zdd);

    statLine(zdd);
    if (P == empty)
	return(empty);
    if (P == base)
	return(zvar);

    /* Check cache. */
    res = cuddCacheLookup2Zdd(zdd, cuddZddChangeAux, P, zvar);
    if (res != NULL)
	return(res);

    top_var = zdd->permZ[P->index];
    level = zdd->permZ[zvar->index];

    if (top_var > level) {
	res = cuddZddGetNode(zdd, zvar->index, P, DD_ZERO(zdd));
	if (res == NULL) return(NULL);
    } else if (top_var == level) {
	res = cuddZddGetNode(zdd, zvar->index, cuddE(P), cuddT(P));
	if (res == NULL) return(NULL);
    } else {
	t = cuddZddChangeAux(zdd, cuddT(P), zvar);
	if (t == NULL) return(NULL);
	cuddRef(t);
	e = cuddZddChangeAux(zdd, cuddE(P), zvar);
	if (e == NULL) {
	    Cudd_RecursiveDerefZdd(zdd, t);
	    return(NULL);
	}
	cuddRef(e);
	res = cuddZddGetNode(zdd, P->index, t, e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(zdd, t);
	    Cudd_RecursiveDerefZdd(zdd, e);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(zdd, cuddZddChangeAux, P, zvar, res);

    return(res);

} /* end of cuddZddChangeAux */


/**Function********************************************************************

  Synopsis [Computes the positive cofactor of a ZDD w.r.t. a variable.]

  Description [Computes the positive cofactor of a ZDD w.r.t. a
  variable. In terms of combinations, the result is the set of all
  combinations in which the variable is asserted. Returns a pointer to
  the result if successful; NULL otherwise. cuddZddSubset1 performs
  the same function as Cudd_zddSubset1, but does not restart if
  reordering has taken place. Therefore it can be called from within a
  recursive procedure.]

  SideEffects [None]

  SeeAlso     [cuddZddSubset0 Cudd_zddSubset1]

******************************************************************************/
DdNode *
cuddZddSubset1(
  DdManager * dd,
  DdNode * P,
  int  var)
{
    DdNode	*zvar, *r;
    DdNode	*base, *empty;

    base = DD_ONE(dd);
    empty = DD_ZERO(dd);

    zvar = cuddUniqueInterZdd(dd, var, base, empty);
    if (zvar == NULL) {
	return(NULL);
    } else {
	cuddRef(zvar);
	r = zdd_subset1_aux(dd, P, zvar);
	if (r == NULL) {
	    Cudd_RecursiveDerefZdd(dd, zvar);
	    return(NULL);
	}
	cuddRef(r);
	Cudd_RecursiveDerefZdd(dd, zvar);
    }

    cuddDeref(r);
    return(r);

} /* end of cuddZddSubset1 */


/**Function********************************************************************

  Synopsis [Computes the negative cofactor of a ZDD w.r.t. a variable.]

  Description [Computes the negative cofactor of a ZDD w.r.t. a
  variable. In terms of combinations, the result is the set of all
  combinations in which the variable is negated. Returns a pointer to
  the result if successful; NULL otherwise. cuddZddSubset0 performs
  the same function as Cudd_zddSubset0, but does not restart if
  reordering has taken place. Therefore it can be called from within a
  recursive procedure.]

  SideEffects [None]

  SeeAlso     [cuddZddSubset1 Cudd_zddSubset0]

******************************************************************************/
DdNode *
cuddZddSubset0(
  DdManager * dd,
  DdNode * P,
  int  var)
{
    DdNode	*zvar, *r;
    DdNode	*base, *empty;

    base = DD_ONE(dd);
    empty = DD_ZERO(dd);

    zvar = cuddUniqueInterZdd(dd, var, base, empty);
    if (zvar == NULL) {
	return(NULL);
    } else {
	cuddRef(zvar);
	r = zdd_subset0_aux(dd, P, zvar);
	if (r == NULL) {
	    Cudd_RecursiveDerefZdd(dd, zvar);
	    return(NULL);
	}
	cuddRef(r);
	Cudd_RecursiveDerefZdd(dd, zvar);
    }

    cuddDeref(r);
    return(r);

} /* end of cuddZddSubset0 */


/**Function********************************************************************

  Synopsis [Substitutes a variable with its complement in a ZDD.]

  Description [Substitutes a variable with its complement in a ZDD.
  returns a pointer to the result if successful; NULL
  otherwise. cuddZddChange performs the same function as
  Cudd_zddChange, but does not restart if reordering has taken
  place. Therefore it can be called from within a recursive
  procedure.]

  SideEffects [None]

  SeeAlso     [Cudd_zddChange]

******************************************************************************/
DdNode *
cuddZddChange(
  DdManager * dd,
  DdNode * P,
  int  var)
{
    DdNode	*zvar, *res;

    zvar = cuddUniqueInterZdd(dd, var, DD_ONE(dd), DD_ZERO(dd));
    if (zvar == NULL) return(NULL);
    cuddRef(zvar);

    res = cuddZddChangeAux(dd, P, zvar);
    if (res == NULL) {
	Cudd_RecursiveDerefZdd(dd,zvar);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDerefZdd(dd,zvar);
    cuddDeref(res);
    return(res);

} /* end of cuddZddChange */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddSubset1.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
zdd_subset1_aux(
  DdManager * zdd,
  DdNode * P,
  DdNode * zvar)
{
    int		top_var, level;
    DdNode	*res, *t, *e;
    DdNode	*empty;

    statLine(zdd);
    empty = DD_ZERO(zdd);

    /* Check cache. */
    res = cuddCacheLookup2Zdd(zdd, zdd_subset1_aux, P, zvar);
    if (res != NULL)
	return(res);

    if (cuddIsConstant(P)) {
	res = empty;
	cuddCacheInsert2(zdd, zdd_subset1_aux, P, zvar, res);
	return(res);
    }

    top_var = zdd->permZ[P->index];
    level = zdd->permZ[zvar->index];

    if (top_var > level) {
        res = empty;
    } else if (top_var == level) {
	res = cuddT(P);
    } else {
        t = zdd_subset1_aux(zdd, cuddT(P), zvar);
	if (t == NULL) return(NULL);
	cuddRef(t);
        e = zdd_subset1_aux(zdd, cuddE(P), zvar);
	if (e == NULL) {
	    Cudd_RecursiveDerefZdd(zdd, t);
	    return(NULL);
	}
	cuddRef(e);
        res = cuddZddGetNode(zdd, P->index, t, e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(zdd, t);
	    Cudd_RecursiveDerefZdd(zdd, e);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(zdd, zdd_subset1_aux, P, zvar, res);

    return(res);

} /* end of zdd_subset1_aux */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddSubset0.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
zdd_subset0_aux(
  DdManager * zdd,
  DdNode * P,
  DdNode * zvar)
{
    int		top_var, level;
    DdNode	*res, *t, *e;

    statLine(zdd);

    /* Check cache. */
    res = cuddCacheLookup2Zdd(zdd, zdd_subset0_aux, P, zvar);
    if (res != NULL)
	return(res);

    if (cuddIsConstant(P)) {
	res = P;
	cuddCacheInsert2(zdd, zdd_subset0_aux, P, zvar, res);
	return(res);
    }

    top_var = zdd->permZ[P->index];
    level = zdd->permZ[zvar->index];

    if (top_var > level) {
        res = P;
    }
    else if (top_var == level) {
        res = cuddE(P);
    }
    else {
        t = zdd_subset0_aux(zdd, cuddT(P), zvar);
	if (t == NULL) return(NULL);
	cuddRef(t);
        e = zdd_subset0_aux(zdd, cuddE(P), zvar);
	if (e == NULL) {
	    Cudd_RecursiveDerefZdd(zdd, t);
	    return(NULL);
	}
	cuddRef(e);
        res = cuddZddGetNode(zdd, P->index, t, e);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(zdd, t);
	    Cudd_RecursiveDerefZdd(zdd, e);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(zdd, zdd_subset0_aux, P, zvar, res);

    return(res);

} /* end of zdd_subset0_aux */


/**Function********************************************************************

  Synopsis    [Replaces variables with constants if possible (part of
  canonical form).]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
zddVarToConst(
  DdNode * f,
  DdNode ** gp,
  DdNode ** hp,
  DdNode * base,
  DdNode * empty)
{
    DdNode *g = *gp;
    DdNode *h = *hp;

    if (f == g) { /* ITE(F,F,H) = ITE(F,1,H) = F + H */
	*gp = base;
    }

    if (f == h) { /* ITE(F,G,F) = ITE(F,G,0) = F * G */
	*hp = empty;
    }

} /* end of zddVarToConst */

