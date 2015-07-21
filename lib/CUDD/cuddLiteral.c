/**CFile***********************************************************************

  FileName    [cuddLiteral.c]

  PackageName [cudd]

  Synopsis    [Functions for manipulation of literal sets represented by
  BDDs.]

  Description [External procedures included in this file:
		<ul>
		<li> Cudd_bddLiteralSetIntersection()
		</ul>
	    Internal procedures included in this file:
		<ul>
		<li> cuddBddLiteralSetIntersectionRecur()
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
static char rcsid[] DD_UNUSED = "$Id: cuddLiteral.c,v 1.9 2012/02/05 01:07:19 fabio Exp $";
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

  Synopsis    [Computes the intesection of two sets of literals
  represented as BDDs.]

  Description [Computes the intesection of two sets of literals
  represented as BDDs. Each set is represented as a cube of the
  literals in the set. The empty set is represented by the constant 1.
  No variable can be simultaneously present in both phases in a set.
  Returns a pointer to the BDD representing the intersected sets, if
  successful; NULL otherwise.]

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_bddLiteralSetIntersection(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddBddLiteralSetIntersectionRecur(dd,f,g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddLiteralSetIntersection */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of
  Cudd_bddLiteralSetIntersection.]

  Description [Performs the recursive step of
  Cudd_bddLiteralSetIntersection. Scans the cubes for common variables,
  and checks whether they agree in phase.  Returns a pointer to the
  resulting cube if successful; NULL otherwise.]

  SideEffects [None]

******************************************************************************/
DdNode *
cuddBddLiteralSetIntersectionRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g)
{
    DdNode *res, *tmp;
    DdNode *F, *G;
    DdNode *fc, *gc;
    DdNode *one;
    DdNode *zero;
    unsigned int topf, topg, comple;
    int phasef, phaseg;

    statLine(dd);
    if (f == g) return(f);

    F = Cudd_Regular(f);
    G = Cudd_Regular(g);
    one = DD_ONE(dd);

    /* Here f != g. If F == G, then f and g are complementary.
    ** Since they are two cubes, this case only occurs when f == v,
    ** g == v', and v is a variable or its complement.
    */
    if (F == G) return(one);

    zero = Cudd_Not(one);
    topf = cuddI(dd,F->index);
    topg = cuddI(dd,G->index);
    /* Look for a variable common to both cubes. If there are none, this
    ** loop will stop when the constant node is reached in both cubes.
    */
    while (topf != topg) {
	if (topf < topg) {	/* move down on f */
	    comple = f != F;
	    f = cuddT(F);
	    if (comple) f = Cudd_Not(f);
	    if (f == zero) {
		f = cuddE(F);
		if (comple) f = Cudd_Not(f);
	    }
	    F = Cudd_Regular(f);
	    topf = cuddI(dd,F->index);
	} else if (topg < topf) {
	    comple = g != G;
	    g = cuddT(G);
	    if (comple) g = Cudd_Not(g);
	    if (g == zero) {
		g = cuddE(G);
		if (comple) g = Cudd_Not(g);
	    }
	    G = Cudd_Regular(g);
	    topg = cuddI(dd,G->index);
	}
    }

    /* At this point, f == one <=> g == 1. It suffices to test one of them. */
    if (f == one) return(one);

    res = cuddCacheLookup2(dd,Cudd_bddLiteralSetIntersection,f,g);
    if (res != NULL) {
	return(res);
    }

    /* Here f and g are both non constant and have the same top variable. */
    comple = f != F;
    fc = cuddT(F);
    phasef = 1;
    if (comple) fc = Cudd_Not(fc);
    if (fc == zero) {
	fc = cuddE(F);
	phasef = 0;
	if (comple) fc = Cudd_Not(fc);
    }
    comple = g != G;
    gc = cuddT(G);
    phaseg = 1;
    if (comple) gc = Cudd_Not(gc);
    if (gc == zero) {
	gc = cuddE(G);
	phaseg = 0;
	if (comple) gc = Cudd_Not(gc);
    }

    tmp = cuddBddLiteralSetIntersectionRecur(dd,fc,gc);
    if (tmp == NULL) {
	return(NULL);
    }

    if (phasef != phaseg) {
	res = tmp;
    } else {
	cuddRef(tmp);
	if (phasef == 0) {
	    res = cuddBddAndRecur(dd,Cudd_Not(dd->vars[F->index]),tmp);
	} else {
	    res = cuddBddAndRecur(dd,dd->vars[F->index],tmp);
	}
	if (res == NULL) {
	    Cudd_RecursiveDeref(dd,tmp);
	    return(NULL);
	}
	cuddDeref(tmp); /* Just cuddDeref, because it is included in result */
    }

    cuddCacheInsert2(dd,Cudd_bddLiteralSetIntersection,f,g,res);

    return(res);

} /* end of cuddBddLiteralSetIntersectionRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

