/**CFile***********************************************************************

  FileName    [cuddZddIsop.c]

  PackageName [cudd]

  Synopsis    [Functions to find irredundant SOP covers as ZDDs from BDDs.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_bddIsop()
		    <li> Cudd_zddIsop()
		    <li> Cudd_MakeBddFromZddCover()
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    <li> cuddBddIsop()
		    <li> cuddZddIsop()
		    <li> cuddMakeBddFromZddCover()
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
static char rcsid[] DD_UNUSED = "$Id: cuddZddIsop.c,v 1.22 2012/02/05 01:07:19 fabio Exp $";
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

  Synopsis    [Computes an ISOP in ZDD form from BDDs.]

  Description [Computes an irredundant sum of products (ISOP) in ZDD
  form from BDDs. The two BDDs L and U represent the lower bound and
  the upper bound, respectively, of the function. The ISOP uses two
  ZDD variables for each BDD variable: One for the positive literal,
  and one for the negative literal. These two variables should be
  adjacent in the ZDD order. The two ZDD variables corresponding to
  BDD variable <code>i</code> should have indices <code>2i</code> and
  <code>2i+1</code>.  The result of this procedure depends on the
  variable order. If successful, Cudd_zddIsop returns the BDD for
  the function chosen from the interval. The ZDD representing the
  irredundant cover is returned as a side effect in zdd_I. In case of
  failure, NULL is returned.]

  SideEffects [zdd_I holds the pointer to the ZDD for the ISOP on
  successful return.]

  SeeAlso     [Cudd_bddIsop Cudd_zddVarsFromBddVars]

******************************************************************************/
DdNode	*
Cudd_zddIsop(
  DdManager * dd,
  DdNode * L,
  DdNode * U,
  DdNode ** zdd_I)
{
    DdNode	*res;
    int		autoDynZ;

    autoDynZ = dd->autoDynZ;
    dd->autoDynZ = 0;

    do {
	dd->reordered = 0;
	res = cuddZddIsop(dd, L, U, zdd_I);
    } while (dd->reordered == 1);
    dd->autoDynZ = autoDynZ;
    return(res);

} /* end of Cudd_zddIsop */


/**Function********************************************************************

  Synopsis    [Computes a BDD in the interval between L and U with a
  simple sum-of-product cover.]

  Description [Computes a BDD in the interval between L and U with a
  simple sum-of-product cover. This procedure is similar to
  Cudd_zddIsop, but it does not return the ZDD for the cover. Returns
  a pointer to the BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddIsop]

******************************************************************************/
DdNode	*
Cudd_bddIsop(
  DdManager * dd,
  DdNode * L,
  DdNode * U)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddBddIsop(dd, L, U);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddIsop */


/**Function********************************************************************

  Synopsis [Converts a ZDD cover to a BDD.]

  Description [Converts a ZDD cover to a BDD for the function represented
  by the cover. If successful, it returns a BDD node, otherwise it returns
  NULL.]

  SideEffects []

  SeeAlso     [Cudd_zddIsop]

******************************************************************************/
DdNode	*
Cudd_MakeBddFromZddCover(
  DdManager * dd,
  DdNode * node)
{
    DdNode	*res;

    do {
	dd->reordered = 0;
	res = cuddMakeBddFromZddCover(dd, node);
    } while (dd->reordered == 1);
    return(res);
} /* end of Cudd_MakeBddFromZddCover */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddIsop.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddIsop]

******************************************************************************/
DdNode	*
cuddZddIsop(
  DdManager * dd,
  DdNode * L,
  DdNode * U,
  DdNode ** zdd_I)
{
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = Cudd_Not(one);
    DdNode	*zdd_one = DD_ONE(dd);
    DdNode	*zdd_zero = DD_ZERO(dd);
    int		v, top_l, top_u;
    DdNode	*Lsub0, *Usub0, *Lsub1, *Usub1, *Ld, *Ud;
    DdNode	*Lsuper0, *Usuper0, *Lsuper1, *Usuper1;
    DdNode	*Isub0, *Isub1, *Id;
    DdNode	*zdd_Isub0, *zdd_Isub1, *zdd_Id;
    DdNode	*x;
    DdNode	*term0, *term1, *sum;
    DdNode	*Lv, *Uv, *Lnv, *Unv;
    DdNode	*r, *y, *z;
    int		index;
    DD_CTFP	cacheOp;

    statLine(dd);
    if (L == zero) {
	*zdd_I = zdd_zero;
	return(zero);
    }
    if (U == one) {
	*zdd_I = zdd_one;
	return(one);
    }

    if (U == zero || L == one) {
	printf("*** ERROR : illegal condition for ISOP (U < L).\n");
	exit(1);
    }

    /* Check the cache. We store two results for each recursive call.
    ** One is the BDD, and the other is the ZDD. Both are needed.
    ** Hence we need a double hit in the cache to terminate the
    ** recursion. Clearly, collisions may evict only one of the two
    ** results. */
    cacheOp = (DD_CTFP) cuddZddIsop;
    r = cuddCacheLookup2(dd, cuddBddIsop, L, U);
    if (r) {
	*zdd_I = cuddCacheLookup2Zdd(dd, cacheOp, L, U);
	if (*zdd_I)
	    return(r);
	else {
	    /* The BDD result may have been dead. In that case
	    ** cuddCacheLookup2 would have called cuddReclaim,
	    ** whose effects we now have to undo. */
	    cuddRef(r);
	    Cudd_RecursiveDeref(dd, r);
	}
    }

    top_l = dd->perm[Cudd_Regular(L)->index];
    top_u = dd->perm[Cudd_Regular(U)->index];
    v = ddMin(top_l, top_u);

    /* Compute cofactors. */
    if (top_l == v) {
	index = Cudd_Regular(L)->index;
	Lv = Cudd_T(L);
	Lnv = Cudd_E(L);
	if (Cudd_IsComplement(L)) {
	    Lv = Cudd_Not(Lv);
	    Lnv = Cudd_Not(Lnv);
	}
    }
    else {
	index = Cudd_Regular(U)->index;
	Lv = Lnv = L;
    }

    if (top_u == v) {
	Uv = Cudd_T(U);
	Unv = Cudd_E(U);
	if (Cudd_IsComplement(U)) {
	    Uv = Cudd_Not(Uv);
	    Unv = Cudd_Not(Unv);
	}
    }
    else {
	Uv = Unv = U;
    }

    Lsub0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Uv));
    if (Lsub0 == NULL)
	return(NULL);
    Cudd_Ref(Lsub0);
    Usub0 = Unv;
    Lsub1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Unv));
    if (Lsub1 == NULL) {
	Cudd_RecursiveDeref(dd, Lsub0);
	return(NULL);
    }
    Cudd_Ref(Lsub1);
    Usub1 = Uv;

    Isub0 = cuddZddIsop(dd, Lsub0, Usub0, &zdd_Isub0);
    if (Isub0 == NULL) {
	Cudd_RecursiveDeref(dd, Lsub0);
	Cudd_RecursiveDeref(dd, Lsub1);
	return(NULL);
    }
    /*
    if ((!cuddIsConstant(Cudd_Regular(Isub0))) &&
	(Cudd_Regular(Isub0)->index != zdd_Isub0->index / 2 ||
	dd->permZ[index * 2] > dd->permZ[zdd_Isub0->index])) {
	printf("*** ERROR : illegal permutation in ZDD. ***\n");
    }
    */
    Cudd_Ref(Isub0);
    Cudd_Ref(zdd_Isub0);
    Isub1 = cuddZddIsop(dd, Lsub1, Usub1, &zdd_Isub1);
    if (Isub1 == NULL) {
	Cudd_RecursiveDeref(dd, Lsub0);
	Cudd_RecursiveDeref(dd, Lsub1);
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	return(NULL);
    }
    /*
    if ((!cuddIsConstant(Cudd_Regular(Isub1))) &&
	(Cudd_Regular(Isub1)->index != zdd_Isub1->index / 2 ||
	dd->permZ[index * 2] > dd->permZ[zdd_Isub1->index])) {
	printf("*** ERROR : illegal permutation in ZDD. ***\n");
    }
    */
    Cudd_Ref(Isub1);
    Cudd_Ref(zdd_Isub1);
    Cudd_RecursiveDeref(dd, Lsub0);
    Cudd_RecursiveDeref(dd, Lsub1);

    Lsuper0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Isub0));
    if (Lsuper0 == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	return(NULL);
    }
    Cudd_Ref(Lsuper0);
    Lsuper1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Isub1));
    if (Lsuper1 == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Lsuper0);
	return(NULL);
    }
    Cudd_Ref(Lsuper1);
    Usuper0 = Unv;
    Usuper1 = Uv;

    /* Ld = Lsuper0 + Lsuper1 */
    Ld = cuddBddAndRecur(dd, Cudd_Not(Lsuper0), Cudd_Not(Lsuper1));
    if (Ld == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Lsuper0);
	Cudd_RecursiveDeref(dd, Lsuper1);
	return(NULL);
    }
    Ld = Cudd_Not(Ld);
    Cudd_Ref(Ld);
    /* Ud = Usuper0 * Usuper1 */
    Ud = cuddBddAndRecur(dd, Usuper0, Usuper1);
    if (Ud == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Lsuper0);
	Cudd_RecursiveDeref(dd, Lsuper1);
	Cudd_RecursiveDeref(dd, Ld);
	return(NULL);
    }
    Cudd_Ref(Ud);
    Cudd_RecursiveDeref(dd, Lsuper0);
    Cudd_RecursiveDeref(dd, Lsuper1);

    Id = cuddZddIsop(dd, Ld, Ud, &zdd_Id);
    if (Id == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Ld);
	Cudd_RecursiveDeref(dd, Ud);
	return(NULL);
    }
    /*
    if ((!cuddIsConstant(Cudd_Regular(Id))) &&
	(Cudd_Regular(Id)->index != zdd_Id->index / 2 ||
	dd->permZ[index * 2] > dd->permZ[zdd_Id->index])) {
	printf("*** ERROR : illegal permutation in ZDD. ***\n");
    }
    */
    Cudd_Ref(Id);
    Cudd_Ref(zdd_Id);
    Cudd_RecursiveDeref(dd, Ld);
    Cudd_RecursiveDeref(dd, Ud);

    x = cuddUniqueInter(dd, index, one, zero);
    if (x == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDerefZdd(dd, zdd_Id);
	return(NULL);
    }
    Cudd_Ref(x);
    /* term0 = x * Isub0 */
    term0 = cuddBddAndRecur(dd, Cudd_Not(x), Isub0);
    if (term0 == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDerefZdd(dd, zdd_Id);
	Cudd_RecursiveDeref(dd, x);
	return(NULL);
    }
    Cudd_Ref(term0);
    Cudd_RecursiveDeref(dd, Isub0);
    /* term1 = x * Isub1 */
    term1 = cuddBddAndRecur(dd, x, Isub1);
    if (term1 == NULL) {
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDerefZdd(dd, zdd_Id);
	Cudd_RecursiveDeref(dd, x);
	Cudd_RecursiveDeref(dd, term0);
	return(NULL);
    }
    Cudd_Ref(term1);
    Cudd_RecursiveDeref(dd, x);
    Cudd_RecursiveDeref(dd, Isub1);
    /* sum = term0 + term1 */
    sum = cuddBddAndRecur(dd, Cudd_Not(term0), Cudd_Not(term1));
    if (sum == NULL) {
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDerefZdd(dd, zdd_Id);
	Cudd_RecursiveDeref(dd, term0);
	Cudd_RecursiveDeref(dd, term1);
	return(NULL);
    }
    sum = Cudd_Not(sum);
    Cudd_Ref(sum);
    Cudd_RecursiveDeref(dd, term0);
    Cudd_RecursiveDeref(dd, term1);
    /* r = sum + Id */
    r = cuddBddAndRecur(dd, Cudd_Not(sum), Cudd_Not(Id));
    r = Cudd_NotCond(r, r != NULL);
    if (r == NULL) {
	Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDerefZdd(dd, zdd_Id);
	Cudd_RecursiveDeref(dd, sum);
	return(NULL);
    }
    Cudd_Ref(r);
    Cudd_RecursiveDeref(dd, sum);
    Cudd_RecursiveDeref(dd, Id);

    if (zdd_Isub0 != zdd_zero) {
	z = cuddZddGetNodeIVO(dd, index * 2 + 1, zdd_Isub0, zdd_Id);
	if (z == NULL) {
	    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	    Cudd_RecursiveDerefZdd(dd, zdd_Id);
	    Cudd_RecursiveDeref(dd, r);
	    return(NULL);
	}
    }
    else {
	z = zdd_Id;
    }
    Cudd_Ref(z);
    if (zdd_Isub1 != zdd_zero) {
	y = cuddZddGetNodeIVO(dd, index * 2, zdd_Isub1, z);
	if (y == NULL) {
	    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
	    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
	    Cudd_RecursiveDerefZdd(dd, zdd_Id);
	    Cudd_RecursiveDeref(dd, r);
	    Cudd_RecursiveDerefZdd(dd, z);
	    return(NULL);
	}
    }
    else
	y = z;
    Cudd_Ref(y);

    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Id);
    Cudd_RecursiveDerefZdd(dd, z);

    cuddCacheInsert2(dd, cuddBddIsop, L, U, r);
    cuddCacheInsert2(dd, cacheOp, L, U, y);

    Cudd_Deref(r);
    Cudd_Deref(y);
    *zdd_I = y;
    /*
    if (Cudd_Regular(r)->index != y->index / 2) {
	printf("*** ERROR : mismatch in indices between BDD and ZDD. ***\n");
    }
    */
    return(r);

} /* end of cuddZddIsop */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_bddIsop.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_bddIsop]

******************************************************************************/
DdNode	*
cuddBddIsop(
  DdManager * dd,
  DdNode * L,
  DdNode * U)
{
    DdNode	*one = DD_ONE(dd);
    DdNode	*zero = Cudd_Not(one);
    int		v, top_l, top_u;
    DdNode	*Lsub0, *Usub0, *Lsub1, *Usub1, *Ld, *Ud;
    DdNode	*Lsuper0, *Usuper0, *Lsuper1, *Usuper1;
    DdNode	*Isub0, *Isub1, *Id;
    DdNode	*x;
    DdNode	*term0, *term1, *sum;
    DdNode	*Lv, *Uv, *Lnv, *Unv;
    DdNode	*r;
    int		index;

    statLine(dd);
    if (L == zero)
	return(zero);
    if (U == one)
	return(one);

    /* Check cache */
    r = cuddCacheLookup2(dd, cuddBddIsop, L, U);
    if (r)
	return(r);

    top_l = dd->perm[Cudd_Regular(L)->index];
    top_u = dd->perm[Cudd_Regular(U)->index];
    v = ddMin(top_l, top_u);

    /* Compute cofactors */
    if (top_l == v) {
	index = Cudd_Regular(L)->index;
	Lv = Cudd_T(L);
	Lnv = Cudd_E(L);
	if (Cudd_IsComplement(L)) {
	    Lv = Cudd_Not(Lv);
	    Lnv = Cudd_Not(Lnv);
	}
    }
    else {
	index = Cudd_Regular(U)->index;
	Lv = Lnv = L;
    }

    if (top_u == v) {
	Uv = Cudd_T(U);
	Unv = Cudd_E(U);
	if (Cudd_IsComplement(U)) {
	    Uv = Cudd_Not(Uv);
	    Unv = Cudd_Not(Unv);
	}
    }
    else {
	Uv = Unv = U;
    }

    Lsub0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Uv));
    if (Lsub0 == NULL)
	return(NULL);
    Cudd_Ref(Lsub0);
    Usub0 = Unv;
    Lsub1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Unv));
    if (Lsub1 == NULL) {
	Cudd_RecursiveDeref(dd, Lsub0);
	return(NULL);
    }
    Cudd_Ref(Lsub1);
    Usub1 = Uv;

    Isub0 = cuddBddIsop(dd, Lsub0, Usub0);
    if (Isub0 == NULL) {
	Cudd_RecursiveDeref(dd, Lsub0);
	Cudd_RecursiveDeref(dd, Lsub1);
	return(NULL);
    }
    Cudd_Ref(Isub0);
    Isub1 = cuddBddIsop(dd, Lsub1, Usub1);
    if (Isub1 == NULL) {
	Cudd_RecursiveDeref(dd, Lsub0);
	Cudd_RecursiveDeref(dd, Lsub1);
	Cudd_RecursiveDeref(dd, Isub0);
	return(NULL);
    }
    Cudd_Ref(Isub1);
    Cudd_RecursiveDeref(dd, Lsub0);
    Cudd_RecursiveDeref(dd, Lsub1);

    Lsuper0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Isub0));
    if (Lsuper0 == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	return(NULL);
    }
    Cudd_Ref(Lsuper0);
    Lsuper1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Isub1));
    if (Lsuper1 == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Lsuper0);
	return(NULL);
    }
    Cudd_Ref(Lsuper1);
    Usuper0 = Unv;
    Usuper1 = Uv;

    /* Ld = Lsuper0 + Lsuper1 */
    Ld = cuddBddAndRecur(dd, Cudd_Not(Lsuper0), Cudd_Not(Lsuper1));
    Ld = Cudd_NotCond(Ld, Ld != NULL);
    if (Ld == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Lsuper0);
	Cudd_RecursiveDeref(dd, Lsuper1);
	return(NULL);
    }
    Cudd_Ref(Ld);
    Ud = cuddBddAndRecur(dd, Usuper0, Usuper1);
    if (Ud == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Lsuper0);
	Cudd_RecursiveDeref(dd, Lsuper1);
	Cudd_RecursiveDeref(dd, Ld);
	return(NULL);
    }
    Cudd_Ref(Ud);
    Cudd_RecursiveDeref(dd, Lsuper0);
    Cudd_RecursiveDeref(dd, Lsuper1);

    Id = cuddBddIsop(dd, Ld, Ud);
    if (Id == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Ld);
	Cudd_RecursiveDeref(dd, Ud);
	return(NULL);
    }
    Cudd_Ref(Id);
    Cudd_RecursiveDeref(dd, Ld);
    Cudd_RecursiveDeref(dd, Ud);

    x = cuddUniqueInter(dd, index, one, zero);
    if (x == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Id);
	return(NULL);
    }
    Cudd_Ref(x);
    term0 = cuddBddAndRecur(dd, Cudd_Not(x), Isub0);
    if (term0 == NULL) {
	Cudd_RecursiveDeref(dd, Isub0);
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDeref(dd, x);
	return(NULL);
    }
    Cudd_Ref(term0);
    Cudd_RecursiveDeref(dd, Isub0);
    term1 = cuddBddAndRecur(dd, x, Isub1);
    if (term1 == NULL) {
	Cudd_RecursiveDeref(dd, Isub1);
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDeref(dd, x);
	Cudd_RecursiveDeref(dd, term0);
	return(NULL);
    }
    Cudd_Ref(term1);
    Cudd_RecursiveDeref(dd, x);
    Cudd_RecursiveDeref(dd, Isub1);
    /* sum = term0 + term1 */
    sum = cuddBddAndRecur(dd, Cudd_Not(term0), Cudd_Not(term1));
    sum = Cudd_NotCond(sum, sum != NULL);
    if (sum == NULL) {
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDeref(dd, term0);
	Cudd_RecursiveDeref(dd, term1);
	return(NULL);
    }
    Cudd_Ref(sum);
    Cudd_RecursiveDeref(dd, term0);
    Cudd_RecursiveDeref(dd, term1);
    /* r = sum + Id */
    r = cuddBddAndRecur(dd, Cudd_Not(sum), Cudd_Not(Id));
    r = Cudd_NotCond(r, r != NULL);
    if (r == NULL) {
	Cudd_RecursiveDeref(dd, Id);
	Cudd_RecursiveDeref(dd, sum);
	return(NULL);
    }
    Cudd_Ref(r);
    Cudd_RecursiveDeref(dd, sum);
    Cudd_RecursiveDeref(dd, Id);

    cuddCacheInsert2(dd, cuddBddIsop, L, U, r);

    Cudd_Deref(r);
    return(r);

} /* end of cuddBddIsop */


/**Function********************************************************************

  Synopsis [Converts a ZDD cover to a BDD.]

  Description [Converts a ZDD cover to a BDD. If successful, it returns
  a BDD node, otherwise it returns NULL. It is a recursive algorithm
  that works as follows. First it computes 3 cofactors of a ZDD cover:
  f1, f0 and fd. Second, it compute BDDs (b1, b0 and bd) of f1, f0 and fd.
  Third, it computes T=b1+bd and E=b0+bd. Fourth, it computes ITE(v,T,E) where
  v is the variable which has the index of the top node of the ZDD cover.
  In this case, since the index of v can be larger than either the one of T
  or the one of E, cuddUniqueInterIVO is called, where IVO stands for
  independent from variable ordering.]

  SideEffects []

  SeeAlso     [Cudd_MakeBddFromZddCover]

******************************************************************************/
DdNode	*
cuddMakeBddFromZddCover(
  DdManager * dd,
  DdNode * node)
{
    DdNode	*neW;
    int		v;
    DdNode	*f1, *f0, *fd;
    DdNode	*b1, *b0, *bd;
    DdNode	*T, *E;

    statLine(dd);
    if (node == dd->one)
	return(dd->one);
    if (node == dd->zero)
	return(Cudd_Not(dd->one));

    /* Check cache */
    neW = cuddCacheLookup1(dd, cuddMakeBddFromZddCover, node);
    if (neW)
	return(neW);

    v = Cudd_Regular(node)->index;	/* either yi or zi */
    if (cuddZddGetCofactors3(dd, node, v, &f1, &f0, &fd)) return(NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    Cudd_Ref(fd);

    b1 = cuddMakeBddFromZddCover(dd, f1);
    if (!b1) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	return(NULL);
    }
    Cudd_Ref(b1);
    b0 = cuddMakeBddFromZddCover(dd, f0);
    if (!b0) {
	Cudd_RecursiveDerefZdd(dd, f1);
	Cudd_RecursiveDerefZdd(dd, f0);
	Cudd_RecursiveDerefZdd(dd, fd);
	Cudd_RecursiveDeref(dd, b1);
	return(NULL);
    }
    Cudd_Ref(b0);
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    if (fd != dd->zero) {
	bd = cuddMakeBddFromZddCover(dd, fd);
	if (!bd) {
	    Cudd_RecursiveDerefZdd(dd, fd);
	    Cudd_RecursiveDeref(dd, b1);
	    Cudd_RecursiveDeref(dd, b0);
	    return(NULL);
	}
	Cudd_Ref(bd);
	Cudd_RecursiveDerefZdd(dd, fd);

	T = cuddBddAndRecur(dd, Cudd_Not(b1), Cudd_Not(bd));
	if (!T) {
	    Cudd_RecursiveDeref(dd, b1);
	    Cudd_RecursiveDeref(dd, b0);
	    Cudd_RecursiveDeref(dd, bd);
	    return(NULL);
	}
	T = Cudd_NotCond(T, T != NULL);
	Cudd_Ref(T);
	Cudd_RecursiveDeref(dd, b1);
	E = cuddBddAndRecur(dd, Cudd_Not(b0), Cudd_Not(bd));
	if (!E) {
	    Cudd_RecursiveDeref(dd, b0);
	    Cudd_RecursiveDeref(dd, bd);
	    Cudd_RecursiveDeref(dd, T);
	    return(NULL);
	}
	E = Cudd_NotCond(E, E != NULL);
	Cudd_Ref(E);
	Cudd_RecursiveDeref(dd, b0);
	Cudd_RecursiveDeref(dd, bd);
    }
    else {
	Cudd_RecursiveDerefZdd(dd, fd);
	T = b1;
	E = b0;
    }

    if (Cudd_IsComplement(T)) {
	neW = cuddUniqueInterIVO(dd, v / 2, Cudd_Not(T), Cudd_Not(E));
	if (!neW) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
	neW = Cudd_Not(neW);
    }
    else {
	neW = cuddUniqueInterIVO(dd, v / 2, T, E);
	if (!neW) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
    }
    Cudd_Ref(neW);
    Cudd_RecursiveDeref(dd, T);
    Cudd_RecursiveDeref(dd, E);

    cuddCacheInsert1(dd, cuddMakeBddFromZddCover, node, neW);
    Cudd_Deref(neW);
    return(neW);

} /* end of cuddMakeBddFromZddCover */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
