/**CFile***********************************************************************

  FileName    [cuddAddNeg.c]

  PackageName [cudd]

  Synopsis    [Function to compute the negation of an ADD.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_addNegate()
		<li> Cudd_addRoundOff()
		</ul>
	Internal procedures included in this module:
		<ul>
		<li> cuddAddNegateRecur()
		<li> cuddAddRoundOffRecur()
		</ul> ]

  Author      [Fabio Somenzi, Balakrishna Kumthekar]

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
static char rcsid[] DD_UNUSED = "$Id: cuddAddNeg.c,v 1.14 2012/02/05 01:07:18 fabio Exp $";
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

  Synopsis    [Computes the additive inverse of an ADD.]

  Description [Computes the additive inverse of an ADD. Returns a pointer
  to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addCmpl]

******************************************************************************/
DdNode *
Cudd_addNegate(
  DdManager * dd,
  DdNode * f)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddAddNegateRecur(dd,f);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addNegate */


/**Function********************************************************************

  Synopsis    [Rounds off the discriminants of an ADD.]

  Description [Rounds off the discriminants of an ADD. The discriminants are
  rounded off to N digits after the decimal. Returns a pointer to the result
  ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_addRoundOff(
  DdManager * dd,
  DdNode * f,
  int  N)
{
    DdNode *res;
    double trunc = pow(10.0,(double)N);

    do {
	dd->reordered = 0;
	res = cuddAddRoundOffRecur(dd,f,trunc);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addRoundOff */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_addNegate.]

  Description [Implements the recursive step of Cudd_addNegate.
  Returns a pointer to the result.]

  SideEffects [None]

******************************************************************************/
DdNode *
cuddAddNegateRecur(
  DdManager * dd,
  DdNode * f)
{
    DdNode *res,
	    *fv, *fvn,
	    *T, *E;

    statLine(dd);
    /* Check terminal cases. */
    if (cuddIsConstant(f)) {
	res = cuddUniqueConst(dd,-cuddV(f));
	return(res);
    }

    /* Check cache */
    res = cuddCacheLookup1(dd,Cudd_addNegate,f);
    if (res != NULL) return(res);

    /* Recursive Step */
    fv = cuddT(f);
    fvn = cuddE(f);
    T = cuddAddNegateRecur(dd,fv);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = cuddAddNegateRecur(dd,fvn);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd,T);
	return(NULL);
    }
    cuddRef(E);
    res = (T == E) ? T : cuddUniqueInter(dd,(int)f->index,T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, T);
	Cudd_RecursiveDeref(dd, E);
	return(NULL);
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert1(dd,Cudd_addNegate,f,res);

    return(res);

} /* end of cuddAddNegateRecur */


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_addRoundOff.]

  Description [Implements the recursive step of Cudd_addRoundOff.
  Returns a pointer to the result.]

  SideEffects [None]

******************************************************************************/
DdNode *
cuddAddRoundOffRecur(
  DdManager * dd,
  DdNode * f,
  double  trunc)
{

    DdNode *res, *fv, *fvn, *T, *E;
    double n;
    DD_CTFP1 cacheOp;

    statLine(dd);
    if (cuddIsConstant(f)) {
	n = ceil(cuddV(f)*trunc)/trunc;
	res = cuddUniqueConst(dd,n);
	return(res);
    }
    cacheOp = (DD_CTFP1) Cudd_addRoundOff;
    res = cuddCacheLookup1(dd,cacheOp,f);
    if (res != NULL) {
	return(res);
    }
    /* Recursive Step */
    fv = cuddT(f);
    fvn = cuddE(f);
    T = cuddAddRoundOffRecur(dd,fv,trunc);
    if (T == NULL) {
       return(NULL);
    }
    cuddRef(T);
    E = cuddAddRoundOffRecur(dd,fvn,trunc);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd,T);
	return(NULL);
    }
    cuddRef(E);
    res = (T == E) ? T : cuddUniqueInter(dd,(int)f->index,T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd,T);
	Cudd_RecursiveDeref(dd,E);
	return(NULL);
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert1(dd,cacheOp,f,res);
    return(res);

} /* end of cuddAddRoundOffRecur */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
