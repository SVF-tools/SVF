/**CFile***********************************************************************

  FileName    [cuddAddFind.c]

  PackageName [cudd]

  Synopsis    [Functions to find maximum and minimum in an ADD and to
  extract the i-th bit.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_addFindMax()
		<li> Cudd_addFindMin()
		<li> Cudd_addIthBit()
		</ul>
	       Static functions included in this module:
		<ul>
		<li> addDoIthBit()
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
static char rcsid[] DD_UNUSED = "$Id: cuddAddFind.c,v 1.9 2012/02/05 01:07:18 fabio Exp $";
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

static DdNode * addDoIthBit (DdManager *dd, DdNode *f, DdNode *index);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Finds the maximum discriminant of f.]

  Description [Returns a pointer to a constant ADD.]

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_addFindMax(
  DdManager * dd,
  DdNode * f)
{
    DdNode *t, *e, *res;

    statLine(dd);
    if (cuddIsConstant(f)) {
	return(f);
    }

    res = cuddCacheLookup1(dd,Cudd_addFindMax,f);
    if (res != NULL) {
	return(res);
    }

    t  = Cudd_addFindMax(dd,cuddT(f));
    if (t == DD_PLUS_INFINITY(dd)) return(t);

    e  = Cudd_addFindMax(dd,cuddE(f));

    res = (cuddV(t) >= cuddV(e)) ? t : e;

    cuddCacheInsert1(dd,Cudd_addFindMax,f,res);

    return(res);

} /* end of Cudd_addFindMax */


/**Function********************************************************************

  Synopsis    [Finds the minimum discriminant of f.]

  Description [Returns a pointer to a constant ADD.]

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_addFindMin(
  DdManager * dd,
  DdNode * f)
{
    DdNode *t, *e, *res;

    statLine(dd);
    if (cuddIsConstant(f)) {
	return(f);
    }

    res = cuddCacheLookup1(dd,Cudd_addFindMin,f);
    if (res != NULL) {
	return(res);
    }

    t  = Cudd_addFindMin(dd,cuddT(f));
    if (t == DD_MINUS_INFINITY(dd)) return(t);

    e  = Cudd_addFindMin(dd,cuddE(f));

    res = (cuddV(t) <= cuddV(e)) ? t : e;

    cuddCacheInsert1(dd,Cudd_addFindMin,f,res);

    return(res);

} /* end of Cudd_addFindMin */


/**Function********************************************************************

  Synopsis    [Extracts the i-th bit from an ADD.]

  Description [Produces an ADD from another ADD by replacing all
  discriminants whose i-th bit is equal to 1 with 1, and all other
  discriminants with 0. The i-th bit refers to the integer
  representation of the leaf value. If the value is has a fractional
  part, it is ignored. Repeated calls to this procedure allow one to
  transform an integer-valued ADD into an array of ADDs, one for each
  bit of the leaf values. Returns a pointer to the resulting ADD if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addBddIthBit]

******************************************************************************/
DdNode *
Cudd_addIthBit(
  DdManager * dd,
  DdNode * f,
  int  bit)
{
    DdNode *res;
    DdNode *index;
    
    /* Use a constant node to remember the bit, so that we can use the
    ** global cache.
    */
    index = cuddUniqueConst(dd,(CUDD_VALUE_TYPE) bit);
    if (index == NULL) return(NULL);
    cuddRef(index);

    do {
	dd->reordered = 0;
	res = addDoIthBit(dd, f, index);
    } while (dd->reordered == 1);

    if (res == NULL) {
	Cudd_RecursiveDeref(dd, index);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, index);
    cuddDeref(res);
    return(res);

} /* end of Cudd_addIthBit */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_addIthBit.]

  Description [Performs the recursive step for Cudd_addIthBit.
  Returns a pointer to the BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
addDoIthBit(
  DdManager * dd,
  DdNode * f,
  DdNode * index)
{
    DdNode *res, *T, *E;
    DdNode *fv, *fvn;
    int mask, value;
    int v;

    statLine(dd);
    /* Check terminal case. */
    if (cuddIsConstant(f)) {
	mask = 1 << ((int) cuddV(index));
	value = (int) cuddV(f);
	return((value & mask) == 0 ? DD_ZERO(dd) : DD_ONE(dd));
    }

    /* Check cache. */
    res = cuddCacheLookup2(dd,addDoIthBit,f,index);
    if (res != NULL) return(res);

    /* Recursive step. */
    v = f->index;
    fv = cuddT(f); fvn = cuddE(f);

    T = addDoIthBit(dd,fv,index);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = addDoIthBit(dd,fvn,index);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);

    res = (T == E) ? T : cuddUniqueInter(dd,v,T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, T);
	Cudd_RecursiveDeref(dd, E);
	return(NULL);
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert2(dd,addDoIthBit,f,index,res);

    return(res);

} /* end of addDoIthBit */

