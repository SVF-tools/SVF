/**CFile***********************************************************************

  FileName    [cuddAddWalsh.c]

  PackageName [cudd]

  Synopsis    [Functions that generate Walsh matrices and residue
  functions in ADD form.]

  Description [External procedures included in this module:
	    <ul>
	    <li> Cudd_addWalsh()
	    <li> Cudd_addResidue()
	    </ul>
	Static procedures included in this module:
	    <ul>
	    <li> addWalshInt()
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
static char rcsid[] DD_UNUSED = "$Id: cuddAddWalsh.c,v 1.11 2012/02/05 01:07:18 fabio Exp $";
#endif


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdNode * addWalshInt (DdManager *dd, DdNode **x, DdNode **y, int n);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Generates a Walsh matrix in ADD form.]

  Description [Generates a Walsh matrix in ADD form. Returns a pointer
  to the matrixi if successful; NULL otherwise.]

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_addWalsh(
  DdManager * dd,
  DdNode ** x,
  DdNode ** y,
  int  n)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = addWalshInt(dd, x, y, n);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addWalsh */


/**Function********************************************************************

  Synopsis    [Builds an ADD for the residue modulo m of an n-bit
  number.]

  Description [Builds an ADD for the residue modulo m of an n-bit
  number. The modulus must be at least 2, and the number of bits at
  least 1. Parameter options specifies whether the MSB should be on top
  or the LSB; and whther the number whose residue is computed is in
  two's complement notation or not. The macro CUDD_RESIDUE_DEFAULT
  specifies LSB on top and unsigned number. The macro CUDD_RESIDUE_MSB
  specifies MSB on top, and the macro CUDD_RESIDUE_TC specifies two's
  complement residue. To request MSB on top and two's complement residue
  simultaneously, one can OR the two macros:
  CUDD_RESIDUE_MSB | CUDD_RESIDUE_TC.
  Cudd_addResidue returns a pointer to the resulting ADD if successful;
  NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_addResidue(
  DdManager * dd /* manager */,
  int  n /* number of bits */,
  int  m /* modulus */,
  int  options /* options */,
  int  top /* index of top variable */)
{
    int msbLsb;	/* MSB on top (1) or LSB on top (0) */
    int tc;	/* two's complement (1) or unsigned (0) */
    int i, j, k, t, residue, thisOne, previous, index;
    DdNode **array[2], *var, *tmp, *res;

    /* Sanity check. */
    if (n < 1 && m < 2) return(NULL);

    msbLsb = options & CUDD_RESIDUE_MSB;
    tc = options & CUDD_RESIDUE_TC;

    /* Allocate and initialize working arrays. */
    array[0] = ALLOC(DdNode *,m);
    if (array[0] == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    array[1] = ALLOC(DdNode *,m);
    if (array[1] == NULL) {
	FREE(array[0]);
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < m; i++) {
	array[0][i] = array[1][i] = NULL;
    }

    /* Initialize residues. */
    for (i = 0; i < m; i++) {
	tmp = cuddUniqueConst(dd,(CUDD_VALUE_TYPE) i);
	if (tmp == NULL) {
	    for (j = 0; j < i; j++) {
		Cudd_RecursiveDeref(dd,array[1][j]);
	    }
	    FREE(array[0]);
	    FREE(array[1]);
	    return(NULL);
	}
	cuddRef(tmp);
	array[1][i] = tmp;
    }

    /* Main iteration. */
    residue = 1;	/* residue of 2**0 */
    for (k = 0; k < n; k++) {
	/* Choose current and previous arrays. */
	thisOne = k & 1;
	previous = thisOne ^ 1;
	/* Build an ADD projection function. */
	if (msbLsb) {
	    index = top+n-k-1;
	} else {
	    index = top+k;
	}
	var = cuddUniqueInter(dd,index,DD_ONE(dd),DD_ZERO(dd));
	if (var == NULL) {
	    for (j = 0; j < m; j++) {
		Cudd_RecursiveDeref(dd,array[previous][j]);
	    }
	    FREE(array[0]);
	    FREE(array[1]);
	    return(NULL);
	}
	cuddRef(var);
	for (i = 0; i < m; i ++) {
	    t = (i + residue) % m;
	    tmp = Cudd_addIte(dd,var,array[previous][t],array[previous][i]);
	    if (tmp == NULL) {
		for (j = 0; j < i; j++) {
		    Cudd_RecursiveDeref(dd,array[thisOne][j]);
		}
		for (j = 0; j < m; j++) {
		    Cudd_RecursiveDeref(dd,array[previous][j]);
		}
		FREE(array[0]);
		FREE(array[1]);
		return(NULL);
	    }
	    cuddRef(tmp);
	    array[thisOne][i] = tmp;
	}
	/* One layer completed. Free the other array for the next iteration. */
	for (i = 0; i < m; i++) {
	    Cudd_RecursiveDeref(dd,array[previous][i]);
	}
	Cudd_RecursiveDeref(dd,var);
	/* Update residue of 2**k. */
	residue = (2 * residue) % m;
	/* Adjust residue for MSB, if this is a two's complement number. */
	if (tc && (k == n - 1)) {
	    residue = (m - residue) % m;
	}
    }

    /* We are only interested in the 0-residue node of the top layer. */
    for (i = 1; i < m; i++) {
	Cudd_RecursiveDeref(dd,array[(n - 1) & 1][i]);
    }
    res = array[(n - 1) & 1][0];

    FREE(array[0]);
    FREE(array[1]);

    cuddDeref(res);
    return(res);

} /* end of Cudd_addResidue */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_addWalsh.]

  Description [Generates a Walsh matrix in ADD form. Returns a pointer
  to the matrixi if successful; NULL otherwise.]

  SideEffects [None]

******************************************************************************/
static DdNode *
addWalshInt(
  DdManager * dd,
  DdNode ** x,
  DdNode ** y,
  int  n)
{
    DdNode *one, *minusone;
    DdNode *t, *u, *t1, *u1, *v, *w;
    int     i;

    one = DD_ONE(dd);
    if (n == 0) return(one);

    /* Build bottom part of ADD outside loop */
    minusone = cuddUniqueConst(dd,(CUDD_VALUE_TYPE) -1);
    if (minusone == NULL) return(NULL);
    cuddRef(minusone);
    v = Cudd_addIte(dd, y[n-1], minusone, one);
    if (v == NULL) {
	Cudd_RecursiveDeref(dd, minusone);
	return(NULL);
    }
    cuddRef(v);
    u = Cudd_addIte(dd, x[n-1], v, one);
    if (u == NULL) {
	Cudd_RecursiveDeref(dd, minusone);
	Cudd_RecursiveDeref(dd, v);
	return(NULL);
    }
    cuddRef(u);
    Cudd_RecursiveDeref(dd, v);
    if (n>1) {
	w = Cudd_addIte(dd, y[n-1], one, minusone);
	if (w == NULL) {
	    Cudd_RecursiveDeref(dd, minusone);
	    Cudd_RecursiveDeref(dd, u);
	    return(NULL);
	}
	cuddRef(w);
	t = Cudd_addIte(dd, x[n-1], w, minusone);
	if (t == NULL) {
	    Cudd_RecursiveDeref(dd, minusone);
	    Cudd_RecursiveDeref(dd, u);
	    Cudd_RecursiveDeref(dd, w);
	    return(NULL);
	}
	cuddRef(t);
	Cudd_RecursiveDeref(dd, w);
    }
    cuddDeref(minusone); /* minusone is in the result; it won't die */

    /* Loop to build the rest of the ADD */
    for (i=n-2; i>=0; i--) {
	t1 = t; u1 = u;
	v = Cudd_addIte(dd, y[i], t1, u1);
	if (v == NULL) {
	    Cudd_RecursiveDeref(dd, u1);
	    Cudd_RecursiveDeref(dd, t1);
	    return(NULL);
	}
	cuddRef(v);
	u = Cudd_addIte(dd, x[i], v, u1);
	if (u == NULL) {
	    Cudd_RecursiveDeref(dd, u1);
	    Cudd_RecursiveDeref(dd, t1);
	    Cudd_RecursiveDeref(dd, v);
	    return(NULL);
	}
	cuddRef(u);
	Cudd_RecursiveDeref(dd, v);
	if (i>0) {
	    w = Cudd_addIte(dd, y[i], u1, t1);
	    if (w == NULL) {
		Cudd_RecursiveDeref(dd, u1);
		Cudd_RecursiveDeref(dd, t1);
		Cudd_RecursiveDeref(dd, u);
		return(NULL);
	    }
	    cuddRef(w);
	    t = Cudd_addIte(dd, x[i], w, t1);
	    if (u == NULL) {
		Cudd_RecursiveDeref(dd, u1);
		Cudd_RecursiveDeref(dd, t1);
		Cudd_RecursiveDeref(dd, u);
		Cudd_RecursiveDeref(dd, w);
		return(NULL);
	    }
	    cuddRef(t);
	    Cudd_RecursiveDeref(dd, w);
	}
	Cudd_RecursiveDeref(dd, u1);
	Cudd_RecursiveDeref(dd, t1);
    }

    cuddDeref(u);
    return(u);

} /* end of addWalshInt */
