/**CFile***********************************************************************

  FileName    [cuddZddFuncs.c]

  PackageName [cudd]

  Synopsis    [Functions to manipulate covers represented as ZDDs.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_zddProduct();
		    <li> Cudd_zddUnateProduct();
		    <li> Cudd_zddWeakDiv();
		    <li> Cudd_zddWeakDivF();
		    <li> Cudd_zddDivide();
		    <li> Cudd_zddDivideF();
		    <li> Cudd_zddComplement();
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    <li> cuddZddProduct();
		    <li> cuddZddUnateProduct();
		    <li> cuddZddWeakDiv();
		    <li> cuddZddWeakDivF();
		    <li> cuddZddDivide();
		    <li> cuddZddDivideF();
		    <li> cuddZddGetCofactors3()
		    <li> cuddZddGetCofactors2()
		    <li> cuddZddComplement();
		    <li> cuddZddGetPosVarIndex();
		    <li> cuddZddGetNegVarIndex();
		    <li> cuddZddGetPosVarLevel();
		    <li> cuddZddGetNegVarLevel();
		    </ul>
	       Static procedures included in this module:
		    <ul>
		    </ul>
	      ]

  SeeAlso     []

  Author      [In-Ho Moon]

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddFuncs.c,v 1.17 2012/02/05 01:07:19 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/


/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Computes the product of two covers represented by ZDDs.]

  Description [Computes the product of two covers represented by
  ZDDs. The result is also a ZDD. Returns a pointer to the result if
  successful; NULL otherwise.  The covers on which Cudd_zddProduct
  operates use two ZDD variables for each function variable (one ZDD
  variable for each literal of the variable). Those two ZDD variables
  should be adjacent in the order.]

  SideEffects [None]

  SeeAlso     [Cudd_zddUnateProduct]

******************************************************************************/
DdNode	*
Cudd_zddProduct(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddZddProduct(dd, f, g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddProduct */


/**Function********************************************************************

  Synopsis [Computes the product of two unate covers.]

  Description [Computes the product of two unate covers represented as
  ZDDs. Unate covers use one ZDD variable for each BDD
  variable. Returns a pointer to the result if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddProduct]

******************************************************************************/
DdNode	*
Cudd_zddUnateProduct(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddZddUnateProduct(dd, f, g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddUnateProduct */


/**Function********************************************************************

  Synopsis    [Applies weak division to two covers.]

  Description [Applies weak division to two ZDDs representing two
  covers. Returns a pointer to the ZDD representing the result if
  successful; NULL otherwise. The result of weak division depends on
  the variable order. The covers on which Cudd_zddWeakDiv operates use
  two ZDD variables for each function variable (one ZDD variable for
  each literal of the variable). Those two ZDD variables should be
  adjacent in the order.]

  SideEffects [None]

  SeeAlso     [Cudd_zddDivide]

******************************************************************************/
DdNode	*
Cudd_zddWeakDiv(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddZddWeakDiv(dd, f, g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddWeakDiv */


/**Function********************************************************************

  Synopsis    [Computes the quotient of two unate covers.]

  Description [Computes the quotient of two unate covers represented
  by ZDDs.  Unate covers use one ZDD variable for each BDD
  variable. Returns a pointer to the resulting ZDD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddWeakDiv]

******************************************************************************/
DdNode	*
Cudd_zddDivide(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddZddDivide(dd, f, g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddDivide */


/**Function********************************************************************

  Synopsis    [Modified version of Cudd_zddWeakDiv.]

  Description [Modified version of Cudd_zddWeakDiv. This function may
  disappear in future releases.]

  SideEffects [None]

  SeeAlso     [Cudd_zddWeakDiv]

******************************************************************************/
DdNode	*
Cudd_zddWeakDivF(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddZddWeakDivF(dd, f, g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddWeakDivF */


/**Function********************************************************************

  Synopsis    [Modified version of Cudd_zddDivide.]

  Description [Modified version of Cudd_zddDivide. This function may
  disappear in future releases.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode	*
Cudd_zddDivideF(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddZddDivideF(dd, f, g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_zddDivideF */


/**Function********************************************************************

  Synopsis    [Computes a complement cover for a ZDD node.]

  Description [Computes a complement cover for a ZDD node. For lack of a
  better method, we first extract the function BDD from the ZDD cover,
  then make the complement of the ZDD cover from the complement of the
  BDD node by using ISOP. Returns a pointer to the resulting cover if
  successful; NULL otherwise. The result depends on current variable
  order.]

  SideEffects [The result depends on current variable order.]

  SeeAlso     []

******************************************************************************/
DdNode	*
Cudd_zddComplement(
  DdManager *dd,
  DdNode *node)
{
    DdNode	*b, *isop, *zdd_I;

    /* Check cache */
    zdd_I = cuddCacheLookup1Zdd(dd, cuddZddComplement, node);
    if (zdd_I)
	return(zdd_I);

    b = Cudd_MakeBddFromZddCover(dd, node);
    if (!b)
	return(NULL);
    Cudd_Ref(b);
    isop = Cudd_zddIsop(dd, Cudd_Not(b), Cudd_Not(b), &zdd_I);
    if (!isop) {
	Cudd_RecursiveDeref(dd, b);
	return(NULL);
    }
    Cudd_Ref(isop);
    Cudd_Ref(zdd_I);
    Cudd_RecursiveDeref(dd, b);
    Cudd_RecursiveDeref(dd, isop);

    cuddCacheInsert1(dd, cuddZddComplement, node, zdd_I);
    Cudd_Deref(zdd_I);
    return(zdd_I);
} /* end of Cudd_zddComplement */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddProduct.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddProduct]

******************************************************************************/
DdNode	*
cuddZddProduct(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    int		v, top_f, top_g;
    DdNode	*tmp, *term1, *term2, *term3;
    DdNode	*f0, *f1, *fd, *g0, *g1, *gd;
    DdNode	*R0, *R1, *Rd, *N0, *N1;
    DdNode	*r;
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = DD_ZERO(dd);
    int		flag;
    int		pv, nv;

    statLine(dd);
    if (f == zero || g == zero)
        return(zero);
    if (f == one)
        return(g);
    if (g == one)
        return(f);

    top_f = dd->permZ[f->index];
    top_g = dd->permZ[g->index];

    if (top_f > top_g)
	return(cuddZddProduct(dd, g, f));

    /* Check cache */
    r = cuddCacheLookup2Zdd(dd, cuddZddProduct, f, g);
    if (r)
	return(r);

    v = f->index;	/* either yi or zi */
    flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
    if (flag == 1)
	return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    Cudd_Ref(fd);
    flag = cuddZddGetCofactors3(dd, g, v, &g1, &g0, &gd);
    if (flag == 1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	return(NULL);
    }
    Cudd_Ref(g1);
    Cudd_Ref(g0);
    Cudd_Ref(gd);
    pv = cuddZddGetPosVarIndex(dd, v);
    nv = cuddZddGetNegVarIndex(dd, v);

    Rd = cuddZddProduct(dd, fd, gd);
    if (Rd == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, gd);
	return(NULL);
    }
    Cudd_Ref(Rd);

    term1 = cuddZddProduct(dd, f0, g0);
    if (term1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, Rd);
	return(NULL);
    }
    Cudd_Ref(term1);
    term2 = cuddZddProduct(dd, f0, gd);
    if (term2 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, Rd);
	Cudd_RecursiveDerefZdd(dd, term1);
	return(NULL);
    }
    Cudd_Ref(term2);
    term3 = cuddZddProduct(dd, fd, g0);
    if (term3 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, Rd);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	return(NULL);
    }
    Cudd_Ref(term3);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g0);
    tmp = cuddZddUnion(dd, term1, term2);
    if (tmp == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, Rd);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	Cudd_RecursiveDerefZdd(dd, term3);
	return(NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    R0 = cuddZddUnion(dd, tmp, term3);
    if (R0 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, Rd);
	Cudd_RecursiveDerefZdd(dd, term3);
	Cudd_RecursiveDerefZdd(dd, tmp);
	return(NULL);
    }
    Cudd_Ref(R0);
    Cudd_RecursiveDerefZdd(dd, tmp);
    Cudd_RecursiveDerefZdd(dd, term3);
    N0 = cuddZddGetNode(dd, nv, R0, Rd); /* nv = zi */
    if (N0 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, Rd);
	Cudd_RecursiveDerefZdd(dd, R0);
	return(NULL);
    }
    Cudd_Ref(N0);
    Cudd_RecursiveDerefZdd(dd, R0);
    Cudd_RecursiveDerefZdd(dd, Rd);

    term1 = cuddZddProduct(dd, f1, g1);
    if (term1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, N0);
	return(NULL);
    }
    Cudd_Ref(term1);
    term2 = cuddZddProduct(dd, f1, gd);
    if (term2 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, N0);
	Cudd_RecursiveDerefZdd(dd, term1);
	return(NULL);
    }
    Cudd_Ref(term2);
    term3 = cuddZddProduct(dd, fd, g1);
    if (term3 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, gd);
	Cudd_RecursiveDerefZdd(dd, N0);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	return(NULL);
    }
    Cudd_Ref(term3);
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    tmp = cuddZddUnion(dd, term1, term2);
    if (tmp == NULL) {
	Cudd_RecursiveDerefZdd(dd, N0);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	Cudd_RecursiveDerefZdd(dd, term3);
	return(NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    R1 = cuddZddUnion(dd, tmp, term3);
    if (R1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, N0);
	Cudd_RecursiveDerefZdd(dd, term3);
	Cudd_RecursiveDerefZdd(dd, tmp);
	return(NULL);
    }
    Cudd_Ref(R1);
    Cudd_RecursiveDerefZdd(dd, tmp);
    Cudd_RecursiveDerefZdd(dd, term3);
    N1 = cuddZddGetNode(dd, pv, R1, N0); /* pv = yi */
    if (N1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, N0);
	Cudd_RecursiveDerefZdd(dd, R1);
	return(NULL);
    }
    Cudd_Ref(N1);
    Cudd_RecursiveDerefZdd(dd, R1);
    Cudd_RecursiveDerefZdd(dd, N0);

    cuddCacheInsert2(dd, cuddZddProduct, f, g, N1);
    Cudd_Deref(N1);
    return(N1);

} /* end of cuddZddProduct */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddUnateProduct.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddUnateProduct]

******************************************************************************/
DdNode	*
cuddZddUnateProduct(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    int		v, top_f, top_g;
    DdNode	*term1, *term2, *term3, *term4;
    DdNode	*sum1, *sum2;
    DdNode	*f0, *f1, *g0, *g1;
    DdNode	*r;
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = DD_ZERO(dd);
    int		flag;

    statLine(dd);
    if (f == zero || g == zero)
        return(zero);
    if (f == one)
        return(g);
    if (g == one)
        return(f);

    top_f = dd->permZ[f->index];
    top_g = dd->permZ[g->index];

    if (top_f > top_g)
	return(cuddZddUnateProduct(dd, g, f));

    /* Check cache */
    r = cuddCacheLookup2Zdd(dd, cuddZddUnateProduct, f, g);
    if (r)
	return(r);

    v = f->index;	/* either yi or zi */
    flag = cuddZddGetCofactors2(dd, f, v, &f1, &f0);
    if (flag == 1)
	return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    flag = cuddZddGetCofactors2(dd, g, v, &g1, &g0);
    if (flag == 1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	return(NULL);
    }
    Cudd_Ref(g1);
    Cudd_Ref(g0);

    term1 = cuddZddUnateProduct(dd, f1, g1);
    if (term1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	return(NULL);
    }
    Cudd_Ref(term1);
    term2 = cuddZddUnateProduct(dd, f1, g0);
    if (term2 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, term1);
	return(NULL);
    }
    Cudd_Ref(term2);
    term3 = cuddZddUnateProduct(dd, f0, g1);
    if (term3 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	return(NULL);
    }
    Cudd_Ref(term3);
    term4 = cuddZddUnateProduct(dd, f0, g0);
    if (term4 == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	Cudd_RecursiveDerefZdd(dd, term3);
	return(NULL);
    }
    Cudd_Ref(term4);
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    sum1 = cuddZddUnion(dd, term1, term2);
    if (sum1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, term2);
	Cudd_RecursiveDerefZdd(dd, term3);
	Cudd_RecursiveDerefZdd(dd, term4);
	return(NULL);
    }
    Cudd_Ref(sum1);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    sum2 = cuddZddUnion(dd, sum1, term3);
    if (sum2 == NULL) {
	Cudd_RecursiveDerefZdd(dd, term3);
	Cudd_RecursiveDerefZdd(dd, term4);
	Cudd_RecursiveDerefZdd(dd, sum1);
	return(NULL);
    }
    Cudd_Ref(sum2);
    Cudd_RecursiveDerefZdd(dd, sum1);
    Cudd_RecursiveDerefZdd(dd, term3);
    r = cuddZddGetNode(dd, v, sum2, term4);
    if (r == NULL) {
	Cudd_RecursiveDerefZdd(dd, term4);
	Cudd_RecursiveDerefZdd(dd, sum2);
	return(NULL);
    }
    Cudd_Ref(r);
    Cudd_RecursiveDerefZdd(dd, sum2);
    Cudd_RecursiveDerefZdd(dd, term4);

    cuddCacheInsert2(dd, cuddZddUnateProduct, f, g, r);
    Cudd_Deref(r);
    return(r);

} /* end of cuddZddUnateProduct */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddWeakDiv.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddWeakDiv]

******************************************************************************/
DdNode	*
cuddZddWeakDiv(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    int		v;
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = DD_ZERO(dd);
    DdNode	*f0, *f1, *fd, *g0, *g1, *gd;
    DdNode	*q, *tmp;
    DdNode	*r;
    int		flag;

    statLine(dd);
    if (g == one)
	return(f);
    if (f == zero || f == one)
	return(zero);
    if (f == g)
	return(one);

    /* Check cache. */
    r = cuddCacheLookup2Zdd(dd, cuddZddWeakDiv, f, g);
    if (r)
	return(r);

    v = g->index;

    flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
    if (flag == 1)
	return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    Cudd_Ref(fd);
    flag = cuddZddGetCofactors3(dd, g, v, &g1, &g0, &gd);
    if (flag == 1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	return(NULL);
    }
    Cudd_Ref(g1);
    Cudd_Ref(g0);
    Cudd_Ref(gd);

    q = g;

    if (g0 != zero) {
	q = cuddZddWeakDiv(dd, f0, g0);
	if (q == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, g0);
	    Cudd_RecursiveDerefZdd(dd, gd);
	    return(NULL);
	}
	Cudd_Ref(q);
    }
    else
	Cudd_Ref(q);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g0);

    if (q == zero) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
	cuddCacheInsert2(dd, cuddZddWeakDiv, f, g, zero);
	Cudd_Deref(q);
	return(zero);
    }

    if (g1 != zero) {
	Cudd_RecursiveDerefZdd(dd, q);
	tmp = cuddZddWeakDiv(dd, f1, g1);
	if (tmp == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, gd);
	    return(NULL);
	}
	Cudd_Ref(tmp);
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, g1);
	if (q == g)
	    q = tmp;
	else {
	    q = cuddZddIntersect(dd, q, tmp);
	    if (q == NULL) {
		Cudd_RecursiveDerefZdd(dd, fd);
		Cudd_RecursiveDerefZdd(dd, gd);
		return(NULL);
	    }
	    Cudd_Ref(q);
	    Cudd_RecursiveDerefZdd(dd, tmp);
	}
    }
    else {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, g1);
    }

    if (q == zero) {
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
	cuddCacheInsert2(dd, cuddZddWeakDiv, f, g, zero);
	Cudd_Deref(q);
	return(zero);
    }

    if (gd != zero) {
	Cudd_RecursiveDerefZdd(dd, q);
	tmp = cuddZddWeakDiv(dd, fd, gd);
	if (tmp == NULL) {
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, gd);
	    return(NULL);
	}
	Cudd_Ref(tmp);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
	if (q == g)
	    q = tmp;
	else {
	    q = cuddZddIntersect(dd, q, tmp);
	    if (q == NULL) {
		Cudd_RecursiveDerefZdd(dd, tmp);
		return(NULL);
	    }
	    Cudd_Ref(q);
	    Cudd_RecursiveDerefZdd(dd, tmp);
	}
    }
    else {
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
    }

    cuddCacheInsert2(dd, cuddZddWeakDiv, f, g, q);
    Cudd_Deref(q);
    return(q);

} /* end of cuddZddWeakDiv */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddWeakDivF.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddWeakDivF]

******************************************************************************/
DdNode	*
cuddZddWeakDivF(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    int		v, top_f, top_g, vf, vg;
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = DD_ZERO(dd);
    DdNode	*f0, *f1, *fd, *g0, *g1, *gd;
    DdNode	*q, *tmp;
    DdNode	*r;
    DdNode	*term1, *term0, *termd;
    int		flag;
    int		pv, nv;

    statLine(dd);
    if (g == one)
	return(f);
    if (f == zero || f == one)
	return(zero);
    if (f == g)
	return(one);

    /* Check cache. */
    r = cuddCacheLookup2Zdd(dd, cuddZddWeakDivF, f, g);
    if (r)
	return(r);

    top_f = dd->permZ[f->index];
    top_g = dd->permZ[g->index];
    vf = top_f >> 1;
    vg = top_g >> 1;
    v = ddMin(top_f, top_g);

    if (v == top_f && vf < vg) {
	v = f->index;
	flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
	if (flag == 1)
	    return(NULL);
	Cudd_Ref(f1);
	Cudd_Ref(f0);
	Cudd_Ref(fd);

	pv = cuddZddGetPosVarIndex(dd, v);
	nv = cuddZddGetNegVarIndex(dd, v);

	term1 = cuddZddWeakDivF(dd, f1, g);
	if (term1 == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, fd);
	    return(NULL);
	}
	Cudd_Ref(term1);
	Cudd_RecursiveDerefZdd(dd, f1);
	term0 = cuddZddWeakDivF(dd, f0, g);
	if (term0 == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, term1);
	    return(NULL);
	}
	Cudd_Ref(term0);
	Cudd_RecursiveDerefZdd(dd, f0);
	termd = cuddZddWeakDivF(dd, fd, g);
	if (termd == NULL) {
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, term1);
	    Cudd_RecursiveDerefZdd(dd, term0);
	    return(NULL);
	}
	Cudd_Ref(termd);
	Cudd_RecursiveDerefZdd(dd, fd);

	tmp = cuddZddGetNode(dd, nv, term0, termd); /* nv = zi */
	if (tmp == NULL) {
	    Cudd_RecursiveDerefZdd(dd, term1);
	    Cudd_RecursiveDerefZdd(dd, term0);
	    Cudd_RecursiveDerefZdd(dd, termd);
	    return(NULL);
	}
	Cudd_Ref(tmp);
	Cudd_RecursiveDerefZdd(dd, term0);
	Cudd_RecursiveDerefZdd(dd, termd);
	q = cuddZddGetNode(dd, pv, term1, tmp); /* pv = yi */
	if (q == NULL) {
	    Cudd_RecursiveDerefZdd(dd, term1);
	    Cudd_RecursiveDerefZdd(dd, tmp);
	    return(NULL);
	}
	Cudd_Ref(q);
	Cudd_RecursiveDerefZdd(dd, term1);
	Cudd_RecursiveDerefZdd(dd, tmp);

	cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, q);
	Cudd_Deref(q);
	return(q);
    }

    if (v == top_f)
	v = f->index;
    else
	v = g->index;

    flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
    if (flag == 1)
	return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    Cudd_Ref(fd);
    flag = cuddZddGetCofactors3(dd, g, v, &g1, &g0, &gd);
    if (flag == 1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	return(NULL);
    }
    Cudd_Ref(g1);
    Cudd_Ref(g0);
    Cudd_Ref(gd);

    q = g;

    if (g0 != zero) {
	q = cuddZddWeakDivF(dd, f0, g0);
	if (q == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, g0);
	    Cudd_RecursiveDerefZdd(dd, gd);
	    return(NULL);
	}
	Cudd_Ref(q);
    }
    else
	Cudd_Ref(q);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g0);

    if (q == zero) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
	cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, zero);
	Cudd_Deref(q);
	return(zero);
    }

    if (g1 != zero) {
	Cudd_RecursiveDerefZdd(dd, q);
	tmp = cuddZddWeakDivF(dd, f1, g1);
	if (tmp == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, gd);
	    return(NULL);
	}
	Cudd_Ref(tmp);
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, g1);
	if (q == g)
	    q = tmp;
	else {
	    q = cuddZddIntersect(dd, q, tmp);
	    if (q == NULL) {
		Cudd_RecursiveDerefZdd(dd, fd);
		Cudd_RecursiveDerefZdd(dd, gd);
		return(NULL);
	    }
	    Cudd_Ref(q);
	    Cudd_RecursiveDerefZdd(dd, tmp);
	}
    }
    else {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, g1);
    }

    if (q == zero) {
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
	cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, zero);
	Cudd_Deref(q);
	return(zero);
    }

    if (gd != zero) {
	Cudd_RecursiveDerefZdd(dd, q);
	tmp = cuddZddWeakDivF(dd, fd, gd);
	if (tmp == NULL) {
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDerefZdd(dd, gd);
	    return(NULL);
	}
	Cudd_Ref(tmp);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
	if (q == g)
	    q = tmp;
	else {
	    q = cuddZddIntersect(dd, q, tmp);
	    if (q == NULL) {
		Cudd_RecursiveDerefZdd(dd, tmp);
		return(NULL);
	    }
	    Cudd_Ref(q);
	    Cudd_RecursiveDerefZdd(dd, tmp);
	}
    }
    else {
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDerefZdd(dd, gd);
    }

    cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, q);
    Cudd_Deref(q);
    return(q);

} /* end of cuddZddWeakDivF */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddDivide.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddDivide]

******************************************************************************/
DdNode	*
cuddZddDivide(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    int		v;
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = DD_ZERO(dd);
    DdNode	*f0, *f1, *g0, *g1;
    DdNode	*q, *r, *tmp;
    int		flag;

    statLine(dd);
    if (g == one)
	return(f);
    if (f == zero || f == one)
	return(zero);
    if (f == g)
	return(one);

    /* Check cache. */
    r = cuddCacheLookup2Zdd(dd, cuddZddDivide, f, g);
    if (r)
	return(r);

    v = g->index;

    flag = cuddZddGetCofactors2(dd, f, v, &f1, &f0);
    if (flag == 1)
	return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    flag = cuddZddGetCofactors2(dd, g, v, &g1, &g0);	/* g1 != zero */
    if (flag == 1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	return(NULL);
    }
    Cudd_Ref(g1);
    Cudd_Ref(g0);

    r = cuddZddDivide(dd, f1, g1);
    if (r == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	return(NULL);
    }
    Cudd_Ref(r);

    if (r != zero && g0 != zero) {
	tmp = r;
	q = cuddZddDivide(dd, f0, g0);
	if (q == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, g0);
	    return(NULL);
	}
	Cudd_Ref(q);
	r = cuddZddIntersect(dd, r, q);
	if (r == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, g0);
	    Cudd_RecursiveDerefZdd(dd, q);
	    return(NULL);
	}
	Cudd_Ref(r);
	Cudd_RecursiveDerefZdd(dd, q);
	Cudd_RecursiveDerefZdd(dd, tmp);
    }

    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    
    cuddCacheInsert2(dd, cuddZddDivide, f, g, r);
    Cudd_Deref(r);
    return(r);

} /* end of cuddZddDivide */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddDivideF.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddDivideF]

******************************************************************************/
DdNode	*
cuddZddDivideF(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    int		v;
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = DD_ZERO(dd);
    DdNode	*f0, *f1, *g0, *g1;
    DdNode	*q, *r, *tmp;
    int		flag;

    statLine(dd);
    if (g == one)
	return(f);
    if (f == zero || f == one)
	return(zero);
    if (f == g)
	return(one);

    /* Check cache. */
    r = cuddCacheLookup2Zdd(dd, cuddZddDivideF, f, g);
    if (r)
	return(r);

    v = g->index;

    flag = cuddZddGetCofactors2(dd, f, v, &f1, &f0);
    if (flag == 1)
	return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    flag = cuddZddGetCofactors2(dd, g, v, &g1, &g0);	/* g1 != zero */
    if (flag == 1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	return(NULL);
    }
    Cudd_Ref(g1);
    Cudd_Ref(g0);

    r = cuddZddDivideF(dd, f1, g1);
    if (r == NULL) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, g1);
	Cudd_RecursiveDerefZdd(dd, g0);
	return(NULL);
    }
    Cudd_Ref(r);

    if (r != zero && g0 != zero) {
	tmp = r;
	q = cuddZddDivideF(dd, f0, g0);
	if (q == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, g0);
	    return(NULL);
	}
	Cudd_Ref(q);
	r = cuddZddIntersect(dd, r, q);
	if (r == NULL) {
	    Cudd_RecursiveDerefZdd(dd, f1);
	    Cudd_RecursiveDerefZdd(dd, f0);
	    Cudd_RecursiveDerefZdd(dd, g1);
	    Cudd_RecursiveDerefZdd(dd, g0);
	    Cudd_RecursiveDerefZdd(dd, q);
	    return(NULL);
	}
	Cudd_Ref(r);
	Cudd_RecursiveDerefZdd(dd, q);
	Cudd_RecursiveDerefZdd(dd, tmp);
    }

    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    
    cuddCacheInsert2(dd, cuddZddDivideF, f, g, r);
    Cudd_Deref(r);
    return(r);

} /* end of cuddZddDivideF */


/**Function********************************************************************

  Synopsis    [Computes the three-way decomposition of f w.r.t. v.]

  Description [Computes the three-way decomposition of function f (represented
  by a ZDD) wit respect to variable v.  Returns 0 if successful; 1 otherwise.]

  SideEffects [The results are returned in f1, f0, and fd.]

  SeeAlso     [cuddZddGetCofactors2]

******************************************************************************/
int
cuddZddGetCofactors3(
  DdManager * dd,
  DdNode * f,
  int  v,
  DdNode ** f1,
  DdNode ** f0,
  DdNode ** fd)
{
    DdNode	*pc, *nc;
    DdNode	*zero = DD_ZERO(dd);
    int		top, hv, ht, pv, nv;
    int		level;

    top = dd->permZ[f->index];
    level = dd->permZ[v];
    hv = level >> 1;
    ht = top >> 1;

    if (hv < ht) {
	*f1 = zero;
	*f0 = zero;
	*fd = f;
    }
    else {
	pv = cuddZddGetPosVarIndex(dd, v);
	nv = cuddZddGetNegVarIndex(dd, v);

	/* not to create intermediate ZDD node */
	if (cuddZddGetPosVarLevel(dd, v) < cuddZddGetNegVarLevel(dd, v)) {
	    pc = cuddZddSubset1(dd, f, pv);
	    if (pc == NULL)
		return(1);
	    Cudd_Ref(pc);
	    nc = cuddZddSubset0(dd, f, pv);
	    if (nc == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		return(1);
	    }
	    Cudd_Ref(nc);

	    *f1 = cuddZddSubset0(dd, pc, nv);
	    if (*f1 == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		Cudd_RecursiveDerefZdd(dd, nc);
		return(1);
	    }
	    Cudd_Ref(*f1);
	    *f0 = cuddZddSubset1(dd, nc, nv);
	    if (*f0 == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		Cudd_RecursiveDerefZdd(dd, nc);
		Cudd_RecursiveDerefZdd(dd, *f1);
		return(1);
	    }
	    Cudd_Ref(*f0);

	    *fd = cuddZddSubset0(dd, nc, nv);
	    if (*fd == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		Cudd_RecursiveDerefZdd(dd, nc);
		Cudd_RecursiveDerefZdd(dd, *f1);
		Cudd_RecursiveDerefZdd(dd, *f0);
		return(1);
	    }
	    Cudd_Ref(*fd);
	} else {
	    pc = cuddZddSubset1(dd, f, nv);
	    if (pc == NULL)
		return(1);
	    Cudd_Ref(pc);
	    nc = cuddZddSubset0(dd, f, nv);
	    if (nc == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		return(1);
	    }
	    Cudd_Ref(nc);

	    *f0 = cuddZddSubset0(dd, pc, pv);
	    if (*f0 == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		Cudd_RecursiveDerefZdd(dd, nc);
		return(1);
	    }
	    Cudd_Ref(*f0);
	    *f1 = cuddZddSubset1(dd, nc, pv);
	    if (*f1 == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		Cudd_RecursiveDerefZdd(dd, nc);
		Cudd_RecursiveDerefZdd(dd, *f0);
		return(1);
	    }
	    Cudd_Ref(*f1);

	    *fd = cuddZddSubset0(dd, nc, pv);
	    if (*fd == NULL) {
		Cudd_RecursiveDerefZdd(dd, pc);
		Cudd_RecursiveDerefZdd(dd, nc);
		Cudd_RecursiveDerefZdd(dd, *f1);
		Cudd_RecursiveDerefZdd(dd, *f0);
		return(1);
	    }
	    Cudd_Ref(*fd);
	}

	Cudd_RecursiveDerefZdd(dd, pc);
	Cudd_RecursiveDerefZdd(dd, nc);
	Cudd_Deref(*f1);
	Cudd_Deref(*f0);
	Cudd_Deref(*fd);
    }
    return(0);

} /* end of cuddZddGetCofactors3 */


/**Function********************************************************************

  Synopsis    [Computes the two-way decomposition of f w.r.t. v.]

  Description []

  SideEffects [The results are returned in f1 and f0.]

  SeeAlso     [cuddZddGetCofactors3]

******************************************************************************/
int
cuddZddGetCofactors2(
  DdManager * dd,
  DdNode * f,
  int  v,
  DdNode ** f1,
  DdNode ** f0)
{
    *f1 = cuddZddSubset1(dd, f, v);
    if (*f1 == NULL)
	return(1);
    *f0 = cuddZddSubset0(dd, f, v);
    if (*f0 == NULL) {
	Cudd_RecursiveDerefZdd(dd, *f1);
	return(1);
    }
    return(0);

} /* end of cuddZddGetCofactors2 */


/**Function********************************************************************

  Synopsis    [Computes a complement of a ZDD node.]

  Description [Computes the complement of a ZDD node. So far, since we
  couldn't find a direct way to get the complement of a ZDD cover, we first
  convert a ZDD cover to a BDD, then make the complement of the ZDD cover
  from the complement of the BDD node by using ISOP.]

  SideEffects [The result depends on current variable order.]

  SeeAlso     []

******************************************************************************/
DdNode	*
cuddZddComplement(
  DdManager * dd,
  DdNode *node)
{
    DdNode	*b, *isop, *zdd_I;

    /* Check cache */
    zdd_I = cuddCacheLookup1Zdd(dd, cuddZddComplement, node);
    if (zdd_I)
	return(zdd_I);

    b = cuddMakeBddFromZddCover(dd, node);
    if (!b)
	return(NULL);
    cuddRef(b);
    isop = cuddZddIsop(dd, Cudd_Not(b), Cudd_Not(b), &zdd_I);
    if (!isop) {
	Cudd_RecursiveDeref(dd, b);
	return(NULL);
    }
    cuddRef(isop);
    cuddRef(zdd_I);
    Cudd_RecursiveDeref(dd, b);
    Cudd_RecursiveDeref(dd, isop);

    cuddCacheInsert1(dd, cuddZddComplement, node, zdd_I);
    cuddDeref(zdd_I);
    return(zdd_I);
} /* end of cuddZddComplement */


/**Function********************************************************************

  Synopsis    [Returns the index of positive ZDD variable.]

  Description [Returns the index of positive ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int
cuddZddGetPosVarIndex(
  DdManager * dd,
  int index)
{
    int	pv = (index >> 1) << 1;
    return(pv);
} /* end of cuddZddGetPosVarIndex */


/**Function********************************************************************

  Synopsis    [Returns the index of negative ZDD variable.]

  Description [Returns the index of negative ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int
cuddZddGetNegVarIndex(
  DdManager * dd,
  int index)
{
    int	nv = index | 0x1;
    return(nv);
} /* end of cuddZddGetPosVarIndex */


/**Function********************************************************************

  Synopsis    [Returns the level of positive ZDD variable.]

  Description [Returns the level of positive ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int
cuddZddGetPosVarLevel(
  DdManager * dd,
  int index)
{
    int	pv = cuddZddGetPosVarIndex(dd, index);
    return(dd->permZ[pv]);
} /* end of cuddZddGetPosVarLevel */


/**Function********************************************************************

  Synopsis    [Returns the level of negative ZDD variable.]

  Description [Returns the level of negative ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int
cuddZddGetNegVarLevel(
  DdManager * dd,
  int index)
{
    int	nv = cuddZddGetNegVarIndex(dd, index);
    return(dd->permZ[nv]);
} /* end of cuddZddGetNegVarLevel */
