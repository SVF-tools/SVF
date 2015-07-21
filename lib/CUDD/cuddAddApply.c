/**CFile***********************************************************************

  FileName    [cuddAddApply.c]

  PackageName [cudd]

  Synopsis    [Apply functions for ADDs and their operators.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_addApply()
		<li> Cudd_addMonadicApply()
		<li> Cudd_addPlus()
		<li> Cudd_addTimes()
		<li> Cudd_addThreshold()
		<li> Cudd_addSetNZ()
		<li> Cudd_addDivide()
		<li> Cudd_addMinus()
		<li> Cudd_addMinimum()
		<li> Cudd_addMaximum()
		<li> Cudd_addOneZeroMaximum()
		<li> Cudd_addDiff()
		<li> Cudd_addAgreement()
		<li> Cudd_addOr()
		<li> Cudd_addNand()
		<li> Cudd_addNor()
		<li> Cudd_addXor()
		<li> Cudd_addXnor()
		</ul>
	    Internal procedures included in this module:
		<ul>
		<li> cuddAddApplyRecur()
		<li> cuddAddMonadicApplyRecur()
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
static char rcsid[] DD_UNUSED = "$Id: cuddAddApply.c,v 1.19 2012/02/05 01:07:18 fabio Exp $";
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

  Synopsis    [Applies op to the corresponding discriminants of f and g.]

  Description [Applies op to the corresponding discriminants of f and g.
  Returns a pointer to the result if succssful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addMonadicApply Cudd_addPlus Cudd_addTimes
  Cudd_addThreshold Cudd_addSetNZ Cudd_addDivide Cudd_addMinus Cudd_addMinimum
  Cudd_addMaximum Cudd_addOneZeroMaximum Cudd_addDiff Cudd_addAgreement
  Cudd_addOr Cudd_addNand Cudd_addNor Cudd_addXor Cudd_addXnor]

******************************************************************************/
DdNode *
Cudd_addApply(
  DdManager * dd,
  DD_AOP op,
  DdNode * f,
  DdNode * g)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddAddApplyRecur(dd,op,f,g);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addApply */


/**Function********************************************************************

  Synopsis    [Integer and floating point addition.]

  Description [Integer and floating point addition. Returns NULL if not
  a terminal case; f+g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addPlus(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *res;
    DdNode *F, *G;
    CUDD_VALUE_TYPE value;

    F = *f; G = *g;
    if (F == DD_ZERO(dd)) return(G);
    if (G == DD_ZERO(dd)) return(F);
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	value = cuddV(F)+cuddV(G);
	res = cuddUniqueConst(dd,value);
	return(res);
    }
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addPlus */


/**Function********************************************************************

  Synopsis    [Integer and floating point multiplication.]

  Description [Integer and floating point multiplication. Returns NULL
  if not a terminal case; f * g otherwise.  This function can be used also
  to take the AND of two 0-1 ADDs.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addTimes(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *res;
    DdNode *F, *G;
    CUDD_VALUE_TYPE value;

    F = *f; G = *g;
    if (F == DD_ZERO(dd) || G == DD_ZERO(dd)) return(DD_ZERO(dd));
    if (F == DD_ONE(dd)) return(G);
    if (G == DD_ONE(dd)) return(F);
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	value = cuddV(F)*cuddV(G);
	res = cuddUniqueConst(dd,value);
	return(res);
    }
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addTimes */


/**Function********************************************************************

  Synopsis    [f if f&gt;=g; 0 if f&lt;g.]

  Description [Threshold operator for Apply (f if f &gt;=g; 0 if f&lt;g).
  Returns NULL if not a terminal case; f op g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addThreshold(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G || F == DD_PLUS_INFINITY(dd)) return(F);
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	if (cuddV(F) >= cuddV(G)) {
	    return(F);
	} else {
	    return(DD_ZERO(dd));
	}
    }
    return(NULL);

} /* end of Cudd_addThreshold */


/**Function********************************************************************

  Synopsis    [This operator sets f to the value of g wherever g != 0.]

  Description [This operator sets f to the value of g wherever g != 0.
  Returns NULL if not a terminal case; f op g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addSetNZ(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G) return(F);
    if (F == DD_ZERO(dd)) return(G);
    if (G == DD_ZERO(dd)) return(F);
    if (cuddIsConstant(G)) return(G);
    return(NULL);

} /* end of Cudd_addSetNZ */


/**Function********************************************************************

  Synopsis    [Integer and floating point division.]

  Description [Integer and floating point division. Returns NULL if not
  a terminal case; f / g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addDivide(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *res;
    DdNode *F, *G;
    CUDD_VALUE_TYPE value;

    F = *f; G = *g;
    /* We would like to use F == G -> F/G == 1, but F and G may
    ** contain zeroes. */
    if (F == DD_ZERO(dd)) return(DD_ZERO(dd));
    if (G == DD_ONE(dd)) return(F);
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	value = cuddV(F)/cuddV(G);
	res = cuddUniqueConst(dd,value);
	return(res);
    }
    return(NULL);

} /* end of Cudd_addDivide */


/**Function********************************************************************

  Synopsis    [Integer and floating point subtraction.]

  Description [Integer and floating point subtraction. Returns NULL if
  not a terminal case; f - g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addMinus(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *res;
    DdNode *F, *G;
    CUDD_VALUE_TYPE value;

    F = *f; G = *g;
    if (F == G) return(DD_ZERO(dd));
    if (F == DD_ZERO(dd)) return(cuddAddNegateRecur(dd,G));
    if (G == DD_ZERO(dd)) return(F);
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	value = cuddV(F)-cuddV(G);
	res = cuddUniqueConst(dd,value);
	return(res);
    }
    return(NULL);

} /* end of Cudd_addMinus */


/**Function********************************************************************

  Synopsis    [Integer and floating point min.]

  Description [Integer and floating point min for Cudd_addApply.
  Returns NULL if not a terminal case; min(f,g) otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addMinimum(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == DD_PLUS_INFINITY(dd)) return(G);
    if (G == DD_PLUS_INFINITY(dd)) return(F);
    if (F == G) return(F);
#if 0
    /* These special cases probably do not pay off. */
    if (F == DD_MINUS_INFINITY(dd)) return(F);
    if (G == DD_MINUS_INFINITY(dd)) return(G);
#endif
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	if (cuddV(F) <= cuddV(G)) {
	    return(F);
	} else {
	    return(G);
	}
    }
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addMinimum */


/**Function********************************************************************

  Synopsis    [Integer and floating point max.]

  Description [Integer and floating point max for Cudd_addApply.
  Returns NULL if not a terminal case; max(f,g) otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addMaximum(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G) return(F);
    if (F == DD_MINUS_INFINITY(dd)) return(G);
    if (G == DD_MINUS_INFINITY(dd)) return(F);
#if 0
    /* These special cases probably do not pay off. */
    if (F == DD_PLUS_INFINITY(dd)) return(F);
    if (G == DD_PLUS_INFINITY(dd)) return(G);
#endif
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	if (cuddV(F) >= cuddV(G)) {
	    return(F);
	} else {
	    return(G);
	}
    }
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addMaximum */


/**Function********************************************************************

  Synopsis    [Returns 1 if f &gt; g and 0 otherwise.]

  Description [Returns 1 if f &gt; g and 0 otherwise. Used in
  conjunction with Cudd_addApply. Returns NULL if not a terminal
  case.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addOneZeroMaximum(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{

    if (*f == *g) return(DD_ZERO(dd));
    if (*g == DD_PLUS_INFINITY(dd))
	return DD_ZERO(dd);
    if (cuddIsConstant(*f) && cuddIsConstant(*g)) {
	if (cuddV(*f) > cuddV(*g)) {
	    return(DD_ONE(dd));
	} else {
	    return(DD_ZERO(dd));
	}
    }

    return(NULL);

} /* end of Cudd_addOneZeroMaximum */


/**Function********************************************************************

  Synopsis    [Returns plusinfinity if f=g; returns min(f,g) if f!=g.]

  Description [Returns NULL if not a terminal case; f op g otherwise,
  where f op g is plusinfinity if f=g; min(f,g) if f!=g.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addDiff(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G) return(DD_PLUS_INFINITY(dd));
    if (F == DD_PLUS_INFINITY(dd)) return(G);
    if (G == DD_PLUS_INFINITY(dd)) return(F);
    if (cuddIsConstant(F) && cuddIsConstant(G)) {
	if (cuddV(F) != cuddV(G)) {
	    if (cuddV(F) < cuddV(G)) {
		return(F);
	    } else {
		return(G);
	    }
	} else {
	    return(DD_PLUS_INFINITY(dd));
	}
    }
    return(NULL);

} /* end of Cudd_addDiff */


/**Function********************************************************************

  Synopsis    [f if f==g; background if f!=g.]

  Description [Returns NULL if not a terminal case; f op g otherwise,
  where f op g is f if f==g; background if f!=g.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addAgreement(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G) return(F);
    if (F == dd->background) return(F);
    if (G == dd->background) return(G);
    if (cuddIsConstant(F) && cuddIsConstant(G)) return(dd->background);
    return(NULL);

} /* end of Cudd_addAgreement */


/**Function********************************************************************

  Synopsis    [Disjunction of two 0-1 ADDs.]

  Description [Disjunction of two 0-1 ADDs. Returns NULL
  if not a terminal case; f OR g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addOr(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == DD_ONE(dd) || G == DD_ONE(dd)) return(DD_ONE(dd));
    if (cuddIsConstant(F)) return(G);
    if (cuddIsConstant(G)) return(F);
    if (F == G) return(F);
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addOr */


/**Function********************************************************************

  Synopsis    [NAND of two 0-1 ADDs.]

  Description [NAND of two 0-1 ADDs. Returns NULL
  if not a terminal case; f NAND g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addNand(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == DD_ZERO(dd) || G == DD_ZERO(dd)) return(DD_ONE(dd));
    if (cuddIsConstant(F) && cuddIsConstant(G)) return(DD_ZERO(dd));
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addNand */


/**Function********************************************************************

  Synopsis    [NOR of two 0-1 ADDs.]

  Description [NOR of two 0-1 ADDs. Returns NULL
  if not a terminal case; f NOR g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addNor(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == DD_ONE(dd) || G == DD_ONE(dd)) return(DD_ZERO(dd));
    if (cuddIsConstant(F) && cuddIsConstant(G)) return(DD_ONE(dd));
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addNor */


/**Function********************************************************************

  Synopsis    [XOR of two 0-1 ADDs.]

  Description [XOR of two 0-1 ADDs. Returns NULL
  if not a terminal case; f XOR g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addXor(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G) return(DD_ZERO(dd));
    if (F == DD_ONE(dd) && G == DD_ZERO(dd)) return(DD_ONE(dd));
    if (G == DD_ONE(dd) && F == DD_ZERO(dd)) return(DD_ONE(dd));
    if (cuddIsConstant(F) && cuddIsConstant(G)) return(DD_ZERO(dd));
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addXor */


/**Function********************************************************************

  Synopsis    [XNOR of two 0-1 ADDs.]

  Description [XNOR of two 0-1 ADDs. Returns NULL
  if not a terminal case; f XNOR g otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply]

******************************************************************************/
DdNode *
Cudd_addXnor(
  DdManager * dd,
  DdNode ** f,
  DdNode ** g)
{
    DdNode *F, *G;

    F = *f; G = *g;
    if (F == G) return(DD_ONE(dd));
    if (F == DD_ONE(dd) && G == DD_ONE(dd)) return(DD_ONE(dd));
    if (G == DD_ZERO(dd) && F == DD_ZERO(dd)) return(DD_ONE(dd));
    if (cuddIsConstant(F) && cuddIsConstant(G)) return(DD_ZERO(dd));
    if (F > G) { /* swap f and g */
	*f = G;
	*g = F;
    }
    return(NULL);

} /* end of Cudd_addXnor */


/**Function********************************************************************

  Synopsis    [Applies op to the discriminants of f.]

  Description [Applies op to the discriminants of f.
  Returns a pointer to the result if succssful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addApply Cudd_addLog]

******************************************************************************/
DdNode *
Cudd_addMonadicApply(
  DdManager * dd,
  DD_MAOP op,
  DdNode * f)
{
    DdNode *res;

    do {
	dd->reordered = 0;
	res = cuddAddMonadicApplyRecur(dd,op,f);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addMonadicApply */


/**Function********************************************************************

  Synopsis    [Natural logarithm of an ADD.]

  Description [Natural logarithm of an ADDs. Returns NULL
  if not a terminal case; log(f) otherwise.  The discriminants of f must
  be positive double's.]

  SideEffects [None]

  SeeAlso     [Cudd_addMonadicApply]

******************************************************************************/
DdNode *
Cudd_addLog(
  DdManager * dd,
  DdNode * f)
{
    if (cuddIsConstant(f)) {
	CUDD_VALUE_TYPE value = log(cuddV(f));
	DdNode *res = cuddUniqueConst(dd,value);
	return(res);
    }
    return(NULL);

} /* end of Cudd_addLog */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addApply.]

  Description [Performs the recursive step of Cudd_addApply. Returns a
  pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddAddMonadicApplyRecur]

******************************************************************************/
DdNode *
cuddAddApplyRecur(
  DdManager * dd,
  DD_AOP op,
  DdNode * f,
  DdNode * g)
{
    DdNode *res,
	   *fv, *fvn, *gv, *gvn,
	   *T, *E;
    unsigned int ford, gord;
    unsigned int index;
    DD_CTFP cacheOp;

    /* Check terminal cases. Op may swap f and g to increase the
     * cache hit rate.
     */
    statLine(dd);
    res = (*op)(dd,&f,&g);
    if (res != NULL) return(res);

    /* Check cache. */
    cacheOp = (DD_CTFP) op;
    res = cuddCacheLookup2(dd,cacheOp,f,g);
    if (res != NULL) return(res);

    /* Recursive step. */
    ford = cuddI(dd,f->index);
    gord = cuddI(dd,g->index);
    if (ford <= gord) {
	index = f->index;
	fv = cuddT(f);
	fvn = cuddE(f);
    } else {
	index = g->index;
	fv = fvn = f;
    }
    if (gord <= ford) {
	gv = cuddT(g);
	gvn = cuddE(g);
    } else {
	gv = gvn = g;
    }

    T = cuddAddApplyRecur(dd,op,fv,gv);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = cuddAddApplyRecur(dd,op,fvn,gvn);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd,T);
	return(NULL);
    }
    cuddRef(E);

    res = (T == E) ? T : cuddUniqueInter(dd,(int)index,T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, T);
	Cudd_RecursiveDeref(dd, E);
	return(NULL);
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert2(dd,cacheOp,f,g,res);

    return(res);

} /* end of cuddAddApplyRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addMonadicApply.]

  Description [Performs the recursive step of Cudd_addMonadicApply. Returns a
  pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddAddApplyRecur]

******************************************************************************/
DdNode *
cuddAddMonadicApplyRecur(
  DdManager * dd,
  DD_MAOP op,
  DdNode * f)
{
    DdNode *res, *ft, *fe, *T, *E;
    unsigned int index;

    /* Check terminal cases. */
    statLine(dd);
    res = (*op)(dd,f);
    if (res != NULL) return(res);

    /* Check cache. */
    res = cuddCacheLookup1(dd,op,f);
    if (res != NULL) return(res);

    /* Recursive step. */
    index = f->index;
    ft = cuddT(f);
    fe = cuddE(f);

    T = cuddAddMonadicApplyRecur(dd,op,ft);
    if (T == NULL) return(NULL);
    cuddRef(T);

    E = cuddAddMonadicApplyRecur(dd,op,fe);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd,T);
	return(NULL);
    }
    cuddRef(E);

    res = (T == E) ? T : cuddUniqueInter(dd,(int)index,T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, T);
	Cudd_RecursiveDeref(dd, E);
	return(NULL);
    }
    cuddDeref(T);
    cuddDeref(E);

    /* Store result. */
    cuddCacheInsert1(dd,op,f,res);

    return(res);

} /* end of cuddAddMonadicApplyRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
