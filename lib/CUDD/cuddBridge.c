/**CFile***********************************************************************

  FileName    [cuddBridge.c]

  PackageName [cudd]

  Synopsis    [Translation from BDD to ADD and vice versa and transfer between
  different managers.]

  Description [External procedures included in this file:
	    <ul>
	    <li> Cudd_addBddThreshold()
	    <li> Cudd_addBddStrictThreshold()
	    <li> Cudd_addBddInterval()
	    <li> Cudd_addBddIthBit()
	    <li> Cudd_BddToAdd()
	    <li> Cudd_addBddPattern()
	    <li> Cudd_bddTransfer()
	    </ul>
	Internal procedures included in this file:
	    <ul>
	    <li> cuddBddTransfer()
	    <li> cuddAddBddDoPattern()
	    </ul>
	Static procedures included in this file:
	    <ul>
	    <li> addBddDoThreshold()
	    <li> addBddDoStrictThreshold()
	    <li> addBddDoInterval()
	    <li> addBddDoIthBit()
	    <li> ddBddToAddRecur()
	    <li> cuddBddTransferRecur()
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
static char rcsid[] DD_UNUSED = "$Id: cuddBridge.c,v 1.20 2012/02/05 01:07:18 fabio Exp $";
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

static DdNode * addBddDoThreshold (DdManager *dd, DdNode *f, DdNode *val);
static DdNode * addBddDoStrictThreshold (DdManager *dd, DdNode *f, DdNode *val);
static DdNode * addBddDoInterval (DdManager *dd, DdNode *f, DdNode *l, DdNode *u);
static DdNode * addBddDoIthBit (DdManager *dd, DdNode *f, DdNode *index);
static DdNode * ddBddToAddRecur (DdManager *dd, DdNode *B);
static DdNode * cuddBddTransferRecur (DdManager *ddS, DdManager *ddD, DdNode *f, st_table *table);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Converts an ADD to a BDD.]

  Description [Converts an ADD to a BDD by replacing all
  discriminants greater than or equal to value with 1, and all other
  discriminants with 0. Returns a pointer to the resulting BDD if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addBddInterval Cudd_addBddPattern Cudd_BddToAdd
  Cudd_addBddStrictThreshold]

******************************************************************************/
DdNode *
Cudd_addBddThreshold(
  DdManager * dd,
  DdNode * f,
  CUDD_VALUE_TYPE  value)
{
    DdNode *res;
    DdNode *val;
    
    val = cuddUniqueConst(dd,value);
    if (val == NULL) return(NULL);
    cuddRef(val);

    do {
	dd->reordered = 0;
	res = addBddDoThreshold(dd, f, val);
    } while (dd->reordered == 1);

    if (res == NULL) {
	Cudd_RecursiveDeref(dd, val);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, val);
    cuddDeref(res);
    return(res);

} /* end of Cudd_addBddThreshold */


/**Function********************************************************************

  Synopsis    [Converts an ADD to a BDD.]

  Description [Converts an ADD to a BDD by replacing all
  discriminants STRICTLY greater than value with 1, and all other
  discriminants with 0. Returns a pointer to the resulting BDD if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addBddInterval Cudd_addBddPattern Cudd_BddToAdd 
  Cudd_addBddThreshold]

******************************************************************************/
DdNode *
Cudd_addBddStrictThreshold(
  DdManager * dd,
  DdNode * f,
  CUDD_VALUE_TYPE  value)
{
    DdNode *res;
    DdNode *val;
    
    val = cuddUniqueConst(dd,value);
    if (val == NULL) return(NULL);
    cuddRef(val);

    do {
	dd->reordered = 0;
	res = addBddDoStrictThreshold(dd, f, val);
    } while (dd->reordered == 1);

    if (res == NULL) {
	Cudd_RecursiveDeref(dd, val);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, val);
    cuddDeref(res);
    return(res);

} /* end of Cudd_addBddStrictThreshold */


/**Function********************************************************************

  Synopsis    [Converts an ADD to a BDD.]

  Description [Converts an ADD to a BDD by replacing all
  discriminants greater than or equal to lower and less than or equal to
  upper with 1, and all other discriminants with 0. Returns a pointer to
  the resulting BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addBddThreshold Cudd_addBddStrictThreshold 
  Cudd_addBddPattern Cudd_BddToAdd]

******************************************************************************/
DdNode *
Cudd_addBddInterval(
  DdManager * dd,
  DdNode * f,
  CUDD_VALUE_TYPE  lower,
  CUDD_VALUE_TYPE  upper)
{
    DdNode *res;
    DdNode *l;
    DdNode *u;
    
    /* Create constant nodes for the interval bounds, so that we can use
    ** the global cache.
    */
    l = cuddUniqueConst(dd,lower);
    if (l == NULL) return(NULL);
    cuddRef(l);
    u = cuddUniqueConst(dd,upper);
    if (u == NULL) {
	Cudd_RecursiveDeref(dd,l);
	return(NULL);
    }
    cuddRef(u);

    do {
	dd->reordered = 0;
	res = addBddDoInterval(dd, f, l, u);
    } while (dd->reordered == 1);

    if (res == NULL) {
	Cudd_RecursiveDeref(dd, l);
	Cudd_RecursiveDeref(dd, u);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, l);
    Cudd_RecursiveDeref(dd, u);
    cuddDeref(res);
    return(res);

} /* end of Cudd_addBddInterval */


/**Function********************************************************************

  Synopsis    [Converts an ADD to a BDD by extracting the i-th bit from
  the leaves.]

  Description [Converts an ADD to a BDD by replacing all
  discriminants whose i-th bit is equal to 1 with 1, and all other
  discriminants with 0. The i-th bit refers to the integer
  representation of the leaf value. If the value is has a fractional
  part, it is ignored. Repeated calls to this procedure allow one to
  transform an integer-valued ADD into an array of BDDs, one for each
  bit of the leaf values. Returns a pointer to the resulting BDD if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addBddInterval Cudd_addBddPattern Cudd_BddToAdd]

******************************************************************************/
DdNode *
Cudd_addBddIthBit(
  DdManager * dd,
  DdNode * f,
  int  bit)
{
    DdNode *res;
    DdNode *index;
    
    index = cuddUniqueConst(dd,(CUDD_VALUE_TYPE) bit);
    if (index == NULL) return(NULL);
    cuddRef(index);

    do {
	dd->reordered = 0;
	res = addBddDoIthBit(dd, f, index);
    } while (dd->reordered == 1);

    if (res == NULL) {
	Cudd_RecursiveDeref(dd, index);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, index);
    cuddDeref(res);
    return(res);

} /* end of Cudd_addBddIthBit */


/**Function********************************************************************

  Synopsis    [Converts a BDD to a 0-1 ADD.]

  Description [Converts a BDD to a 0-1 ADD. Returns a pointer to the
  resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addBddPattern Cudd_addBddThreshold Cudd_addBddInterval
  Cudd_addBddStrictThreshold]

******************************************************************************/
DdNode *
Cudd_BddToAdd(
  DdManager * dd,
  DdNode * B)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = ddBddToAddRecur(dd, B);
    } while (dd->reordered ==1);
    return(res);

} /* end of Cudd_BddToAdd */


/**Function********************************************************************

  Synopsis    [Converts an ADD to a BDD.]

  Description [Converts an ADD to a BDD by replacing all
  discriminants different from 0 with 1. Returns a pointer to the
  resulting BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_BddToAdd Cudd_addBddThreshold Cudd_addBddInterval
  Cudd_addBddStrictThreshold]

******************************************************************************/
DdNode *
Cudd_addBddPattern(
  DdManager * dd,
  DdNode * f)
{
    DdNode *res;
    
    do {
	dd->reordered = 0;
	res = cuddAddBddDoPattern(dd, f);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addBddPattern */


/**Function********************************************************************

  Synopsis    [Convert a BDD from a manager to another one.]

  Description [Convert a BDD from a manager to another one. The orders of the
  variables in the two managers may be different. Returns a
  pointer to the BDD in the destination manager if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_bddTransfer(
  DdManager * ddSource,
  DdManager * ddDestination,
  DdNode * f)
{
    DdNode *res;
    do {
	ddDestination->reordered = 0;
	res = cuddBddTransfer(ddSource, ddDestination, f);
    } while (ddDestination->reordered == 1);
    return(res);

} /* end of Cudd_bddTransfer */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Convert a BDD from a manager to another one.]

  Description [Convert a BDD from a manager to another one. Returns a
  pointer to the BDD in the destination manager if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddTransfer]

******************************************************************************/
DdNode *
cuddBddTransfer(
  DdManager * ddS,
  DdManager * ddD,
  DdNode * f)
{
    DdNode *res;
    st_table *table = NULL;
    st_generator *gen = NULL;
    DdNode *key, *value;

    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) goto failure;
    res = cuddBddTransferRecur(ddS, ddD, f, table);
    if (res != NULL) cuddRef(res);

    /* Dereference all elements in the table and dispose of the table.
    ** This must be done also if res is NULL to avoid leaks in case of
    ** reordering. */
    gen = st_init_gen(table);
    if (gen == NULL) goto failure;
    while (st_gen(gen, &key, &value)) {
	Cudd_RecursiveDeref(ddD, value);
    }
    st_free_gen(gen); gen = NULL;
    st_free_table(table); table = NULL;

    if (res != NULL) cuddDeref(res);
    return(res);

failure:
    /* No need to free gen because it is always NULL here. */
    if (table != NULL) st_free_table(table);
    return(NULL);

} /* end of cuddBddTransfer */


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_addBddPattern.]

  Description [Performs the recursive step for Cudd_addBddPattern. Returns a
  pointer to the resulting BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddAddBddDoPattern(
  DdManager * dd,
  DdNode * f)
{
    DdNode *res, *T, *E;
    DdNode *fv, *fvn;
    int v;

    statLine(dd);
    /* Check terminal case. */
    if (cuddIsConstant(f)) {
	return(Cudd_NotCond(DD_ONE(dd),f == DD_ZERO(dd)));
    }

    /* Check cache. */
    res = cuddCacheLookup1(dd,Cudd_addBddPattern,f);
    if (res != NULL) return(res);

    /* Recursive step. */
    v = f->index;
    fv = cuddT(f); fvn = cuddE(f);

    T = cuddAddBddDoPattern(dd,fv);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = cuddAddBddDoPattern(dd,fvn);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);
    if (Cudd_IsComplement(T)) {
	res = (T == E) ? Cudd_Not(T) : cuddUniqueInter(dd,v,Cudd_Not(T),Cudd_Not(E));
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
	res = Cudd_Not(res);
    } else {
	res = (T == E) ? T : cuddUniqueInter(dd,v,T,E);
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert1(dd,Cudd_addBddPattern,f,res);

    return(res);

} /* end of cuddAddBddDoPattern */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_addBddThreshold.]

  Description [Performs the recursive step for Cudd_addBddThreshold.
  Returns a pointer to the BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [addBddDoStrictThreshold]

******************************************************************************/
static DdNode *
addBddDoThreshold(
  DdManager * dd,
  DdNode * f,
  DdNode * val)
{
    DdNode *res, *T, *E;
    DdNode *fv, *fvn;
    int v;

    statLine(dd);
    /* Check terminal case. */
    if (cuddIsConstant(f)) {
	return(Cudd_NotCond(DD_ONE(dd),cuddV(f) < cuddV(val)));
    }

    /* Check cache. */
    res = cuddCacheLookup2(dd,addBddDoThreshold,f,val);
    if (res != NULL) return(res);

    /* Recursive step. */
    v = f->index;
    fv = cuddT(f); fvn = cuddE(f);

    T = addBddDoThreshold(dd,fv,val);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = addBddDoThreshold(dd,fvn,val);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);
    if (Cudd_IsComplement(T)) {
	res = (T == E) ? Cudd_Not(T) : cuddUniqueInter(dd,v,Cudd_Not(T),Cudd_Not(E));
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
	res = Cudd_Not(res);
    } else {
	res = (T == E) ? T : cuddUniqueInter(dd,v,T,E);
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert2(dd,addBddDoThreshold,f,val,res);

    return(res);

} /* end of addBddDoThreshold */


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_addBddStrictThreshold.]

  Description [Performs the recursive step for Cudd_addBddStrictThreshold.
  Returns a pointer to the BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [addBddDoThreshold]

******************************************************************************/
static DdNode *
addBddDoStrictThreshold(
  DdManager * dd,
  DdNode * f,
  DdNode * val)
{
    DdNode *res, *T, *E;
    DdNode *fv, *fvn;
    int v;

    statLine(dd);
    /* Check terminal case. */
    if (cuddIsConstant(f)) {
	return(Cudd_NotCond(DD_ONE(dd),cuddV(f) <= cuddV(val)));
    }

    /* Check cache. */
    res = cuddCacheLookup2(dd,addBddDoStrictThreshold,f,val);
    if (res != NULL) return(res);

    /* Recursive step. */
    v = f->index;
    fv = cuddT(f); fvn = cuddE(f);

    T = addBddDoStrictThreshold(dd,fv,val);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = addBddDoStrictThreshold(dd,fvn,val);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);
    if (Cudd_IsComplement(T)) {
	res = (T == E) ? Cudd_Not(T) : cuddUniqueInter(dd,v,Cudd_Not(T),Cudd_Not(E));
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
	res = Cudd_Not(res);
    } else {
	res = (T == E) ? T : cuddUniqueInter(dd,v,T,E);
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert2(dd,addBddDoStrictThreshold,f,val,res);

    return(res);

} /* end of addBddDoStrictThreshold */


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_addBddInterval.]

  Description [Performs the recursive step for Cudd_addBddInterval.
  Returns a pointer to the BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [addBddDoThreshold addBddDoStrictThreshold]

******************************************************************************/
static DdNode *
addBddDoInterval(
  DdManager * dd,
  DdNode * f,
  DdNode * l,
  DdNode * u)
{
    DdNode *res, *T, *E;
    DdNode *fv, *fvn;
    int v;

    statLine(dd);
    /* Check terminal case. */
    if (cuddIsConstant(f)) {
	return(Cudd_NotCond(DD_ONE(dd),cuddV(f) < cuddV(l) || cuddV(f) > cuddV(u)));
    }

    /* Check cache. */
    res = cuddCacheLookup(dd,DD_ADD_BDD_DO_INTERVAL_TAG,f,l,u);
    if (res != NULL) return(res);

    /* Recursive step. */
    v = f->index;
    fv = cuddT(f); fvn = cuddE(f);

    T = addBddDoInterval(dd,fv,l,u);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = addBddDoInterval(dd,fvn,l,u);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);
    if (Cudd_IsComplement(T)) {
	res = (T == E) ? Cudd_Not(T) : cuddUniqueInter(dd,v,Cudd_Not(T),Cudd_Not(E));
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
	res = Cudd_Not(res);
    } else {
	res = (T == E) ? T : cuddUniqueInter(dd,v,T,E);
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert(dd,DD_ADD_BDD_DO_INTERVAL_TAG,f,l,u,res);

    return(res);

} /* end of addBddDoInterval */


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_addBddIthBit.]

  Description [Performs the recursive step for Cudd_addBddIthBit.
  Returns a pointer to the BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
addBddDoIthBit(
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
	return(Cudd_NotCond(DD_ONE(dd),(value & mask) == 0));
    }

    /* Check cache. */
    res = cuddCacheLookup2(dd,addBddDoIthBit,f,index);
    if (res != NULL) return(res);

    /* Recursive step. */
    v = f->index;
    fv = cuddT(f); fvn = cuddE(f);

    T = addBddDoIthBit(dd,fv,index);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = addBddDoIthBit(dd,fvn,index);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);
    if (Cudd_IsComplement(T)) {
	res = (T == E) ? Cudd_Not(T) : cuddUniqueInter(dd,v,Cudd_Not(T),Cudd_Not(E));
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
	res = Cudd_Not(res);
    } else {
	res = (T == E) ? T : cuddUniqueInter(dd,v,T,E);
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd, T);
	    Cudd_RecursiveDeref(dd, E);
	    return(NULL);
	}
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert2(dd,addBddDoIthBit,f,index,res);

    return(res);

} /* end of addBddDoIthBit */


/**Function********************************************************************

  Synopsis    [Performs the recursive step for Cudd_BddToAdd.]

  Description [Performs the recursive step for Cudd_BddToAdd. Returns a
  pointer to the resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
ddBddToAddRecur(
  DdManager * dd,
  DdNode * B)
{
    DdNode *one;
    DdNode *res, *res1, *T, *E, *Bt, *Be;
    int complement = 0;

    statLine(dd);
    one = DD_ONE(dd);

    if (Cudd_IsConstant(B)) {
	if (B == one) {
	    res = one;
	} else {
	    res = DD_ZERO(dd);
	}
	return(res);
    }
    /* Check visited table */
    res = cuddCacheLookup1(dd,ddBddToAddRecur,B);
    if (res != NULL) return(res);

    if (Cudd_IsComplement(B)) {
	complement = 1;
	Bt = cuddT(Cudd_Regular(B));
	Be = cuddE(Cudd_Regular(B));
    } else {
	Bt = cuddT(B);
	Be = cuddE(B);
    }

    T = ddBddToAddRecur(dd, Bt);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = ddBddToAddRecur(dd, Be);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);

    /* No need to check for T == E, because it is guaranteed not to happen. */
    res = cuddUniqueInter(dd, (int) Cudd_Regular(B)->index, T, E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd ,T);
	Cudd_RecursiveDeref(dd ,E);
	return(NULL);
    }
    cuddDeref(T);
    cuddDeref(E);

    if (complement) {
	cuddRef(res);
	res1 = cuddAddCmplRecur(dd, res);
	if (res1 == NULL) {
	    Cudd_RecursiveDeref(dd, res);
	    return(NULL);
	}
	cuddRef(res1);
	Cudd_RecursiveDeref(dd, res);
	res = res1;
	cuddDeref(res);
    }

    /* Store result. */
    cuddCacheInsert1(dd,ddBddToAddRecur,B,res);

    return(res);

} /* end of ddBddToAddRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddTransfer.]

  Description [Performs the recursive step of Cudd_bddTransfer.
  Returns a pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddBddTransfer]

******************************************************************************/
static DdNode *
cuddBddTransferRecur(
  DdManager * ddS,
  DdManager * ddD,
  DdNode * f,
  st_table * table)
{
    DdNode *ft, *fe, *t, *e, *var, *res;
    DdNode *one, *zero;
    int	   index;
    int    comple = 0;

    statLine(ddD);
    one = DD_ONE(ddD);
    comple = Cudd_IsComplement(f);

    /* Trivial cases. */
    if (Cudd_IsConstant(f)) return(Cudd_NotCond(one, comple));

    /* Make canonical to increase the utilization of the cache. */
    f = Cudd_NotCond(f,comple);
    /* Now f is a regular pointer to a non-constant node. */

    /* Check the cache. */
    if (st_lookup(table, f, &res))
	return(Cudd_NotCond(res,comple));
    
    /* Recursive step. */
    index = f->index;
    ft = cuddT(f); fe = cuddE(f);

    t = cuddBddTransferRecur(ddS, ddD, ft, table);
    if (t == NULL) {
    	return(NULL);
    }
    cuddRef(t);

    e = cuddBddTransferRecur(ddS, ddD, fe, table);
    if (e == NULL) {
    	Cudd_RecursiveDeref(ddD, t);
    	return(NULL);
    }
    cuddRef(e);

    zero = Cudd_Not(one);
    var = cuddUniqueInter(ddD,index,one,zero);
    if (var == NULL) {
	Cudd_RecursiveDeref(ddD, t);
	Cudd_RecursiveDeref(ddD, e);
    	return(NULL);
    }
    res = cuddBddIteRecur(ddD,var,t,e);
    if (res == NULL) {
	Cudd_RecursiveDeref(ddD, t);
	Cudd_RecursiveDeref(ddD, e);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(ddD, t);
    Cudd_RecursiveDeref(ddD, e);

    if (st_add_direct(table, (char *) f, (char *) res) == ST_OUT_OF_MEM) {
	Cudd_RecursiveDeref(ddD, res);
	return(NULL);
    }
    return(Cudd_NotCond(res,comple));

} /* end of cuddBddTransferRecur */

