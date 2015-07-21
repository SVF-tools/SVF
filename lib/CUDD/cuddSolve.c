/**CFile***********************************************************************

  FileName    [cuddSolve.c]

  PackageName [cudd]

  Synopsis    [Boolean equation solver and related functions.]

  Description [External functions included in this modoule:
		<ul>
		<li> Cudd_SolveEqn()
		<li> Cudd_VerifySol()
		</ul>
	Internal functions included in this module:
		<ul>
		<li> cuddSolveEqnRecur()
		<li> cuddVerifySol()
		</ul> ]

  SeeAlso     []

  Author      [Balakrishna Kumthekar]

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
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Structure declarations                                                    */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddSolve.c,v 1.13 2012/02/05 01:07:19 fabio Exp $";
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

  Synopsis    [Implements the solution of F(x,y) = 0.]

  Description [Implements the solution for F(x,y) = 0. The return
  value is the consistency condition. The y variables are the unknowns
  and the remaining variables are the parameters.  Returns the
  consistency condition if successful; NULL otherwise. Cudd_SolveEqn
  allocates an array and fills it with the indices of the
  unknowns. This array is used by Cudd_VerifySol.]

  SideEffects [The solution is returned in G; the indices of the y
  variables are returned in yIndex.]

  SeeAlso     [Cudd_VerifySol]

******************************************************************************/
DdNode *
Cudd_SolveEqn(
  DdManager *  bdd,
  DdNode * F /* the left-hand side of the equation */,
  DdNode * Y /* the cube of the y variables */,
  DdNode ** G /* the array of solutions (return parameter) */,
  int ** yIndex /* index of y variables */,
  int  n /* numbers of unknowns */)
{
    DdNode *res;
    int *temp;

    *yIndex = temp = ALLOC(int, n);
    if (temp == NULL) {
	bdd->errorCode = CUDD_MEMORY_OUT;
	(void) fprintf(bdd->out,
		       "Cudd_SolveEqn: Out of memory for yIndex\n");
	return(NULL);
    }

    do {
	bdd->reordered = 0;
	res = cuddSolveEqnRecur(bdd, F, Y, G, n, temp, 0);
    } while (bdd->reordered == 1);

    return(res);

} /* end of Cudd_SolveEqn */


/**Function********************************************************************

  Synopsis    [Checks the solution of F(x,y) = 0.]

  Description [Checks the solution of F(x,y) = 0. This procedure 
  substitutes the solution components for the unknowns of F and returns 
  the resulting BDD for F.] 

  SideEffects [Frees the memory pointed by yIndex.]

  SeeAlso     [Cudd_SolveEqn]

******************************************************************************/
DdNode *
Cudd_VerifySol(
  DdManager *  bdd,
  DdNode * F /* the left-hand side of the equation */,
  DdNode ** G /* the array of solutions */,
  int * yIndex /* index of y variables */,
  int  n /* numbers of unknowns */)
{
    DdNode *res;

    do {
	bdd->reordered = 0;
	res = cuddVerifySol(bdd, F, G, yIndex, n);
    } while (bdd->reordered == 1);

    FREE(yIndex);

    return(res);

} /* end of Cudd_VerifySol */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_SolveEqn.]

  Description [Implements the recursive step of Cudd_SolveEqn. 
  Returns NULL if the intermediate solution blows up
  or reordering occurs. The parametric solutions are
  stored in the array G.]

  SideEffects [none]

  SeeAlso     [Cudd_SolveEqn, Cudd_VerifySol]

******************************************************************************/
DdNode *
cuddSolveEqnRecur(
  DdManager * bdd,
  DdNode * F /* the left-hand side of the equation */,
  DdNode * Y /* the cube of remaining y variables */,
  DdNode ** G /* the array of solutions */,
  int  n /* number of unknowns */,
  int * yIndex /* array holding the y variable indices */,
  int  i /* level of recursion */)
{
    DdNode *Fn, *Fm1, *Fv, *Fvbar, *T, *w, *nextY, *one;
    DdNodePtr *variables;

    int j;

    statLine(bdd);
    variables = bdd->vars;
    one = DD_ONE(bdd);

    /* Base condition. */
    if (Y == one) {
	return F;
    }

    /* Cofactor of Y. */
    yIndex[i] = Y->index;
    nextY = Cudd_T(Y);

    /* Universal abstraction of F with respect to the top variable index. */
    Fm1 = cuddBddExistAbstractRecur(bdd, Cudd_Not(F), variables[yIndex[i]]);
    if (Fm1) {
	Fm1 = Cudd_Not(Fm1);
	cuddRef(Fm1);
    } else {
	return(NULL);
    }

    Fn = cuddSolveEqnRecur(bdd, Fm1, nextY, G, n, yIndex, i+1);
    if (Fn) {
	cuddRef(Fn);
    } else {
	Cudd_RecursiveDeref(bdd, Fm1);
	return(NULL);
    }

    Fv = cuddCofactorRecur(bdd, F, variables[yIndex[i]]);
    if (Fv) {
	cuddRef(Fv);
    } else {
	Cudd_RecursiveDeref(bdd, Fm1);
	Cudd_RecursiveDeref(bdd, Fn);
	return(NULL);
    }

    Fvbar = cuddCofactorRecur(bdd, F, Cudd_Not(variables[yIndex[i]]));
    if (Fvbar) {
	cuddRef(Fvbar);
    } else {
	Cudd_RecursiveDeref(bdd, Fm1);
	Cudd_RecursiveDeref(bdd, Fn);
	Cudd_RecursiveDeref(bdd, Fv);
	return(NULL);
    }

    /* Build i-th component of the solution. */
    w = cuddBddIteRecur(bdd, variables[yIndex[i]], Cudd_Not(Fv), Fvbar);
    if (w) {
	cuddRef(w);
    } else {
	Cudd_RecursiveDeref(bdd, Fm1);
	Cudd_RecursiveDeref(bdd, Fn);
	Cudd_RecursiveDeref(bdd, Fv);
	Cudd_RecursiveDeref(bdd, Fvbar);
	return(NULL);
    }

    T = cuddBddRestrictRecur(bdd, w, Cudd_Not(Fm1));
    if(T) {
	cuddRef(T);
    } else {
	Cudd_RecursiveDeref(bdd, Fm1);
	Cudd_RecursiveDeref(bdd, Fn);
	Cudd_RecursiveDeref(bdd, Fv);
	Cudd_RecursiveDeref(bdd, Fvbar);
	Cudd_RecursiveDeref(bdd, w);
	return(NULL);
    }

    Cudd_RecursiveDeref(bdd,Fm1);
    Cudd_RecursiveDeref(bdd,w);
    Cudd_RecursiveDeref(bdd,Fv);
    Cudd_RecursiveDeref(bdd,Fvbar);

    /* Substitute components of solution already found into solution. */
    for (j = n-1; j > i; j--) {
	w = cuddBddComposeRecur(bdd,T, G[j], variables[yIndex[j]]);
	if(w) {
	    cuddRef(w);
	} else {
	    Cudd_RecursiveDeref(bdd, Fn);
	    Cudd_RecursiveDeref(bdd, T);
	    return(NULL);
	}
	Cudd_RecursiveDeref(bdd,T);
	T = w;
    }
    G[i] = T;

    Cudd_Deref(Fn);

    return(Fn);

} /* end of cuddSolveEqnRecur */


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_VerifySol. ]

  Description []

  SideEffects [none]

  SeeAlso     [Cudd_VerifySol]

******************************************************************************/
DdNode *
cuddVerifySol(
  DdManager * bdd,
  DdNode * F /* the left-hand side of the equation */,
  DdNode ** G /* the array of solutions */,
  int * yIndex /* array holding the y variable indices */,
  int  n /* number of unknowns */)
{
    DdNode *w, *R;

    int j;

    R = F;
    cuddRef(R);
    for(j = n - 1; j >= 0; j--) {
	 w = Cudd_bddCompose(bdd, R, G[j], yIndex[j]);
	if (w) {
	    cuddRef(w);
	} else {
	    return(NULL); 
	}
	Cudd_RecursiveDeref(bdd,R);
	R = w;
    }

    cuddDeref(R);

    return(R);

} /* end of cuddVerifySol */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

