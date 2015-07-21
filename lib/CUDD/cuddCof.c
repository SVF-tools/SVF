/**CFile***********************************************************************

  FileName    [cuddCof.c]

  PackageName [cudd]

  Synopsis    [Cofactoring functions.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_Cofactor()
		<li> Cudd_CheckCube()
		</ul>
	       Internal procedures included in this module:
		<ul>
		<li> cuddGetBranches()
		<li> cuddCofactorRecur()
		</ul>
	      ]

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
static char rcsid[] DD_UNUSED = "$Id: cuddCof.c,v 1.11 2012/02/05 01:07:18 fabio Exp $";
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

  Synopsis    [Computes the cofactor of f with respect to g.]

  Description [Computes the cofactor of f with respect to g; g must be
  the BDD or the ADD of a cube. Returns a pointer to the cofactor if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddConstrain Cudd_bddRestrict]

******************************************************************************/
DdNode *
Cudd_Cofactor(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res,*zero;

    zero = Cudd_Not(DD_ONE(dd));
    if (g == zero || g == DD_ZERO(dd)) {
	(void) fprintf(dd->err,"Cudd_Cofactor: Invalid restriction 1\n");
	dd->errorCode = CUDD_INVALID_ARG;
	return(NULL);
    }
    do {
	dd->reordered = 0;
	res = cuddCofactorRecur(dd,f,g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_Cofactor */


/**Function********************************************************************

  Synopsis    [Checks whether g is the BDD of a cube.]

  Description [Checks whether g is the BDD of a cube. Returns 1 in case
  of success; 0 otherwise. The constant 1 is a valid cube, but all other
  constant functions cause cuddCheckCube to return 0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_CheckCube(
  DdManager * dd,
  DdNode * g)
{
    DdNode *g1,*g0,*one,*zero;
    
    one = DD_ONE(dd);
    if (g == one) return(1);
    if (Cudd_IsConstant(g)) return(0);

    zero = Cudd_Not(one);
    cuddGetBranches(g,&g1,&g0);

    if (g0 == zero) {
        return(Cudd_CheckCube(dd, g1));
    }
    if (g1 == zero) {
        return(Cudd_CheckCube(dd, g0));
    }
    return(0);

} /* end of Cudd_CheckCube */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Computes the children of g.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddGetBranches(
  DdNode * g,
  DdNode ** g1,
  DdNode ** g0)
{
    DdNode	*G = Cudd_Regular(g);

    *g1 = cuddT(G);
    *g0 = cuddE(G);
    if (Cudd_IsComplement(g)) {
	*g1 = Cudd_Not(*g1);
	*g0 = Cudd_Not(*g0);
    }

} /* end of cuddGetBranches */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_Cofactor.]

  Description [Performs the recursive step of Cudd_Cofactor. Returns a
  pointer to the cofactor if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Cofactor]

******************************************************************************/
DdNode *
cuddCofactorRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *one,*zero,*F,*G,*g1,*g0,*f1,*f0,*t,*e,*r;
    unsigned int topf,topg;
    int comple;

    statLine(dd);
    F = Cudd_Regular(f);
    if (cuddIsConstant(F)) return(f);

    one = DD_ONE(dd);

    /* The invariant g != 0 is true on entry to this procedure and is
    ** recursively maintained by it. Therefore it suffices to test g
    ** against one to make sure it is not constant.
    */
    if (g == one) return(f);
    /* From now on, f and g are known not to be constants. */

    comple = f != F;
    r = cuddCacheLookup2(dd,Cudd_Cofactor,F,g);
    if (r != NULL) {
	return(Cudd_NotCond(r,comple));
    }

    topf = dd->perm[F->index];
    G = Cudd_Regular(g);
    topg = dd->perm[G->index];

    /* We take the cofactors of F because we are going to rely on
    ** the fact that the cofactors of the complement are the complements
    ** of the cofactors to better utilize the cache. Variable comple
    ** remembers whether we have to complement the result or not.
    */
    if (topf <= topg) {
	f1 = cuddT(F); f0 = cuddE(F);
    } else {
	f1 = f0 = F;
    }
    if (topg <= topf) {
	g1 = cuddT(G); g0 = cuddE(G);
	if (g != G) { g1 = Cudd_Not(g1); g0 = Cudd_Not(g0); }
    } else {
	g1 = g0 = g;
    }

    zero = Cudd_Not(one);
    if (topf >= topg) {
	if (g0 == zero || g0 == DD_ZERO(dd)) {
	    r = cuddCofactorRecur(dd, f1, g1);
	} else if (g1 == zero || g1 == DD_ZERO(dd)) {
	    r = cuddCofactorRecur(dd, f0, g0);
	} else {
	    (void) fprintf(dd->out,
			   "Cudd_Cofactor: Invalid restriction 2\n");
	    dd->errorCode = CUDD_INVALID_ARG;
	    return(NULL);
	}
	if (r == NULL) return(NULL);
    } else /* if (topf < topg) */ {
	t = cuddCofactorRecur(dd, f1, g);
	if (t == NULL) return(NULL);
    	cuddRef(t);
    	e = cuddCofactorRecur(dd, f0, g);
	if (e == NULL) {
	    Cudd_RecursiveDeref(dd, t);
	    return(NULL);
	}
	cuddRef(e);

	if (t == e) {
	    r = t;
	} else if (Cudd_IsComplement(t)) {
	    r = cuddUniqueInter(dd,(int)F->index,Cudd_Not(t),Cudd_Not(e));
	    if (r != NULL)
		r = Cudd_Not(r);
	} else {
	    r = cuddUniqueInter(dd,(int)F->index,t,e);
	}
	if (r == NULL) {
	    Cudd_RecursiveDeref(dd ,e);
	    Cudd_RecursiveDeref(dd ,t);
	    return(NULL);
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert2(dd,Cudd_Cofactor,F,g,r);

    return(Cudd_NotCond(r,comple));

} /* end of cuddCofactorRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

