/**CFile***********************************************************************

  FileName    [cuddPriority.c]

  PackageName [cudd]

  Synopsis    [Priority functions.]

  Description [External procedures included in this file:
	    <ul>
	    <li> Cudd_PrioritySelect()
	    <li> Cudd_Xgty()
	    <li> Cudd_Xeqy()
	    <li> Cudd_addXeqy()
	    <li> Cudd_Dxygtdxz()
	    <li> Cudd_Dxygtdyz()
	    <li> Cudd_Inequality()
	    <li> Cudd_Disequality()
	    <li> Cudd_bddInterval()
	    <li> Cudd_CProjection()
	    <li> Cudd_addHamming()
	    <li> Cudd_MinHammingDist()
	    <li> Cudd_bddClosestCube()
	    </ul>
	Internal procedures included in this module:
	    <ul>
	    <li> cuddCProjectionRecur()
	    <li> cuddBddClosestCube()
	    </ul>
	Static procedures included in this module:
	    <ul>
	    <li> cuddMinHammingDistRecur()
	    <li> separateCube()
	    <li> createResult()
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

#define DD_DEBUG 1

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
static char rcsid[] DD_UNUSED = "$Id: cuddPriority.c,v 1.36 2012/02/05 01:07:19 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/
static int cuddMinHammingDistRecur (DdNode * f, int *minterm, DdHashTable * table, int upperBound);
static DdNode * separateCube (DdManager *dd, DdNode *f, CUDD_VALUE_TYPE *distance);
static DdNode * createResult (DdManager *dd, unsigned int index, unsigned int phase, DdNode *cube, CUDD_VALUE_TYPE distance);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Selects pairs from R using a priority function.]

  Description [Selects pairs from a relation R(x,y) (given as a BDD)
  in such a way that a given x appears in one pair only. Uses a
  priority function to determine which y should be paired to a given x.
  Cudd_PrioritySelect returns a pointer to
  the selected function if successful; NULL otherwise.
  Three of the arguments--x, y, and z--are vectors of BDD variables.
  The first two are the variables on which R depends. The third vector
  is a vector of auxiliary variables, used during the computation. This
  vector is optional. If a NULL value is passed instead,
  Cudd_PrioritySelect will create the working variables on the fly.
  The sizes of x and y (and z if it is not NULL) should equal n.
  The priority function Pi can be passed as a BDD, or can be built by
  Cudd_PrioritySelect. If NULL is passed instead of a DdNode *,
  parameter Pifunc is used by Cudd_PrioritySelect to build a BDD for the
  priority function. (Pifunc is a pointer to a C function.) If Pi is not
  NULL, then Pifunc is ignored. Pifunc should have the same interface as
  the standard priority functions (e.g., Cudd_Dxygtdxz).
  Cudd_PrioritySelect and Cudd_CProjection can sometimes be used
  interchangeably. Specifically, calling Cudd_PrioritySelect with
  Cudd_Xgty as Pifunc produces the same result as calling
  Cudd_CProjection with the all-zero minterm as reference minterm.
  However, depending on the application, one or the other may be
  preferable:
  <ul>
  <li> When extracting representatives from an equivalence relation,
  Cudd_CProjection has the advantage of nor requiring the auxiliary
  variables.
  <li> When computing matchings in general bipartite graphs,
  Cudd_PrioritySelect normally obtains better results because it can use
  more powerful matching schemes (e.g., Cudd_Dxygtdxz).
  </ul>
  ]

  SideEffects [If called with z == NULL, will create new variables in
  the manager.]

  SeeAlso     [Cudd_Dxygtdxz Cudd_Dxygtdyz Cudd_Xgty
  Cudd_bddAdjPermuteX Cudd_CProjection]

******************************************************************************/
DdNode *
Cudd_PrioritySelect(
  DdManager * dd /* manager */,
  DdNode * R /* BDD of the relation */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */,
  DdNode ** z /* array of z variables (optional: may be NULL) */,
  DdNode * Pi /* BDD of the priority function (optional: may be NULL) */,
  int  n /* size of x, y, and z */,
  DD_PRFP Pifunc /* function used to build Pi if it is NULL */)
{
    DdNode *res = NULL;
    DdNode *zcube = NULL;
    DdNode *Rxz, *Q;
    int createdZ = 0;
    int createdPi = 0;
    int i;

    /* Create z variables if needed. */
    if (z == NULL) {
	if (Pi != NULL) return(NULL);
	z = ALLOC(DdNode *,n);
	if (z == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(NULL);
	}
	createdZ = 1;
	for (i = 0; i < n; i++) {
	    if (dd->size >= (int) CUDD_MAXINDEX - 1) goto endgame;
	    z[i] = cuddUniqueInter(dd,dd->size,dd->one,Cudd_Not(dd->one));
	    if (z[i] == NULL) goto endgame;
	}
    }

    /* Create priority function BDD if needed. */
    if (Pi == NULL) {
	Pi = Pifunc(dd,n,x,y,z);
	if (Pi == NULL) goto endgame;
	createdPi = 1;
	cuddRef(Pi);
    }

    /* Initialize abstraction cube. */
    zcube = DD_ONE(dd);
    cuddRef(zcube);
    for (i = n - 1; i >= 0; i--) {
	DdNode *tmpp;
	tmpp = Cudd_bddAnd(dd,z[i],zcube);
	if (tmpp == NULL) goto endgame;
	cuddRef(tmpp);
	Cudd_RecursiveDeref(dd,zcube);
	zcube = tmpp;
    }

    /* Compute subset of (x,y) pairs. */
    Rxz = Cudd_bddSwapVariables(dd,R,y,z,n);
    if (Rxz == NULL) goto endgame;
    cuddRef(Rxz);
    Q = Cudd_bddAndAbstract(dd,Rxz,Pi,zcube);
    if (Q == NULL) {
	Cudd_RecursiveDeref(dd,Rxz);
	goto endgame;
    }
    cuddRef(Q);
    Cudd_RecursiveDeref(dd,Rxz);
    res = Cudd_bddAnd(dd,R,Cudd_Not(Q));
    if (res == NULL) {
	Cudd_RecursiveDeref(dd,Q);
	goto endgame;
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd,Q);

endgame:
    if (zcube != NULL) Cudd_RecursiveDeref(dd,zcube);
    if (createdZ) {
	FREE(z);
    }
    if (createdPi) {
	Cudd_RecursiveDeref(dd,Pi);
    }
    if (res != NULL) cuddDeref(res);
    return(res);

} /* Cudd_PrioritySelect */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function x &gt; y.]

  Description [This function generates a BDD for the function x &gt; y.
  Both x and y are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\] and
  y\[0\] y\[1\] ...  y\[N-1\], with 0 the most significant bit.
  The BDD is built bottom-up.
  It has 3*N-1 internal nodes, if the variables are ordered as follows:
  x\[0\] y\[0\] x\[1\] y\[1\] ... x\[N-1\] y\[N-1\].
  Argument z is not used by Cudd_Xgty: it is included to make it
  call-compatible to Cudd_Dxygtdxz and Cudd_Dxygtdyz.]

  SideEffects [None]

  SeeAlso     [Cudd_PrioritySelect Cudd_Dxygtdxz Cudd_Dxygtdyz]

******************************************************************************/
DdNode *
Cudd_Xgty(
  DdManager * dd /* DD manager */,
  int  N /* number of x and y variables */,
  DdNode ** z /* array of z variables: unused */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */)
{
    DdNode *u, *v, *w;
    int     i;

    /* Build bottom part of BDD outside loop. */
    u = Cudd_bddAnd(dd, x[N-1], Cudd_Not(y[N-1]));
    if (u == NULL) return(NULL);
    cuddRef(u);

    /* Loop to build the rest of the BDD. */
    for (i = N-2; i >= 0; i--) {
	v = Cudd_bddAnd(dd, y[i], Cudd_Not(u));
	if (v == NULL) {
	    Cudd_RecursiveDeref(dd, u);
	    return(NULL);
	}
	cuddRef(v);
	w = Cudd_bddAnd(dd, Cudd_Not(y[i]), u);
	if (w == NULL) {
	    Cudd_RecursiveDeref(dd, u);
	    Cudd_RecursiveDeref(dd, v);
	    return(NULL);
	}
	cuddRef(w);
	Cudd_RecursiveDeref(dd, u);
	u = Cudd_bddIte(dd, x[i], Cudd_Not(v), w);
	if (u == NULL) {
	    Cudd_RecursiveDeref(dd, v);
	    Cudd_RecursiveDeref(dd, w);
	    return(NULL);
	}
	cuddRef(u);
	Cudd_RecursiveDeref(dd, v);
	Cudd_RecursiveDeref(dd, w);

    }
    cuddDeref(u);
    return(u);

} /* end of Cudd_Xgty */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function x==y.]

  Description [This function generates a BDD for the function x==y.
  Both x and y are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\] and
  y\[0\] y\[1\] ...  y\[N-1\], with 0 the most significant bit.
  The BDD is built bottom-up.
  It has 3*N-1 internal nodes, if the variables are ordered as follows:
  x\[0\] y\[0\] x\[1\] y\[1\] ... x\[N-1\] y\[N-1\]. ]

  SideEffects [None]

  SeeAlso     [Cudd_addXeqy]

******************************************************************************/
DdNode *
Cudd_Xeqy(
  DdManager * dd /* DD manager */,
  int  N /* number of x and y variables */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */)
{
    DdNode *u, *v, *w;
    int     i;

    /* Build bottom part of BDD outside loop. */
    u = Cudd_bddIte(dd, x[N-1], y[N-1], Cudd_Not(y[N-1]));
    if (u == NULL) return(NULL);
    cuddRef(u);

    /* Loop to build the rest of the BDD. */
    for (i = N-2; i >= 0; i--) {
	v = Cudd_bddAnd(dd, y[i], u);
	if (v == NULL) {
	    Cudd_RecursiveDeref(dd, u);
	    return(NULL);
	}
	cuddRef(v);
	w = Cudd_bddAnd(dd, Cudd_Not(y[i]), u);
	if (w == NULL) {
	    Cudd_RecursiveDeref(dd, u);
	    Cudd_RecursiveDeref(dd, v);
	    return(NULL);
	}
	cuddRef(w);
	Cudd_RecursiveDeref(dd, u);
	u = Cudd_bddIte(dd, x[i], v, w);
	if (u == NULL) {
	    Cudd_RecursiveDeref(dd, v);
	    Cudd_RecursiveDeref(dd, w);
	    return(NULL);
	}
	cuddRef(u);
	Cudd_RecursiveDeref(dd, v);
	Cudd_RecursiveDeref(dd, w);
    }
    cuddDeref(u);
    return(u);

} /* end of Cudd_Xeqy */


/**Function********************************************************************

  Synopsis    [Generates an ADD for the function x==y.]

  Description [This function generates an ADD for the function x==y.
  Both x and y are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\] and
  y\[0\] y\[1\] ...  y\[N-1\], with 0 the most significant bit.
  The ADD is built bottom-up.
  It has 3*N-1 internal nodes, if the variables are ordered as follows:
  x\[0\] y\[0\] x\[1\] y\[1\] ... x\[N-1\] y\[N-1\]. ]

  SideEffects [None]

  SeeAlso     [Cudd_Xeqy]

******************************************************************************/
DdNode *
Cudd_addXeqy(
  DdManager * dd /* DD manager */,
  int  N /* number of x and y variables */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */)
{
    DdNode *one, *zero;
    DdNode *u, *v, *w;
    int     i;

    one = DD_ONE(dd);
    zero = DD_ZERO(dd);

    /* Build bottom part of ADD outside loop. */
    v = Cudd_addIte(dd, y[N-1], one, zero);
    if (v == NULL) return(NULL);
    cuddRef(v);
    w = Cudd_addIte(dd, y[N-1], zero, one);
    if (w == NULL) {
	Cudd_RecursiveDeref(dd, v);
	return(NULL);
    }
    cuddRef(w);
    u = Cudd_addIte(dd, x[N-1], v, w);
    if (u == NULL) {
	Cudd_RecursiveDeref(dd, v);
	Cudd_RecursiveDeref(dd, w);
	return(NULL);
    }
    cuddRef(u);
    Cudd_RecursiveDeref(dd, v);
    Cudd_RecursiveDeref(dd, w);

    /* Loop to build the rest of the ADD. */
    for (i = N-2; i >= 0; i--) {
	v = Cudd_addIte(dd, y[i], u, zero);
	if (v == NULL) {
	    Cudd_RecursiveDeref(dd, u);
	    return(NULL);
	}
	cuddRef(v);
	w = Cudd_addIte(dd, y[i], zero, u);
	if (w == NULL) {
	    Cudd_RecursiveDeref(dd, u);
	    Cudd_RecursiveDeref(dd, v);
	    return(NULL);
	}
	cuddRef(w);
	Cudd_RecursiveDeref(dd, u);
	u = Cudd_addIte(dd, x[i], v, w);
	if (w == NULL) {
	    Cudd_RecursiveDeref(dd, v);
	    Cudd_RecursiveDeref(dd, w);
	    return(NULL);
	}
	cuddRef(u);
	Cudd_RecursiveDeref(dd, v);
	Cudd_RecursiveDeref(dd, w);
    }
    cuddDeref(u);
    return(u);

} /* end of Cudd_addXeqy */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function d(x,y) &gt; d(x,z).]

  Description [This function generates a BDD for the function d(x,y)
  &gt; d(x,z);
  x, y, and z are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\],
  y\[0\] y\[1\] ...  y\[N-1\], and z\[0\] z\[1\] ...  z\[N-1\],
  with 0 the most significant bit.
  The distance d(x,y) is defined as:
	\sum_{i=0}^{N-1}(|x_i - y_i| \cdot 2^{N-i-1}).
  The BDD is built bottom-up.
  It has 7*N-3 internal nodes, if the variables are ordered as follows:
  x\[0\] y\[0\] z\[0\] x\[1\] y\[1\] z\[1\] ... x\[N-1\] y\[N-1\] z\[N-1\]. ]

  SideEffects [None]

  SeeAlso     [Cudd_PrioritySelect Cudd_Dxygtdyz Cudd_Xgty Cudd_bddAdjPermuteX]

******************************************************************************/
DdNode *
Cudd_Dxygtdxz(
  DdManager * dd /* DD manager */,
  int  N /* number of x, y, and z variables */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */,
  DdNode ** z /* array of z variables */)
{
    DdNode *one, *zero;
    DdNode *z1, *z2, *z3, *z4, *y1_, *y2, *x1;
    int     i;

    one = DD_ONE(dd);
    zero = Cudd_Not(one);

    /* Build bottom part of BDD outside loop. */
    y1_ = Cudd_bddIte(dd, y[N-1], one, Cudd_Not(z[N-1]));
    if (y1_ == NULL) return(NULL);
    cuddRef(y1_);
    y2 = Cudd_bddIte(dd, y[N-1], z[N-1], one);
    if (y2 == NULL) {
	Cudd_RecursiveDeref(dd, y1_);
	return(NULL);
    }
    cuddRef(y2);
    x1 = Cudd_bddIte(dd, x[N-1], y1_, y2);
    if (x1 == NULL) {
	Cudd_RecursiveDeref(dd, y1_);
	Cudd_RecursiveDeref(dd, y2);
	return(NULL);
    }
    cuddRef(x1);
    Cudd_RecursiveDeref(dd, y1_);
    Cudd_RecursiveDeref(dd, y2);

    /* Loop to build the rest of the BDD. */
    for (i = N-2; i >= 0; i--) {
	z1 = Cudd_bddIte(dd, z[i], one, Cudd_Not(x1));
	if (z1 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    return(NULL);
	}
	cuddRef(z1);
	z2 = Cudd_bddIte(dd, z[i], x1, one);
	if (z2 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    Cudd_RecursiveDeref(dd, z1);
	    return(NULL);
	}
	cuddRef(z2);
	z3 = Cudd_bddIte(dd, z[i], one, x1);
	if (z3 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    return(NULL);
	}
	cuddRef(z3);
	z4 = Cudd_bddIte(dd, z[i], x1, zero);
	if (z4 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    Cudd_RecursiveDeref(dd, z3);
	    return(NULL);
	}
	cuddRef(z4);
	Cudd_RecursiveDeref(dd, x1);
	y1_ = Cudd_bddIte(dd, y[i], z2, Cudd_Not(z1));
	if (y1_ == NULL) {
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    Cudd_RecursiveDeref(dd, z3);
	    Cudd_RecursiveDeref(dd, z4);
	    return(NULL);
	}
	cuddRef(y1_);
	y2 = Cudd_bddIte(dd, y[i], z4, z3);
	if (y2 == NULL) {
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    Cudd_RecursiveDeref(dd, z3);
	    Cudd_RecursiveDeref(dd, z4);
	    Cudd_RecursiveDeref(dd, y1_);
	    return(NULL);
	}
	cuddRef(y2);
	Cudd_RecursiveDeref(dd, z1);
	Cudd_RecursiveDeref(dd, z2);
	Cudd_RecursiveDeref(dd, z3);
	Cudd_RecursiveDeref(dd, z4);
	x1 = Cudd_bddIte(dd, x[i], y1_, y2);
	if (x1 == NULL) {
	    Cudd_RecursiveDeref(dd, y1_);
	    Cudd_RecursiveDeref(dd, y2);
	    return(NULL);
	}
	cuddRef(x1);
	Cudd_RecursiveDeref(dd, y1_);
	Cudd_RecursiveDeref(dd, y2);
    }
    cuddDeref(x1);
    return(Cudd_Not(x1));

} /* end of Cudd_Dxygtdxz */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function d(x,y) &gt; d(y,z).]

  Description [This function generates a BDD for the function d(x,y)
  &gt; d(y,z);
  x, y, and z are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\],
  y\[0\] y\[1\] ...  y\[N-1\], and z\[0\] z\[1\] ...  z\[N-1\],
  with 0 the most significant bit.
  The distance d(x,y) is defined as:
	\sum_{i=0}^{N-1}(|x_i - y_i| \cdot 2^{N-i-1}).
  The BDD is built bottom-up.
  It has 7*N-3 internal nodes, if the variables are ordered as follows:
  x\[0\] y\[0\] z\[0\] x\[1\] y\[1\] z\[1\] ... x\[N-1\] y\[N-1\] z\[N-1\]. ]

  SideEffects [None]

  SeeAlso     [Cudd_PrioritySelect Cudd_Dxygtdxz Cudd_Xgty Cudd_bddAdjPermuteX]

******************************************************************************/
DdNode *
Cudd_Dxygtdyz(
  DdManager * dd /* DD manager */,
  int  N /* number of x, y, and z variables */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */,
  DdNode ** z /* array of z variables */)
{
    DdNode *one, *zero;
    DdNode *z1, *z2, *z3, *z4, *y1_, *y2, *x1;
    int     i;

    one = DD_ONE(dd);
    zero = Cudd_Not(one);

    /* Build bottom part of BDD outside loop. */
    y1_ = Cudd_bddIte(dd, y[N-1], one, z[N-1]);
    if (y1_ == NULL) return(NULL);
    cuddRef(y1_);
    y2 = Cudd_bddIte(dd, y[N-1], z[N-1], zero);
    if (y2 == NULL) {
	Cudd_RecursiveDeref(dd, y1_);
	return(NULL);
    }
    cuddRef(y2);
    x1 = Cudd_bddIte(dd, x[N-1], y1_, Cudd_Not(y2));
    if (x1 == NULL) {
	Cudd_RecursiveDeref(dd, y1_);
	Cudd_RecursiveDeref(dd, y2);
	return(NULL);
    }
    cuddRef(x1);
    Cudd_RecursiveDeref(dd, y1_);
    Cudd_RecursiveDeref(dd, y2);

    /* Loop to build the rest of the BDD. */
    for (i = N-2; i >= 0; i--) {
	z1 = Cudd_bddIte(dd, z[i], x1, zero);
	if (z1 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    return(NULL);
	}
	cuddRef(z1);
	z2 = Cudd_bddIte(dd, z[i], x1, one);
	if (z2 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    Cudd_RecursiveDeref(dd, z1);
	    return(NULL);
	}
	cuddRef(z2);
	z3 = Cudd_bddIte(dd, z[i], one, x1);
	if (z3 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    return(NULL);
	}
	cuddRef(z3);
	z4 = Cudd_bddIte(dd, z[i], one, Cudd_Not(x1));
	if (z4 == NULL) {
	    Cudd_RecursiveDeref(dd, x1);
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    Cudd_RecursiveDeref(dd, z3);
	    return(NULL);
	}
	cuddRef(z4);
	Cudd_RecursiveDeref(dd, x1);
	y1_ = Cudd_bddIte(dd, y[i], z2, z1);
	if (y1_ == NULL) {
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    Cudd_RecursiveDeref(dd, z3);
	    Cudd_RecursiveDeref(dd, z4);
	    return(NULL);
	}
	cuddRef(y1_);
	y2 = Cudd_bddIte(dd, y[i], z4, Cudd_Not(z3));
	if (y2 == NULL) {
	    Cudd_RecursiveDeref(dd, z1);
	    Cudd_RecursiveDeref(dd, z2);
	    Cudd_RecursiveDeref(dd, z3);
	    Cudd_RecursiveDeref(dd, z4);
	    Cudd_RecursiveDeref(dd, y1_);
	    return(NULL);
	}
	cuddRef(y2);
	Cudd_RecursiveDeref(dd, z1);
	Cudd_RecursiveDeref(dd, z2);
	Cudd_RecursiveDeref(dd, z3);
	Cudd_RecursiveDeref(dd, z4);
	x1 = Cudd_bddIte(dd, x[i], y1_, Cudd_Not(y2));
	if (x1 == NULL) {
	    Cudd_RecursiveDeref(dd, y1_);
	    Cudd_RecursiveDeref(dd, y2);
	    return(NULL);
	}
	cuddRef(x1);
	Cudd_RecursiveDeref(dd, y1_);
	Cudd_RecursiveDeref(dd, y2);
    }
    cuddDeref(x1);
    return(Cudd_Not(x1));

} /* end of Cudd_Dxygtdyz */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function x - y &ge; c.]

  Description [This function generates a BDD for the function x -y &ge; c.
  Both x and y are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\] and
  y\[0\] y\[1\] ...  y\[N-1\], with 0 the most significant bit.
  The BDD is built bottom-up.
  It has a linear number of nodes if the variables are ordered as follows:
  x\[0\] y\[0\] x\[1\] y\[1\] ... x\[N-1\] y\[N-1\].]

  SideEffects [None]

  SeeAlso     [Cudd_Xgty]

******************************************************************************/
DdNode *
Cudd_Inequality(
  DdManager * dd /* DD manager */,
  int  N /* number of x and y variables */,
  int c /* right-hand side constant */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */)
{
    /* The nodes at level i represent values of the difference that are
    ** multiples of 2^i.  We use variables with names starting with k
    ** to denote the multipliers of 2^i in such multiples. */
    int kTrue = c;
    int kFalse = c - 1;
    /* Mask used to compute the ceiling function.  Since we divide by 2^i,
    ** we want to know whether the dividend is a multiple of 2^i.  If it is,
    ** then ceiling and floor coincide; otherwise, they differ by one. */
    int mask = 1;
    int i;

    DdNode *f = NULL;		/* the eventual result */
    DdNode *one = DD_ONE(dd);
    DdNode *zero = Cudd_Not(one);

    /* Two x-labeled nodes are created at most at each iteration.  They are
    ** stored, along with their k values, in these variables.  At each level,
    ** the old nodes are freed and the new nodes are copied into the old map.
    */
    DdNode *map[2];
    int invalidIndex = 1 << (N-1);
    int index[2] = {invalidIndex, invalidIndex};

    /* This should never happen. */
    if (N < 0) return(NULL);

    /* If there are no bits, both operands are 0.  The result depends on c. */
    if (N == 0) {
	if (c >= 0) return(one);
	else return(zero);
    }

    /* The maximum or the minimum difference comparing to c can generate the terminal case */
    if ((1 << N) - 1 < c) return(zero);
    else if ((-(1 << N) + 1) >= c) return(one);

    /* Build the result bottom up. */
    for (i = 1; i <= N; i++) {
	int kTrueLower, kFalseLower;
	int leftChild, middleChild, rightChild;
	DdNode *g0, *g1, *fplus, *fequal, *fminus;
	int j;
	DdNode *newMap[2];
	int newIndex[2];

	kTrueLower = kTrue;
	kFalseLower = kFalse;
	/* kTrue = ceiling((c-1)/2^i) + 1 */
	kTrue = ((c-1) >> i) + ((c & mask) != 1) + 1;
	mask = (mask << 1) | 1;
	/* kFalse = floor(c/2^i) - 1 */
	kFalse = (c >> i) - 1;
	newIndex[0] = invalidIndex;
	newIndex[1] = invalidIndex;

	for (j = kFalse + 1; j < kTrue; j++) {
	    /* Skip if node is not reachable from top of BDD. */
	    if ((j >= (1 << (N - i))) || (j <= -(1 << (N -i)))) continue;

	    /* Find f- */
	    leftChild = (j << 1) - 1;
	    if (leftChild >= kTrueLower) {
		fminus = one;
	    } else if (leftChild <= kFalseLower) {
		fminus = zero;
	    } else {
		assert(leftChild == index[0] || leftChild == index[1]);
		if (leftChild == index[0]) {
		    fminus = map[0];
		} else {
		    fminus = map[1];
		}
	    }

	    /* Find f= */
	    middleChild = j << 1;
	    if (middleChild >= kTrueLower) {
		fequal = one;
	    } else if (middleChild <= kFalseLower) {
		fequal = zero;
	    } else {
		assert(middleChild == index[0] || middleChild == index[1]);
		if (middleChild == index[0]) {
		    fequal = map[0];
		} else {
		    fequal = map[1];
		}
	    }

	    /* Find f+ */
	    rightChild = (j << 1) + 1;
	    if (rightChild >= kTrueLower) {
		fplus = one;
	    } else if (rightChild <= kFalseLower) {
		fplus = zero;
	    } else {
		assert(rightChild == index[0] || rightChild == index[1]);
		if (rightChild == index[0]) {
		    fplus = map[0];
		} else {
		    fplus = map[1];
		}
	    }

	    /* Build new nodes. */
	    g1 = Cudd_bddIte(dd, y[N - i], fequal, fplus);
	    if (g1 == NULL) {
		if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
		if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
		if (newIndex[0] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[0]);
		if (newIndex[1] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[1]);
		return(NULL);
	    }
	    cuddRef(g1);
	    g0 = Cudd_bddIte(dd, y[N - i], fminus, fequal);
	    if (g0 == NULL) {
		Cudd_IterDerefBdd(dd, g1);
		if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
		if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
		if (newIndex[0] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[0]);
		if (newIndex[1] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[1]);
		return(NULL);
	    }
	    cuddRef(g0);
	    f = Cudd_bddIte(dd, x[N - i], g1, g0);
	    if (f == NULL) {
		Cudd_IterDerefBdd(dd, g1);
		Cudd_IterDerefBdd(dd, g0);
		if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
		if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
		if (newIndex[0] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[0]);
		if (newIndex[1] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[1]);
		return(NULL);
	    }
	    cuddRef(f);
	    Cudd_IterDerefBdd(dd, g1);
	    Cudd_IterDerefBdd(dd, g0);

	    /* Save newly computed node in map. */
	    assert(newIndex[0] == invalidIndex || newIndex[1] == invalidIndex);
	    if (newIndex[0] == invalidIndex) {
		newIndex[0] = j;
		newMap[0] = f;
	    } else {
		newIndex[1] = j;
		newMap[1] = f;
	    }
	}

	/* Copy new map to map. */
	if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
	if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
	map[0] = newMap[0];
	map[1] = newMap[1];
	index[0] = newIndex[0];
	index[1] = newIndex[1];
    }

    cuddDeref(f);
    return(f);

} /* end of Cudd_Inequality */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function x - y != c.]

  Description [This function generates a BDD for the function x -y != c.
  Both x and y are N-bit numbers, x\[0\] x\[1\] ... x\[N-1\] and
  y\[0\] y\[1\] ...  y\[N-1\], with 0 the most significant bit.
  The BDD is built bottom-up.
  It has a linear number of nodes if the variables are ordered as follows:
  x\[0\] y\[0\] x\[1\] y\[1\] ... x\[N-1\] y\[N-1\].]

  SideEffects [None]

  SeeAlso     [Cudd_Xgty]

******************************************************************************/
DdNode *
Cudd_Disequality(
  DdManager * dd /* DD manager */,
  int  N /* number of x and y variables */,
  int c /* right-hand side constant */,
  DdNode ** x /* array of x variables */,
  DdNode ** y /* array of y variables */)
{
    /* The nodes at level i represent values of the difference that are
    ** multiples of 2^i.  We use variables with names starting with k
    ** to denote the multipliers of 2^i in such multiples. */
    int kTrueLb = c + 1;
    int kTrueUb = c - 1;
    int kFalse = c;
    /* Mask used to compute the ceiling function.  Since we divide by 2^i,
    ** we want to know whether the dividend is a multiple of 2^i.  If it is,
    ** then ceiling and floor coincide; otherwise, they differ by one. */
    int mask = 1;
    int i;

    DdNode *f = NULL;		/* the eventual result */
    DdNode *one = DD_ONE(dd);
    DdNode *zero = Cudd_Not(one);

    /* Two x-labeled nodes are created at most at each iteration.  They are
    ** stored, along with their k values, in these variables.  At each level,
    ** the old nodes are freed and the new nodes are copied into the old map.
    */
    DdNode *map[2];
    int invalidIndex = 1 << (N-1);
    int index[2] = {invalidIndex, invalidIndex};

    /* This should never happen. */
    if (N < 0) return(NULL);

    /* If there are no bits, both operands are 0.  The result depends on c. */
    if (N == 0) {
	if (c != 0) return(one);
	else return(zero);
    }

    /* The maximum or the minimum difference comparing to c can generate the terminal case */
    if ((1 << N) - 1 < c || (-(1 << N) + 1) > c) return(one);

    /* Build the result bottom up. */
    for (i = 1; i <= N; i++) {
	int kTrueLbLower, kTrueUbLower;
	int leftChild, middleChild, rightChild;
	DdNode *g0, *g1, *fplus, *fequal, *fminus;
	int j;
	DdNode *newMap[2];
	int newIndex[2];

	kTrueLbLower = kTrueLb;
	kTrueUbLower = kTrueUb;
	/* kTrueLb = floor((c-1)/2^i) + 2 */
	kTrueLb = ((c-1) >> i) + 2;
	/* kTrueUb = ceiling((c+1)/2^i) - 2 */
	kTrueUb = ((c+1) >> i) + (((c+2) & mask) != 1) - 2;
	mask = (mask << 1) | 1;
	newIndex[0] = invalidIndex;
	newIndex[1] = invalidIndex;

	for (j = kTrueUb + 1; j < kTrueLb; j++) {
	    /* Skip if node is not reachable from top of BDD. */
	    if ((j >= (1 << (N - i))) || (j <= -(1 << (N -i)))) continue;

	    /* Find f- */
	    leftChild = (j << 1) - 1;
	    if (leftChild >= kTrueLbLower || leftChild <= kTrueUbLower) {
		fminus = one;
	    } else if (i == 1 && leftChild == kFalse) {
		fminus = zero;
	    } else {
		assert(leftChild == index[0] || leftChild == index[1]);
		if (leftChild == index[0]) {
		    fminus = map[0];
		} else {
		    fminus = map[1];
		}
	    }

	    /* Find f= */
	    middleChild = j << 1;
	    if (middleChild >= kTrueLbLower || middleChild <= kTrueUbLower) {
		fequal = one;
	    } else if (i == 1 && middleChild == kFalse) {
		fequal = zero;
	    } else {
		assert(middleChild == index[0] || middleChild == index[1]);
		if (middleChild == index[0]) {
		    fequal = map[0];
		} else {
		    fequal = map[1];
		}
	    }

	    /* Find f+ */
	    rightChild = (j << 1) + 1;
	    if (rightChild >= kTrueLbLower || rightChild <= kTrueUbLower) {
		fplus = one;
	    } else if (i == 1 && rightChild == kFalse) {
		fplus = zero;
	    } else {
		assert(rightChild == index[0] || rightChild == index[1]);
		if (rightChild == index[0]) {
		    fplus = map[0];
		} else {
		    fplus = map[1];
		}
	    }

	    /* Build new nodes. */
	    g1 = Cudd_bddIte(dd, y[N - i], fequal, fplus);
	    if (g1 == NULL) {
		if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
		if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
		if (newIndex[0] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[0]);
		if (newIndex[1] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[1]);
		return(NULL);
	    }
	    cuddRef(g1);
	    g0 = Cudd_bddIte(dd, y[N - i], fminus, fequal);
	    if (g0 == NULL) {
		Cudd_IterDerefBdd(dd, g1);
		if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
		if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
		if (newIndex[0] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[0]);
		if (newIndex[1] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[1]);
		return(NULL);
	    }
	    cuddRef(g0);
	    f = Cudd_bddIte(dd, x[N - i], g1, g0);
	    if (f == NULL) {
		Cudd_IterDerefBdd(dd, g1);
		Cudd_IterDerefBdd(dd, g0);
		if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
		if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
		if (newIndex[0] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[0]);
		if (newIndex[1] != invalidIndex) Cudd_IterDerefBdd(dd, newMap[1]);
		return(NULL);
	    }
	    cuddRef(f);
	    Cudd_IterDerefBdd(dd, g1);
	    Cudd_IterDerefBdd(dd, g0);

	    /* Save newly computed node in map. */
	    assert(newIndex[0] == invalidIndex || newIndex[1] == invalidIndex);
	    if (newIndex[0] == invalidIndex) {
		newIndex[0] = j;
		newMap[0] = f;
	    } else {
		newIndex[1] = j;
		newMap[1] = f;
	    }
	}

	/* Copy new map to map. */
	if (index[0] != invalidIndex) Cudd_IterDerefBdd(dd, map[0]);
	if (index[1] != invalidIndex) Cudd_IterDerefBdd(dd, map[1]);
	map[0] = newMap[0];
	map[1] = newMap[1];
	index[0] = newIndex[0];
	index[1] = newIndex[1];
    }

    cuddDeref(f);
    return(f);

} /* end of Cudd_Disequality */


/**Function********************************************************************

  Synopsis    [Generates a BDD for the function lowerB &le; x &le; upperB.]

  Description [This function generates a BDD for the function
  lowerB &le; x &le; upperB, where x is an N-bit number,
  x\[0\] x\[1\] ... x\[N-1\], with 0 the most significant bit (important!).
  The number of variables N should be sufficient to represent the bounds;
  otherwise, the bounds are truncated to their N least significant bits.
  Two BDDs are built bottom-up for lowerB &le; x and x &le; upperB, and they
  are finally conjoined.]

  SideEffects [None]

  SeeAlso     [Cudd_Xgty]

******************************************************************************/
DdNode *
Cudd_bddInterval(
  DdManager * dd /* DD manager */,
  int  N /* number of x variables */,
  DdNode ** x /* array of x variables */,
  unsigned int lowerB /* lower bound */,
  unsigned int upperB /* upper bound */)
{
    DdNode *one, *zero;
    DdNode *r, *rl, *ru;
    int     i;

    one = DD_ONE(dd);
    zero = Cudd_Not(one);

    rl = one;
    cuddRef(rl);
    ru = one;
    cuddRef(ru);

    /* Loop to build the rest of the BDDs. */
    for (i = N-1; i >= 0; i--) {
	DdNode *vl, *vu;
	vl = Cudd_bddIte(dd, x[i],
			 lowerB&1 ? rl : one,
			 lowerB&1 ? zero : rl);
	if (vl == NULL) {
	    Cudd_IterDerefBdd(dd, rl);
	    Cudd_IterDerefBdd(dd, ru);
	    return(NULL);
	}
	cuddRef(vl);
	Cudd_IterDerefBdd(dd, rl);
	rl = vl;
	lowerB >>= 1;
	vu = Cudd_bddIte(dd, x[i],
			 upperB&1 ? ru : zero,
			 upperB&1 ? one : ru);
	if (vu == NULL) {
	    Cudd_IterDerefBdd(dd, rl);
	    Cudd_IterDerefBdd(dd, ru);
	    return(NULL);
	}
	cuddRef(vu);
	Cudd_IterDerefBdd(dd, ru);
	ru = vu;
	upperB >>= 1;
    }

    /* Conjoin the two bounds. */
    r = Cudd_bddAnd(dd, rl, ru);
    if (r == NULL) {
	Cudd_IterDerefBdd(dd, rl);
	Cudd_IterDerefBdd(dd, ru);
	return(NULL);
    }
    cuddRef(r);
    Cudd_IterDerefBdd(dd, rl);
    Cudd_IterDerefBdd(dd, ru);
    cuddDeref(r);
    return(r);

} /* end of Cudd_bddInterval */


/**Function********************************************************************

  Synopsis    [Computes the compatible projection of R w.r.t. cube Y.]

  Description [Computes the compatible projection of relation R with
  respect to cube Y. Returns a pointer to the c-projection if
  successful; NULL otherwise. For a comparison between Cudd_CProjection
  and Cudd_PrioritySelect, see the documentation of the latter.]

  SideEffects [None]

  SeeAlso     [Cudd_PrioritySelect]

******************************************************************************/
DdNode *
Cudd_CProjection(
  DdManager * dd,
  DdNode * R,
  DdNode * Y)
{
    DdNode *res;
    DdNode *support;

    if (Cudd_CheckCube(dd,Y) == 0) {
	(void) fprintf(dd->err,
	"Error: The third argument of Cudd_CProjection should be a cube\n");
	dd->errorCode = CUDD_INVALID_ARG;
	return(NULL);
    }

    /* Compute the support of Y, which is used by the abstraction step
    ** in cuddCProjectionRecur.
    */
    support = Cudd_Support(dd,Y);
    if (support == NULL) return(NULL);
    cuddRef(support);

    do {
	dd->reordered = 0;
	res = cuddCProjectionRecur(dd,R,Y,support);
    } while (dd->reordered == 1);

    if (res == NULL) {
	Cudd_RecursiveDeref(dd,support);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd,support);
    cuddDeref(res);

    return(res);

} /* end of Cudd_CProjection */


/**Function********************************************************************

  Synopsis    [Computes the Hamming distance ADD.]

  Description [Computes the Hamming distance ADD. Returns an ADD that
  gives the Hamming distance between its two arguments if successful;
  NULL otherwise. The two vectors xVars and yVars identify the variables
  that form the two arguments.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_addHamming(
  DdManager * dd,
  DdNode ** xVars,
  DdNode ** yVars,
  int  nVars)
{
    DdNode  *result,*tempBdd;
    DdNode  *tempAdd,*temp;
    int     i;

    result = DD_ZERO(dd);
    cuddRef(result);

    for (i = 0; i < nVars; i++) {
	tempBdd = Cudd_bddIte(dd,xVars[i],Cudd_Not(yVars[i]),yVars[i]);
	if (tempBdd == NULL) {
	    Cudd_RecursiveDeref(dd,result);
	    return(NULL);
	}
	cuddRef(tempBdd);
	tempAdd = Cudd_BddToAdd(dd,tempBdd);
	if (tempAdd == NULL) {
	    Cudd_RecursiveDeref(dd,tempBdd);
	    Cudd_RecursiveDeref(dd,result);
	    return(NULL);
	}
	cuddRef(tempAdd);
	Cudd_RecursiveDeref(dd,tempBdd);
	temp = Cudd_addApply(dd,Cudd_addPlus,tempAdd,result);
	if (temp == NULL) {
	    Cudd_RecursiveDeref(dd,tempAdd);
	    Cudd_RecursiveDeref(dd,result);
	    return(NULL);
	}
	cuddRef(temp);
	Cudd_RecursiveDeref(dd,tempAdd);
	Cudd_RecursiveDeref(dd,result);
	result = temp;
    }

    cuddDeref(result);
    return(result);

} /* end of Cudd_addHamming */


/**Function********************************************************************

  Synopsis    [Returns the minimum Hamming distance between f and minterm.]

  Description [Returns the minimum Hamming distance between the
  minterms of a function f and a reference minterm. The function is
  given as a BDD; the minterm is given as an array of integers, one
  for each variable in the manager.  Returns the minimum distance if
  it is less than the upper bound; the upper bound if the minimum
  distance is at least as large; CUDD_OUT_OF_MEM in case of failure.]

  SideEffects [None]

  SeeAlso     [Cudd_addHamming Cudd_bddClosestCube]

******************************************************************************/
int
Cudd_MinHammingDist(
  DdManager *dd /* DD manager */,
  DdNode *f /* function to examine */,
  int *minterm /* reference minterm */,
  int upperBound /* distance above which an approximate answer is OK */)
{
    DdHashTable *table;
    CUDD_VALUE_TYPE epsilon;
    int res;

    table = cuddHashTableInit(dd,1,2);
    if (table == NULL) {
	return(CUDD_OUT_OF_MEM);
    }
    epsilon = Cudd_ReadEpsilon(dd);
    Cudd_SetEpsilon(dd,(CUDD_VALUE_TYPE)0.0);
    res = cuddMinHammingDistRecur(f,minterm,table,upperBound);
    cuddHashTableQuit(table);
    Cudd_SetEpsilon(dd,epsilon);

    return(res);

} /* end of Cudd_MinHammingDist */


/**Function********************************************************************

  Synopsis    [Finds a cube of f at minimum Hamming distance from g.]

  Description [Finds a cube of f at minimum Hamming distance from the
  minterms of g.  All the minterms of the cube are at the minimum
  distance.  If the distance is 0, the cube belongs to the
  intersection of f and g.  Returns the cube if successful; NULL
  otherwise.]

  SideEffects [The distance is returned as a side effect.]

  SeeAlso     [Cudd_MinHammingDist]

******************************************************************************/
DdNode *
Cudd_bddClosestCube(
  DdManager *dd,
  DdNode * f,
  DdNode *g,
  int *distance)
{
    DdNode *res, *acube;
    CUDD_VALUE_TYPE rdist;

    /* Compute the cube and distance as a single ADD. */
    do {
	dd->reordered = 0;
	res = cuddBddClosestCube(dd,f,g,CUDD_CONST_INDEX + 1.0);
    } while (dd->reordered == 1);
    if (res == NULL) return(NULL);
    cuddRef(res);

    /* Unpack distance and cube. */
    do {
	dd->reordered = 0;
	acube = separateCube(dd, res, &rdist);
    } while (dd->reordered == 1);
    if (acube == NULL) {
	Cudd_RecursiveDeref(dd, res);
	return(NULL);
    }
    cuddRef(acube);
    Cudd_RecursiveDeref(dd, res);

    /* Convert cube from ADD to BDD. */
    do {
	dd->reordered = 0;
	res = cuddAddBddDoPattern(dd, acube);
    } while (dd->reordered == 1);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, acube);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, acube);

    *distance = (int) rdist;
    cuddDeref(res);
    return(res);

} /* end of Cudd_bddClosestCube */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CProjection.]

  Description [Performs the recursive step of Cudd_CProjection. Returns
  the projection if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_CProjection]

******************************************************************************/
DdNode *
cuddCProjectionRecur(
  DdManager * dd,
  DdNode * R,
  DdNode * Y,
  DdNode * Ysupp)
{
    DdNode *res, *res1, *res2, *resA;
    DdNode *r, *y, *RT, *RE, *YT, *YE, *Yrest, *Ra, *Ran, *Gamma, *Alpha;
    unsigned int topR, topY, top, index;
    DdNode *one = DD_ONE(dd);

    statLine(dd);
    if (Y == one) return(R);

#ifdef DD_DEBUG
    assert(!Cudd_IsConstant(Y));
#endif

    if (R == Cudd_Not(one)) return(R);

    res = cuddCacheLookup2(dd, Cudd_CProjection, R, Y);
    if (res != NULL) return(res);

    r = Cudd_Regular(R);
    topR = cuddI(dd,r->index);
    y = Cudd_Regular(Y);
    topY = cuddI(dd,y->index);

    top = ddMin(topR, topY);

    /* Compute the cofactors of R */
    if (topR == top) {
	index = r->index;
	RT = cuddT(r);
	RE = cuddE(r);
	if (r != R) {
	    RT = Cudd_Not(RT); RE = Cudd_Not(RE);
	}
    } else {
	RT = RE = R;
    }

    if (topY > top) {
	/* Y does not depend on the current top variable.
	** We just need to compute the results on the two cofactors of R
	** and make them the children of a node labeled r->index.
	*/
	res1 = cuddCProjectionRecur(dd,RT,Y,Ysupp);
	if (res1 == NULL) return(NULL);
	cuddRef(res1);
	res2 = cuddCProjectionRecur(dd,RE,Y,Ysupp);
	if (res2 == NULL) {
	    Cudd_RecursiveDeref(dd,res1);
	    return(NULL);
	}
	cuddRef(res2);
	res = cuddBddIteRecur(dd, dd->vars[index], res1, res2);
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd,res1);
	    Cudd_RecursiveDeref(dd,res2);
	    return(NULL);
	}
	/* If we have reached this point, res1 and res2 are now
	** incorporated in res. cuddDeref is therefore sufficient.
	*/
	cuddDeref(res1);
	cuddDeref(res2);
    } else {
	/* Compute the cofactors of Y */
	index = y->index;
	YT = cuddT(y);
	YE = cuddE(y);
	if (y != Y) {
	    YT = Cudd_Not(YT); YE = Cudd_Not(YE);
	}
	if (YT == Cudd_Not(one)) {
	    Alpha  = Cudd_Not(dd->vars[index]);
	    Yrest = YE;
	    Ra = RE;
	    Ran = RT;
	} else {
	    Alpha = dd->vars[index];
	    Yrest = YT;
	    Ra = RT;
	    Ran = RE;
	}
	Gamma = cuddBddExistAbstractRecur(dd,Ra,cuddT(Ysupp));
	if (Gamma == NULL) return(NULL);
	if (Gamma == one) {
	    res1 = cuddCProjectionRecur(dd,Ra,Yrest,cuddT(Ysupp));
	    if (res1 == NULL) return(NULL);
	    cuddRef(res1);
	    res = cuddBddAndRecur(dd, Alpha, res1);
	    if (res == NULL) {
		Cudd_RecursiveDeref(dd,res1);
		return(NULL);
	    }
	    cuddDeref(res1);
	} else if (Gamma == Cudd_Not(one)) {
	    res1 = cuddCProjectionRecur(dd,Ran,Yrest,cuddT(Ysupp));
	    if (res1 == NULL) return(NULL);
	    cuddRef(res1);
	    res = cuddBddAndRecur(dd, Cudd_Not(Alpha), res1);
	    if (res == NULL) {
		Cudd_RecursiveDeref(dd,res1);
		return(NULL);
	    }
	    cuddDeref(res1);
	} else {
	    cuddRef(Gamma);
	    resA = cuddCProjectionRecur(dd,Ran,Yrest,cuddT(Ysupp));
	    if (resA == NULL) {
		Cudd_RecursiveDeref(dd,Gamma);
		return(NULL);
	    }
	    cuddRef(resA);
	    res2 = cuddBddAndRecur(dd, Cudd_Not(Gamma), resA);
	    if (res2 == NULL) {
		Cudd_RecursiveDeref(dd,Gamma);
		Cudd_RecursiveDeref(dd,resA);
		return(NULL);
	    }
	    cuddRef(res2);
	    Cudd_RecursiveDeref(dd,Gamma);
	    Cudd_RecursiveDeref(dd,resA);
	    res1 = cuddCProjectionRecur(dd,Ra,Yrest,cuddT(Ysupp));
	    if (res1 == NULL) {
		Cudd_RecursiveDeref(dd,res2);
		return(NULL);
	    }
	    cuddRef(res1);
	    res = cuddBddIteRecur(dd, Alpha, res1, res2);
	    if (res == NULL) {
		Cudd_RecursiveDeref(dd,res1);
		Cudd_RecursiveDeref(dd,res2);
		return(NULL);
	    }
	    cuddDeref(res1);
	    cuddDeref(res2);
	}
    }

    cuddCacheInsert2(dd,Cudd_CProjection,R,Y,res);

    return(res);

} /* end of cuddCProjectionRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddClosestCube.]

  Description [Performs the recursive step of Cudd_bddClosestCube.
  Returns the cube if succesful; NULL otherwise.  The procedure uses a
  four-way recursion to examine all four combinations of cofactors of
  <code>f</code> and <code>g</code> according to the following formula.
  <pre>
    H(f,g) = min(H(ft,gt), H(fe,ge), H(ft,ge)+1, H(fe,gt)+1)
  </pre>
  Bounding is based on the following observations.
  <ul>
  <li> If we already found two points at distance 0, there is no point in
       continuing.  Furthermore,
  <li> If F == not(G) then the best we can hope for is a minimum distance
       of 1.  If we have already found two points at distance 1, there is
       no point in continuing.  (Indeed, H(F,G) == 1 in this case.  We
       have to continue, though, to find the cube.)
  </ul>
  The variable <code>bound</code> is set at the largest value of the distance
  that we are still interested in.  Therefore, we desist when
  <pre>
    (bound == -1) and (F != not(G)) or (bound == 0) and (F == not(G)).
  </pre>
  If we were maximally aggressive in using the bound, we would always
  set the bound to the minimum distance seen thus far minus one.  That
  is, we would maintain the invariant
  <pre>
    bound < minD,
  </pre>
  except at the very beginning, when we have no value for
  <code>minD</code>.<p>

  However, we do not use <code>bound < minD</code> when examining the
  two negative cofactors, because we try to find a large cube at
  minimum distance.  To do so, we try to find a cube in the negative
  cofactors at the same or smaller distance from the cube found in the
  positive cofactors.<p>

  When we compute <code>H(ft,ge)</code> and <code>H(fe,gt)</code> we
  know that we are going to add 1 to the result of the recursive call
  to account for the difference in the splitting variable.  Therefore,
  we decrease the bound correspondingly.<p>

  Another important observation concerns the need of examining all
  four pairs of cofators only when both <code>f</code> and
  <code>g</code> depend on the top variable.<p>

  Suppose <code>gt == ge == g</code>.  (That is, <code>g</code> does
  not depend on the top variable.)  Then
  <pre>
    H(f,g) = min(H(ft,g), H(fe,g), H(ft,g)+1, H(fe,g)+1)
	   = min(H(ft,g), H(fe,g)) .
  </pre>
  Therefore, under these circumstances, we skip the two "cross" cases.<p>

  An interesting feature of this function is the scheme used for
  caching the results in the global computed table.  Since we have a
  cube and a distance, we combine them to form an ADD.  The
  combination replaces the zero child of the top node of the cube with
  the negative of the distance.  (The use of the negative is to avoid
  ambiguity with 1.)  The degenerate cases (zero and one) are treated
  specially because the distance is known (0 for one, and infinity for
  zero).]

  SideEffects [None]

  SeeAlso     [Cudd_bddClosestCube]

******************************************************************************/
DdNode *
cuddBddClosestCube(
  DdManager *dd,
  DdNode *f,
  DdNode *g,
  CUDD_VALUE_TYPE bound)
{
    DdNode *res, *F, *G, *ft, *fe, *gt, *ge, *tt, *ee;
    DdNode *ctt, *cee, *cte, *cet;
    CUDD_VALUE_TYPE minD, dtt, dee, dte, det;
    DdNode *one = DD_ONE(dd);
    DdNode *lzero = Cudd_Not(one);
    DdNode *azero = DD_ZERO(dd);
    unsigned int topf, topg, index;

    statLine(dd);
    if (bound < (f == Cudd_Not(g))) return(azero);
    /* Terminal cases. */
    if (g == lzero || f == lzero) return(azero);
    if (f == one && g == one) return(one);

    /* Check cache. */
    F = Cudd_Regular(f);
    G = Cudd_Regular(g);
    if (F->ref != 1 || G->ref != 1) {
	res = cuddCacheLookup2(dd,(DD_CTFP) Cudd_bddClosestCube, f, g);
	if (res != NULL) return(res);
    }

    topf = cuddI(dd,F->index);
    topg = cuddI(dd,G->index);

    /* Compute cofactors. */
    if (topf <= topg) {
	index = F->index;
	ft = cuddT(F);
	fe = cuddE(F);
	if (Cudd_IsComplement(f)) {
	    ft = Cudd_Not(ft);
	    fe = Cudd_Not(fe);
	}
    } else {
	index = G->index;
	ft = fe = f;
    }

    if (topg <= topf) {
	gt = cuddT(G);
	ge = cuddE(G);
	if (Cudd_IsComplement(g)) {
	    gt = Cudd_Not(gt);
	    ge = Cudd_Not(ge);
	}
    } else {
	gt = ge = g;
    }

    tt = cuddBddClosestCube(dd,ft,gt,bound);
    if (tt == NULL) return(NULL);
    cuddRef(tt);
    ctt = separateCube(dd,tt,&dtt);
    if (ctt == NULL) {
	Cudd_RecursiveDeref(dd, tt);
	return(NULL);
    }
    cuddRef(ctt);
    Cudd_RecursiveDeref(dd, tt);
    minD = dtt;
    bound = ddMin(bound,minD);

    ee = cuddBddClosestCube(dd,fe,ge,bound);
    if (ee == NULL) {
	Cudd_RecursiveDeref(dd, ctt);
	return(NULL);
    }
    cuddRef(ee);
    cee = separateCube(dd,ee,&dee);
    if (cee == NULL) {
	Cudd_RecursiveDeref(dd, ctt);
	Cudd_RecursiveDeref(dd, ee);
	return(NULL);
    }
    cuddRef(cee);
    Cudd_RecursiveDeref(dd, ee);
    minD = ddMin(dtt, dee);
    if (minD <= CUDD_CONST_INDEX) bound = ddMin(bound,minD-1);

    if (minD > 0 && topf == topg) {
	DdNode *te = cuddBddClosestCube(dd,ft,ge,bound-1);
	if (te == NULL) {
	    Cudd_RecursiveDeref(dd, ctt);
	    Cudd_RecursiveDeref(dd, cee);
	    return(NULL);
	}
	cuddRef(te);
	cte = separateCube(dd,te,&dte);
	if (cte == NULL) {
	    Cudd_RecursiveDeref(dd, ctt);
	    Cudd_RecursiveDeref(dd, cee);
	    Cudd_RecursiveDeref(dd, te);
	    return(NULL);
	}
	cuddRef(cte);
	Cudd_RecursiveDeref(dd, te);
	dte += 1.0;
	minD = ddMin(minD, dte);
    } else {
	cte = azero;
	cuddRef(cte);
	dte = CUDD_CONST_INDEX + 1.0;
    }
    if (minD <= CUDD_CONST_INDEX) bound = ddMin(bound,minD-1);

    if (minD > 0 && topf == topg) {
	DdNode *et = cuddBddClosestCube(dd,fe,gt,bound-1);
	if (et == NULL) {
	    Cudd_RecursiveDeref(dd, ctt);
	    Cudd_RecursiveDeref(dd, cee);
	    Cudd_RecursiveDeref(dd, cte);
	    return(NULL);
	}
	cuddRef(et);
	cet = separateCube(dd,et,&det);
	if (cet == NULL) {
	    Cudd_RecursiveDeref(dd, ctt);
	    Cudd_RecursiveDeref(dd, cee);
	    Cudd_RecursiveDeref(dd, cte);
	    Cudd_RecursiveDeref(dd, et);
	    return(NULL);
	}
	cuddRef(cet);
	Cudd_RecursiveDeref(dd, et);
	det += 1.0;
	minD = ddMin(minD, det);
    } else {
	cet = azero;
	cuddRef(cet);
	det = CUDD_CONST_INDEX + 1.0;
    }

    if (minD == dtt) {
	if (dtt == dee && ctt == cee) {
	    res = createResult(dd,CUDD_CONST_INDEX,1,ctt,dtt);
	} else {
	    res = createResult(dd,index,1,ctt,dtt);
	}
    } else if (minD == dee) {
	res = createResult(dd,index,0,cee,dee);
    } else if (minD == dte) {
#ifdef DD_DEBUG
	assert(topf == topg);
#endif
	res = createResult(dd,index,1,cte,dte);
    } else {
#ifdef DD_DEBUG
	assert(topf == topg);
#endif
	res = createResult(dd,index,0,cet,det);
    }
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, ctt);
	Cudd_RecursiveDeref(dd, cee);
	Cudd_RecursiveDeref(dd, cte);
	Cudd_RecursiveDeref(dd, cet);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, ctt);
    Cudd_RecursiveDeref(dd, cee);
    Cudd_RecursiveDeref(dd, cte);
    Cudd_RecursiveDeref(dd, cet);

    /* Only cache results that are different from azero to avoid
    ** storing results that depend on the value of the bound. */
    if ((F->ref != 1 || G->ref != 1) && res != azero)
	cuddCacheInsert2(dd,(DD_CTFP) Cudd_bddClosestCube, f, g, res);

    cuddDeref(res);
    return(res);

} /* end of cuddBddClosestCube */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_MinHammingDist.]

  Description [Performs the recursive step of Cudd_MinHammingDist.
  It is based on the following identity. Let H(f) be the
  minimum Hamming distance of the minterms of f from the reference
  minterm. Then:
  <xmp>
    H(f) = min(H(f0)+h0,H(f1)+h1)
  </xmp>
  where f0 and f1 are the two cofactors of f with respect to its top
  variable; h0 is 1 if the minterm assigns 1 to the top variable of f;
  h1 is 1 if the minterm assigns 0 to the top variable of f.
  The upper bound on the distance is used to bound the depth of the
  recursion.
  Returns the minimum distance unless it exceeds the upper bound or
  computation fails.]

  SideEffects [None]

  SeeAlso     [Cudd_MinHammingDist]

******************************************************************************/
static int
cuddMinHammingDistRecur(
  DdNode * f,
  int *minterm,
  DdHashTable * table,
  int upperBound)
{
    DdNode	*F, *Ft, *Fe;
    double	h, hT, hE;
    DdNode	*zero, *res;
    DdManager	*dd = table->manager;

    statLine(dd);
    if (upperBound == 0) return(0);

    F = Cudd_Regular(f);

    if (cuddIsConstant(F)) {
	zero = Cudd_Not(DD_ONE(dd));
	if (f == dd->background || f == zero) {
	    return(upperBound);
	} else {
	    return(0);
	}
    }
    if ((res = cuddHashTableLookup1(table,f)) != NULL) {
	h = cuddV(res);
	if (res->ref == 0) {
	    dd->dead++;
	    dd->constants.dead++;
	}
	return((int) h);
    }

    Ft = cuddT(F); Fe = cuddE(F);
    if (Cudd_IsComplement(f)) {
	Ft = Cudd_Not(Ft); Fe = Cudd_Not(Fe);
    }
    if (minterm[F->index] == 0) {
	DdNode *temp = Ft;
	Ft = Fe; Fe = temp;
    }

    hT = cuddMinHammingDistRecur(Ft,minterm,table,upperBound);
    if (hT == CUDD_OUT_OF_MEM) return(CUDD_OUT_OF_MEM);
    if (hT == 0) {
	hE = upperBound;
    } else {
	hE = cuddMinHammingDistRecur(Fe,minterm,table,upperBound - 1);
	if (hE == CUDD_OUT_OF_MEM) return(CUDD_OUT_OF_MEM);
    }
    h = ddMin(hT, hE + 1);

    if (F->ref != 1) {
	ptrint fanout = (ptrint) F->ref;
	cuddSatDec(fanout);
	res = cuddUniqueConst(dd, (CUDD_VALUE_TYPE) h);
	if (!cuddHashTableInsert1(table,f,res,fanout)) {
	    cuddRef(res); Cudd_RecursiveDeref(dd, res);
	    return(CUDD_OUT_OF_MEM);
	}
    }

    return((int) h);

} /* end of cuddMinHammingDistRecur */


/**Function********************************************************************

  Synopsis    [Separates cube from distance.]

  Description [Separates cube from distance.  Returns the cube if
  successful; NULL otherwise.]

  SideEffects [The distance is returned as a side effect.]

  SeeAlso     [cuddBddClosestCube createResult]

******************************************************************************/
static DdNode *
separateCube(
  DdManager *dd,
  DdNode *f,
  CUDD_VALUE_TYPE *distance)
{
    DdNode *cube, *t;

    /* One and zero are special cases because the distance is implied. */
    if (Cudd_IsConstant(f)) {
	*distance = (f == DD_ONE(dd)) ? 0.0 :
	    (1.0 + (CUDD_VALUE_TYPE) CUDD_CONST_INDEX);
	return(f);
    }

    /* Find out which branch points to the distance and replace the top
    ** node with one pointing to zero instead. */
    t = cuddT(f);
    if (Cudd_IsConstant(t) && cuddV(t) <= 0) {
#ifdef DD_DEBUG
	assert(!Cudd_IsConstant(cuddE(f)) || cuddE(f) == DD_ONE(dd));
#endif
	*distance = -cuddV(t);
	cube = cuddUniqueInter(dd, f->index, DD_ZERO(dd), cuddE(f));
    } else {
#ifdef DD_DEBUG
	assert(!Cudd_IsConstant(t) || t == DD_ONE(dd));
#endif
	*distance = -cuddV(cuddE(f));
	cube = cuddUniqueInter(dd, f->index, t, DD_ZERO(dd));
    }

    return(cube);

} /* end of separateCube */


/**Function********************************************************************

  Synopsis    [Builds a result for cache storage.]

  Description [Builds a result for cache storage.  Returns a pointer
  to the resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddBddClosestCube separateCube]

******************************************************************************/
static DdNode *
createResult(
  DdManager *dd,
  unsigned int index,
  unsigned int phase,
  DdNode *cube,
  CUDD_VALUE_TYPE distance)
{
    DdNode *res, *constant;

    /* Special case.  The cube is either one or zero, and we do not
    ** add any variables.  Hence, the result is also one or zero,
    ** and the distance remains implied by the value of the constant. */
    if (index == CUDD_CONST_INDEX && Cudd_IsConstant(cube)) return(cube);

    constant = cuddUniqueConst(dd,-distance);
    if (constant == NULL) return(NULL);
    cuddRef(constant);

    if (index == CUDD_CONST_INDEX) {
	/* Replace the top node. */
	if (cuddT(cube) == DD_ZERO(dd)) {
	    res = cuddUniqueInter(dd,cube->index,constant,cuddE(cube));
	} else {
	    res = cuddUniqueInter(dd,cube->index,cuddT(cube),constant);
	}
    } else {
	/* Add a new top node. */
#ifdef DD_DEBUG
	assert(cuddI(dd,index) < cuddI(dd,cube->index));
#endif
	if (phase) {
	    res = cuddUniqueInter(dd,index,cube,constant);
	} else {
	    res = cuddUniqueInter(dd,index,constant,cube);
	}
    }
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, constant);
	return(NULL);
    }
    cuddDeref(constant); /* safe because constant is part of res */

    return(res);

} /* end of createResult */
