/**CFile***********************************************************************

  FileName    [cuddCompose.c]

  PackageName [cudd]

  Synopsis    [Functional composition and variable permutation of DDs.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_bddCompose()
		<li> Cudd_addCompose()
		<li> Cudd_addPermute()
		<li> Cudd_addSwapVariables()
		<li> Cudd_bddPermute()
		<li> Cudd_bddVarMap()
		<li> Cudd_SetVarMap()
		<li> Cudd_bddSwapVariables()
		<li> Cudd_bddAdjPermuteX()
		<li> Cudd_addVectorCompose()
		<li> Cudd_addGeneralVectorCompose()
		<li> Cudd_addNonSimCompose()
		<li> Cudd_bddVectorCompose()
		</ul>
	       Internal procedures included in this module:
		<ul>
		<li> cuddBddComposeRecur()
		<li> cuddAddComposeRecur()
		</ul>
	       Static procedures included in this module:
		<ul>
		<li> cuddAddPermuteRecur()
		<li> cuddBddPermuteRecur()
		<li> cuddBddVarMapRecur()
		<li> cuddAddVectorComposeRecur()
		<li> cuddAddGeneralVectorComposeRecur()
		<li> cuddAddNonSimComposeRecur()
		<li> cuddBddVectorComposeRecur()
		<li> ddIsIthAddVar()
		<li> ddIsIthAddVarPair()
	       </ul>
  The permutation functions use a local cache because the results to
  be remembered depend on the permutation being applied.  Since the
  permutation is just an array, it cannot be stored in the global
  cache. There are different procedured for BDDs and ADDs. This is
  because bddPermuteRecur uses cuddBddIteRecur. If this were changed,
  the procedures could be merged.]

  Author      [Fabio Somenzi and Kavita Ravi]

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
static char rcsid[] DD_UNUSED = "$Id: cuddCompose.c,v 1.46 2012/02/05 01:07:18 fabio Exp $";
#endif

#ifdef DD_DEBUG
static int addPermuteRecurHits;
static int bddPermuteRecurHits;
static int bddVectorComposeHits;
static int addVectorComposeHits;

static int addGeneralVectorComposeHits;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdNode * cuddAddPermuteRecur (DdManager *manager, DdHashTable *table, DdNode *node, int *permut);
static DdNode * cuddBddPermuteRecur (DdManager *manager, DdHashTable *table, DdNode *node, int *permut);
static DdNode * cuddBddVarMapRecur (DdManager *manager, DdNode *f);
static DdNode * cuddAddVectorComposeRecur (DdManager *dd, DdHashTable *table, DdNode *f, DdNode **vector, int deepest);
static DdNode * cuddAddNonSimComposeRecur (DdManager *dd, DdNode *f, DdNode **vector, DdNode *key, DdNode *cube, int lastsub);
static DdNode * cuddBddVectorComposeRecur (DdManager *dd, DdHashTable *table, DdNode *f, DdNode **vector, int deepest);
DD_INLINE static int ddIsIthAddVar (DdManager *dd, DdNode *f, unsigned int i);

static DdNode * cuddAddGeneralVectorComposeRecur (DdManager *dd, DdHashTable *table, DdNode *f, DdNode **vectorOn, DdNode **vectorOff, int deepest);
DD_INLINE static int ddIsIthAddVarPair (DdManager *dd, DdNode *f, DdNode *g, unsigned int i);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Substitutes g for x_v in the BDD for f.]

  Description [Substitutes g for x_v in the BDD for f. v is the index of the
  variable to be substituted. Cudd_bddCompose passes the corresponding
  projection function to the recursive procedure, so that the cache may
  be used.  Returns the composed BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addCompose]

******************************************************************************/
DdNode *
Cudd_bddCompose(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  int  v)
{
    DdNode *proj, *res;

    /* Sanity check. */
    if (v < 0 || v >= dd->size) return(NULL);

    proj =  dd->vars[v];
    do {
	dd->reordered = 0;
	res = cuddBddComposeRecur(dd,f,g,proj);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_bddCompose */


/**Function********************************************************************

  Synopsis    [Substitutes g for x_v in the ADD for f.]

  Description [Substitutes g for x_v in the ADD for f. v is the index of the
  variable to be substituted. g must be a 0-1 ADD. Cudd_bddCompose passes
  the corresponding projection function to the recursive procedure, so
  that the cache may be used.  Returns the composed ADD if successful;
  NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddCompose]

******************************************************************************/
DdNode *
Cudd_addCompose(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  int  v)
{
    DdNode *proj, *res;

    /* Sanity check. */
    if (v < 0 || v >= dd->size) return(NULL);

    proj =  dd->vars[v];
    do {
	dd->reordered = 0;
	res = cuddAddComposeRecur(dd,f,g,proj);
    } while (dd->reordered == 1);
    return(res);

} /* end of Cudd_addCompose */


/**Function********************************************************************

  Synopsis    [Permutes the variables of an ADD.]

  Description [Given a permutation in array permut, creates a new ADD
  with permuted variables. There should be an entry in array permut
  for each variable in the manager. The i-th entry of permut holds the
  index of the variable that is to substitute the i-th
  variable. Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPermute Cudd_addSwapVariables]

******************************************************************************/
DdNode *
Cudd_addPermute(
  DdManager * manager,
  DdNode * node,
  int * permut)
{
    DdHashTable		*table;
    DdNode		*res;

    do {
	manager->reordered = 0;
	table = cuddHashTableInit(manager,1,2);
	if (table == NULL) return(NULL);
	/* Recursively solve the problem. */
	res = cuddAddPermuteRecur(manager,table,node,permut);
	if (res != NULL) cuddRef(res);
	/* Dispose of local cache. */
	cuddHashTableQuit(table);
    } while (manager->reordered == 1);

    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_addPermute */


/**Function********************************************************************

  Synopsis [Swaps two sets of variables of the same size (x and y) in
  the ADD f.]

  Description [Swaps two sets of variables of the same size (x and y) in
  the ADD f.  The size is given by n. The two sets of variables are
  assumed to be disjoint. Returns a pointer to the resulting ADD if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addPermute Cudd_bddSwapVariables]

******************************************************************************/
DdNode *
Cudd_addSwapVariables(
  DdManager * dd,
  DdNode * f,
  DdNode ** x,
  DdNode ** y,
  int  n)
{
    DdNode *swapped;
    int	 i, j, k;
    int	 *permut;

    permut = ALLOC(int,dd->size);
    if (permut == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < dd->size; i++) permut[i] = i;
    for (i = 0; i < n; i++) {
	j = x[i]->index;
	k = y[i]->index;
	permut[j] = k;
	permut[k] = j;
    }

    swapped = Cudd_addPermute(dd,f,permut);
    FREE(permut);

    return(swapped);

} /* end of Cudd_addSwapVariables */


/**Function********************************************************************

  Synopsis    [Permutes the variables of a BDD.]

  Description [Given a permutation in array permut, creates a new BDD
  with permuted variables. There should be an entry in array permut
  for each variable in the manager. The i-th entry of permut holds the
  index of the variable that is to substitute the i-th variable.
  Returns a pointer to the resulting BDD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addPermute Cudd_bddSwapVariables]

******************************************************************************/
DdNode *
Cudd_bddPermute(
  DdManager * manager,
  DdNode * node,
  int * permut)
{
    DdHashTable		*table;
    DdNode		*res;

    do {
	manager->reordered = 0;
	table = cuddHashTableInit(manager,1,2);
	if (table == NULL) return(NULL);
	res = cuddBddPermuteRecur(manager,table,node,permut);
	if (res != NULL) cuddRef(res);
	/* Dispose of local cache. */
	cuddHashTableQuit(table);

    } while (manager->reordered == 1);

    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_bddPermute */


/**Function********************************************************************

  Synopsis    [Remaps the variables of a BDD using the default variable map.]

  Description [Remaps the variables of a BDD using the default
  variable map.  A typical use of this function is to swap two sets of
  variables.  The variable map must be registered with Cudd_SetVarMap.
  Returns a pointer to the resulting BDD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPermute Cudd_bddSwapVariables Cudd_SetVarMap]

******************************************************************************/
DdNode *
Cudd_bddVarMap(
  DdManager * manager /* DD manager */,
  DdNode * f /* function in which to remap variables */)
{
    DdNode *res;

    if (manager->map == NULL) return(NULL);
    do {
	manager->reordered = 0;
	res = cuddBddVarMapRecur(manager, f);
    } while (manager->reordered == 1);

    return(res);

} /* end of Cudd_bddVarMap */


/**Function********************************************************************

  Synopsis [Registers a variable mapping with the manager.]

  Description [Registers with the manager a variable mapping described
  by two sets of variables.  This variable mapping is then used by
  functions like Cudd_bddVarMap.  This function is convenient for
  those applications that perform the same mapping several times.
  However, if several different permutations are used, it may be more
  efficient not to rely on the registered mapping, because changing
  mapping causes the cache to be cleared.  (The initial setting,
  however, does not clear the cache.) The two sets of variables (x and
  y) must have the same size (x and y).  The size is given by n. The
  two sets of variables are normally disjoint, but this restriction is
  not imposeded by the function. When new variables are created, the
  map is automatically extended (each new variable maps to
  itself). The typical use, however, is to wait until all variables
  are created, and then create the map.  Returns 1 if the mapping is
  successfully registered with the manager; 0 otherwise.]

  SideEffects [Modifies the manager. May clear the cache.]

  SeeAlso     [Cudd_bddVarMap Cudd_bddPermute Cudd_bddSwapVariables]

******************************************************************************/
int
Cudd_SetVarMap (
  DdManager *manager /* DD manager */,
  DdNode **x /* first array of variables */,
  DdNode **y /* second array of variables */,
  int n /* length of both arrays */)
{
    int i;

    if (manager->map != NULL) {
	cuddCacheFlush(manager);
    } else {
	manager->map = ALLOC(int,manager->maxSize);
	if (manager->map == NULL) {
	    manager->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	manager->memused += sizeof(int) * manager->maxSize;
    }
    /* Initialize the map to the identity. */
    for (i = 0; i < manager->size; i++) {
	manager->map[i] = i;
    }
    /* Create the map. */
    for (i = 0; i < n; i++) {
	manager->map[x[i]->index] = y[i]->index;
	manager->map[y[i]->index] = x[i]->index;
    }
    return(1);

} /* end of Cudd_SetVarMap */


/**Function********************************************************************

  Synopsis [Swaps two sets of variables of the same size (x and y) in
  the BDD f.]

  Description [Swaps two sets of variables of the same size (x and y)
  in the BDD f. The size is given by n. The two sets of variables are
  assumed to be disjoint.  Returns a pointer to the resulting BDD if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPermute Cudd_addSwapVariables]

******************************************************************************/
DdNode *
Cudd_bddSwapVariables(
  DdManager * dd,
  DdNode * f,
  DdNode ** x,
  DdNode ** y,
  int  n)
{
    DdNode *swapped;
    int	 i, j, k;
    int	 *permut;

    permut = ALLOC(int,dd->size);
    if (permut == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < dd->size; i++) permut[i] = i;
    for (i = 0; i < n; i++) {
	j = x[i]->index;
	k = y[i]->index;
	permut[j] = k;
	permut[k] = j;
    }

    swapped = Cudd_bddPermute(dd,f,permut);
    FREE(permut);

    return(swapped);

} /* end of Cudd_bddSwapVariables */


/**Function********************************************************************

  Synopsis [Rearranges a set of variables in the BDD B.]

  Description [Rearranges a set of variables in the BDD B. The size of
  the set is given by n. This procedure is intended for the
  `randomization' of the priority functions. Returns a pointer to the
  BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPermute Cudd_bddSwapVariables
  Cudd_Dxygtdxz Cudd_Dxygtdyz Cudd_PrioritySelect]

******************************************************************************/
DdNode *
Cudd_bddAdjPermuteX(
  DdManager * dd,
  DdNode * B,
  DdNode ** x,
  int  n)
{
    DdNode *swapped;
    int	 i, j, k;
    int	 *permut;

    permut = ALLOC(int,dd->size);
    if (permut == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < dd->size; i++) permut[i] = i;
    for (i = 0; i < n-2; i += 3) {
	j = x[i]->index;
	k = x[i+1]->index;
	permut[j] = k;
	permut[k] = j;
    }

    swapped = Cudd_bddPermute(dd,B,permut);
    FREE(permut);

    return(swapped);

} /* end of Cudd_bddAdjPermuteX */


/**Function********************************************************************

  Synopsis    [Composes an ADD with a vector of 0-1 ADDs.]

  Description [Given a vector of 0-1 ADDs, creates a new ADD by
  substituting the 0-1 ADDs for the variables of the ADD f.  There
  should be an entry in vector for each variable in the manager.
  If no substitution is sought for a given variable, the corresponding
  projection function should be specified in the vector.
  This function implements simultaneous composition.
  Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addNonSimCompose Cudd_addPermute Cudd_addCompose
  Cudd_bddVectorCompose]

******************************************************************************/
DdNode *
Cudd_addVectorCompose(
  DdManager * dd,
  DdNode * f,
  DdNode ** vector)
{
    DdHashTable		*table;
    DdNode		*res;
    int			deepest;
    int                 i;

    do {
	dd->reordered = 0;
	/* Initialize local cache. */
	table = cuddHashTableInit(dd,1,2);
	if (table == NULL) return(NULL);

	/* Find deepest real substitution. */
	for (deepest = dd->size - 1; deepest >= 0; deepest--) {
	    i = dd->invperm[deepest];
	    if (!ddIsIthAddVar(dd,vector[i],i)) {
		break;
	    }
	}

	/* Recursively solve the problem. */
	res = cuddAddVectorComposeRecur(dd,table,f,vector,deepest);
	if (res != NULL) cuddRef(res);

	/* Dispose of local cache. */
	cuddHashTableQuit(table);
    } while (dd->reordered == 1);

    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_addVectorCompose */


/**Function********************************************************************

  Synopsis    [Composes an ADD with a vector of ADDs.]

  Description [Given a vector of ADDs, creates a new ADD by substituting the
  ADDs for the variables of the ADD f. vectorOn contains ADDs to be substituted
  for the x_v and vectorOff the ADDs to be substituted for x_v'. There should
  be an entry in vector for each variable in the manager.  If no substitution
  is sought for a given variable, the corresponding projection function should
  be specified in the vector.  This function implements simultaneous
  composition.  Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso [Cudd_addVectorCompose Cudd_addNonSimCompose Cudd_addPermute
  Cudd_addCompose Cudd_bddVectorCompose]

******************************************************************************/
DdNode *
Cudd_addGeneralVectorCompose(
  DdManager * dd,
  DdNode * f,
  DdNode ** vectorOn,
  DdNode ** vectorOff)
{
    DdHashTable		*table;
    DdNode		*res;
    int			deepest;
    int                 i;

    do {
	dd->reordered = 0;
	/* Initialize local cache. */
	table = cuddHashTableInit(dd,1,2);
	if (table == NULL) return(NULL);

	/* Find deepest real substitution. */
	for (deepest = dd->size - 1; deepest >= 0; deepest--) {
	    i = dd->invperm[deepest];
	    if (!ddIsIthAddVarPair(dd,vectorOn[i],vectorOff[i],i)) {
		break;
	    }
	}

	/* Recursively solve the problem. */
	res = cuddAddGeneralVectorComposeRecur(dd,table,f,vectorOn,
					       vectorOff,deepest);
	if (res != NULL) cuddRef(res);

	/* Dispose of local cache. */
	cuddHashTableQuit(table);
    } while (dd->reordered == 1);

    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_addGeneralVectorCompose */


/**Function********************************************************************

  Synopsis    [Composes an ADD with a vector of 0-1 ADDs.]

  Description [Given a vector of 0-1 ADDs, creates a new ADD by
  substituting the 0-1 ADDs for the variables of the ADD f.  There
  should be an entry in vector for each variable in the manager.
  This function implements non-simultaneous composition. If any of the
  functions being composed depends on any of the variables being
  substituted, then the result depends on the order of composition,
  which in turn depends on the variable order: The variables farther from
  the roots in the order are substituted first.
  Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addVectorCompose Cudd_addPermute Cudd_addCompose]

******************************************************************************/
DdNode *
Cudd_addNonSimCompose(
  DdManager * dd,
  DdNode * f,
  DdNode ** vector)
{
    DdNode		*cube, *key, *var, *tmp, *piece;
    DdNode		*res;
    int			i, lastsub;

    /* The cache entry for this function is composed of three parts:
    ** f itself, the replacement relation, and the cube of the
    ** variables being substituted.
    ** The replacement relation is the product of the terms (yi EXNOR gi).
    ** This apporach allows us to use the global cache for this function,
    ** with great savings in memory with respect to using arrays for the
    ** cache entries.
    ** First we build replacement relation and cube of substituted
    ** variables from the vector specifying the desired composition.
    */
    key = DD_ONE(dd);
    cuddRef(key);
    cube = DD_ONE(dd);
    cuddRef(cube);
    for (i = (int) dd->size - 1; i >= 0; i--) {
	if (ddIsIthAddVar(dd,vector[i],(unsigned int)i)) {
	    continue;
	}
	var = Cudd_addIthVar(dd,i);
	if (var == NULL) {
	    Cudd_RecursiveDeref(dd,key);
	    Cudd_RecursiveDeref(dd,cube);
	    return(NULL);
	}
	cuddRef(var);
	/* Update cube. */
	tmp = Cudd_addApply(dd,Cudd_addTimes,var,cube);
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,key);
	    Cudd_RecursiveDeref(dd,cube);
	    Cudd_RecursiveDeref(dd,var);
	    return(NULL);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,cube);
	cube = tmp;
	/* Update replacement relation. */
	piece = Cudd_addApply(dd,Cudd_addXnor,var,vector[i]);
	if (piece == NULL) {
	    Cudd_RecursiveDeref(dd,key);
	    Cudd_RecursiveDeref(dd,var);
	    return(NULL);
	}
	cuddRef(piece);
	Cudd_RecursiveDeref(dd,var);
	tmp = Cudd_addApply(dd,Cudd_addTimes,key,piece);
	if (tmp == NULL) {
	    Cudd_RecursiveDeref(dd,key);
	    Cudd_RecursiveDeref(dd,piece);
	    return(NULL);
	}
	cuddRef(tmp);
	Cudd_RecursiveDeref(dd,key);
	Cudd_RecursiveDeref(dd,piece);
	key = tmp;
    }

    /* Now try composition, until no reordering occurs. */
    do {
	/* Find real substitution with largest index. */
	for (lastsub = dd->size - 1; lastsub >= 0; lastsub--) {
	    if (!ddIsIthAddVar(dd,vector[lastsub],(unsigned int)lastsub)) {
		break;
	    }
	}

	/* Recursively solve the problem. */
	dd->reordered = 0;
	res = cuddAddNonSimComposeRecur(dd,f,vector,key,cube,lastsub+1);
	if (res != NULL) cuddRef(res);

    } while (dd->reordered == 1);

    Cudd_RecursiveDeref(dd,key);
    Cudd_RecursiveDeref(dd,cube);
    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_addNonSimCompose */


/**Function********************************************************************

  Synopsis    [Composes a BDD with a vector of BDDs.]

  Description [Given a vector of BDDs, creates a new BDD by
  substituting the BDDs for the variables of the BDD f.  There
  should be an entry in vector for each variable in the manager.
  If no substitution is sought for a given variable, the corresponding
  projection function should be specified in the vector.
  This function implements simultaneous composition.
  Returns a pointer to the resulting BDD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPermute Cudd_bddCompose Cudd_addVectorCompose]

******************************************************************************/
DdNode *
Cudd_bddVectorCompose(
  DdManager * dd,
  DdNode * f,
  DdNode ** vector)
{
    DdHashTable		*table;
    DdNode		*res;
    int			deepest;
    int                 i;

    do {
	dd->reordered = 0;
	/* Initialize local cache. */
	table = cuddHashTableInit(dd,1,2);
	if (table == NULL) return(NULL);

	/* Find deepest real substitution. */
	for (deepest = dd->size - 1; deepest >= 0; deepest--) {
	    i = dd->invperm[deepest];
	    if (vector[i] != dd->vars[i]) {
		break;
	    }
	}

	/* Recursively solve the problem. */
	res = cuddBddVectorComposeRecur(dd,table,f,vector, deepest);
	if (res != NULL) cuddRef(res);

	/* Dispose of local cache. */
	cuddHashTableQuit(table);
    } while (dd->reordered == 1);

    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_bddVectorCompose */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddCompose.]

  Description [Performs the recursive step of Cudd_bddCompose.
  Exploits the fact that the composition of f' with g
  produces the complement of the composition of f with g to better
  utilize the cache.  Returns the composed BDD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddCompose]

******************************************************************************/
DdNode *
cuddBddComposeRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * proj)
{
    DdNode	*F, *G, *f1, *f0, *g1, *g0, *r, *t, *e;
    unsigned int v, topf, topg, topindex;
    int		comple;

    statLine(dd);
    v = dd->perm[proj->index];
    F = Cudd_Regular(f);
    topf = cuddI(dd,F->index);

    /* Terminal case. Subsumes the test for constant f. */
    if (topf > v) return(f);

    /* We solve the problem for a regular pointer, and then complement
    ** the result if the pointer was originally complemented.
    */
    comple = Cudd_IsComplement(f);

    /* Check cache. */
    r = cuddCacheLookup(dd,DD_BDD_COMPOSE_RECUR_TAG,F,g,proj);
    if (r != NULL) {
	return(Cudd_NotCond(r,comple));
    }

    if (topf == v) {
	/* Compose. */
	f1 = cuddT(F);
	f0 = cuddE(F);
	r = cuddBddIteRecur(dd, g, f1, f0);
	if (r == NULL) return(NULL);
    } else {
	/* Compute cofactors of f and g. Remember the index of the top
	** variable.
	*/
	G = Cudd_Regular(g);
	topg = cuddI(dd,G->index);
	if (topf > topg) {
	    topindex = G->index;
	    f1 = f0 = F;
	} else {
	    topindex = F->index;
	    f1 = cuddT(F);
	    f0 = cuddE(F);
	}
	if (topg > topf) {
	    g1 = g0 = g;
	} else {
	    g1 = cuddT(G);
	    g0 = cuddE(G);
	    if (g != G) {
		g1 = Cudd_Not(g1);
		g0 = Cudd_Not(g0);
	    }
	}
	/* Recursive step. */
	t = cuddBddComposeRecur(dd, f1, g1, proj);
	if (t == NULL) return(NULL);
	cuddRef(t);
	e = cuddBddComposeRecur(dd, f0, g0, proj);
	if (e == NULL) {
	    Cudd_IterDerefBdd(dd, t);
	    return(NULL);
	}
	cuddRef(e);

	r = cuddBddIteRecur(dd, dd->vars[topindex], t, e);
	if (r == NULL) {
	    Cudd_IterDerefBdd(dd, t);
	    Cudd_IterDerefBdd(dd, e);
	    return(NULL);
	}
	cuddRef(r);
	Cudd_IterDerefBdd(dd, t); /* t & e not necessarily part of r */
	Cudd_IterDerefBdd(dd, e);
	cuddDeref(r);
    }

    cuddCacheInsert(dd,DD_BDD_COMPOSE_RECUR_TAG,F,g,proj,r);

    return(Cudd_NotCond(r,comple));

} /* end of cuddBddComposeRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addCompose.]

  Description [Performs the recursive step of Cudd_addCompose.
  Returns the composed BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addCompose]

******************************************************************************/
DdNode *
cuddAddComposeRecur(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  DdNode * proj)
{
    DdNode *f1, *f0, *g1, *g0, *r, *t, *e;
    unsigned int v, topf, topg, topindex;

    statLine(dd);
    v = dd->perm[proj->index];
    topf = cuddI(dd,f->index);

    /* Terminal case. Subsumes the test for constant f. */
    if (topf > v) return(f);

    /* Check cache. */
    r = cuddCacheLookup(dd,DD_ADD_COMPOSE_RECUR_TAG,f,g,proj);
    if (r != NULL) {
	return(r);
    }

    if (topf == v) {
	/* Compose. */
	f1 = cuddT(f);
	f0 = cuddE(f);
	r = cuddAddIteRecur(dd, g, f1, f0);
	if (r == NULL) return(NULL);
    } else {
	/* Compute cofactors of f and g. Remember the index of the top
	** variable.
	*/
	topg = cuddI(dd,g->index);
	if (topf > topg) {
	    topindex = g->index;
	    f1 = f0 = f;
	} else {
	    topindex = f->index;
	    f1 = cuddT(f);
	    f0 = cuddE(f);
	}
	if (topg > topf) {
	    g1 = g0 = g;
	} else {
	    g1 = cuddT(g);
	    g0 = cuddE(g);
	}
	/* Recursive step. */
	t = cuddAddComposeRecur(dd, f1, g1, proj);
	if (t == NULL) return(NULL);
	cuddRef(t);
	e = cuddAddComposeRecur(dd, f0, g0, proj);
	if (e == NULL) {
	    Cudd_RecursiveDeref(dd, t);
	    return(NULL);
	}
	cuddRef(e);

	if (t == e) {
	    r = t;
	} else {
	    r = cuddUniqueInter(dd, (int) topindex, t, e);
	    if (r == NULL) {
		Cudd_RecursiveDeref(dd, t);
		Cudd_RecursiveDeref(dd, e);
		return(NULL);
	    }
	}
	cuddDeref(t);
	cuddDeref(e);
    }

    cuddCacheInsert(dd,DD_ADD_COMPOSE_RECUR_TAG,f,g,proj,r);

    return(r);

} /* end of cuddAddComposeRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_addPermute.]

  Description [ Recursively puts the ADD in the order given in the
  array permut. Checks for trivial cases to terminate recursion, then
  splits on the children of this node.  Once the solutions for the
  children are obtained, it puts into the current position the node
  from the rest of the ADD that should be here. Then returns this ADD.
  The key here is that the node being visited is NOT put in its proper
  place by this instance, but rather is switched when its proper
  position is reached in the recursion tree.<p>
  The DdNode * that is returned is the same ADD as passed in as node,
  but in the new order.]

  SideEffects [None]

  SeeAlso     [Cudd_addPermute cuddBddPermuteRecur]

******************************************************************************/
static DdNode *
cuddAddPermuteRecur(
  DdManager * manager /* DD manager */,
  DdHashTable * table /* computed table */,
  DdNode * node /* ADD to be reordered */,
  int * permut /* permutation array */)
{
    DdNode	*T,*E;
    DdNode	*res,*var;
    int		index;
    
    statLine(manager);
    /* Check for terminal case of constant node. */
    if (cuddIsConstant(node)) {
	return(node);
    }

    /* If problem already solved, look up answer and return. */
    if (node->ref != 1 && (res = cuddHashTableLookup1(table,node)) != NULL) {
#ifdef DD_DEBUG
	addPermuteRecurHits++;
#endif
	return(res);
    }

    /* Split and recur on children of this node. */
    T = cuddAddPermuteRecur(manager,table,cuddT(node),permut);
    if (T == NULL) return(NULL);
    cuddRef(T);
    E = cuddAddPermuteRecur(manager,table,cuddE(node),permut);
    if (E == NULL) {
	Cudd_RecursiveDeref(manager, T);
	return(NULL);
    }
    cuddRef(E);

    /* Move variable that should be in this position to this position
    ** by creating a single var ADD for that variable, and calling
    ** cuddAddIteRecur with the T and E we just created.
    */
    index = permut[node->index];
    var = cuddUniqueInter(manager,index,DD_ONE(manager),DD_ZERO(manager));
    if (var == NULL) return(NULL);
    cuddRef(var);
    res = cuddAddIteRecur(manager,var,T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(manager,var);
	Cudd_RecursiveDeref(manager, T);
	Cudd_RecursiveDeref(manager, E);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(manager,var);
    Cudd_RecursiveDeref(manager, T);
    Cudd_RecursiveDeref(manager, E);

    /* Do not keep the result if the reference count is only 1, since
    ** it will not be visited again.
    */
    if (node->ref != 1) {
	ptrint fanout = (ptrint) node->ref;
	cuddSatDec(fanout);
	if (!cuddHashTableInsert1(table,node,res,fanout)) {
	    Cudd_RecursiveDeref(manager, res);
	    return(NULL);
	}
    }
    cuddDeref(res);
    return(res);

} /* end of cuddAddPermuteRecur */


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_bddPermute.]

  Description [ Recursively puts the BDD in the order given in the array permut.
  Checks for trivial cases to terminate recursion, then splits on the
  children of this node.  Once the solutions for the children are
  obtained, it puts into the current position the node from the rest of
  the BDD that should be here. Then returns this BDD.
  The key here is that the node being visited is NOT put in its proper
  place by this instance, but rather is switched when its proper position
  is reached in the recursion tree.<p>
  The DdNode * that is returned is the same BDD as passed in as node,
  but in the new order.]

  SideEffects [None]

  SeeAlso     [Cudd_bddPermute cuddAddPermuteRecur]

******************************************************************************/
static DdNode *
cuddBddPermuteRecur(
  DdManager * manager /* DD manager */,
  DdHashTable * table /* computed table */,
  DdNode * node /* BDD to be reordered */,
  int * permut /* permutation array */)
{
    DdNode	*N,*T,*E;
    DdNode	*res;
    int		index;

    statLine(manager);
    N = Cudd_Regular(node);

    /* Check for terminal case of constant node. */
    if (cuddIsConstant(N)) {
	return(node);
    }

    /* If problem already solved, look up answer and return. */
    if (N->ref != 1 && (res = cuddHashTableLookup1(table,N)) != NULL) {
#ifdef DD_DEBUG
	bddPermuteRecurHits++;
#endif
	return(Cudd_NotCond(res,N != node));
    }

    /* Split and recur on children of this node. */
    T = cuddBddPermuteRecur(manager,table,cuddT(N),permut);
    if (T == NULL) return(NULL);
    cuddRef(T);
    E = cuddBddPermuteRecur(manager,table,cuddE(N),permut);
    if (E == NULL) {
	Cudd_IterDerefBdd(manager, T);
	return(NULL);
    }
    cuddRef(E);

    /* Move variable that should be in this position to this position
    ** by retrieving the single var BDD for that variable, and calling
    ** cuddBddIteRecur with the T and E we just created.
    */
    index = permut[N->index];
    res = cuddBddIteRecur(manager,manager->vars[index],T,E);
    if (res == NULL) {
	Cudd_IterDerefBdd(manager, T);
	Cudd_IterDerefBdd(manager, E);
	return(NULL);
    }
    cuddRef(res);
    Cudd_IterDerefBdd(manager, T);
    Cudd_IterDerefBdd(manager, E);

    /* Do not keep the result if the reference count is only 1, since
    ** it will not be visited again.
    */
    if (N->ref != 1) {
	ptrint fanout = (ptrint) N->ref;
	cuddSatDec(fanout);
	if (!cuddHashTableInsert1(table,N,res,fanout)) {
	    Cudd_IterDerefBdd(manager, res);
	    return(NULL);
	}
    }
    cuddDeref(res);
    return(Cudd_NotCond(res,N != node));

} /* end of cuddBddPermuteRecur */


/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_bddVarMap.]

  Description [Implements the recursive step of Cudd_bddVarMap.
  Returns a pointer to the result if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddVarMap]

******************************************************************************/
static DdNode *
cuddBddVarMapRecur(
  DdManager *manager /* DD manager */,
  DdNode *f /* BDD to be remapped */)
{
    DdNode	*F, *T, *E;
    DdNode	*res;
    int		index;

    statLine(manager);
    F = Cudd_Regular(f);

    /* Check for terminal case of constant node. */
    if (cuddIsConstant(F)) {
	return(f);
    }

    /* If problem already solved, look up answer and return. */
    if (F->ref != 1 &&
	(res = cuddCacheLookup1(manager,Cudd_bddVarMap,F)) != NULL) {
	return(Cudd_NotCond(res,F != f));
    }

    /* Split and recur on children of this node. */
    T = cuddBddVarMapRecur(manager,cuddT(F));
    if (T == NULL) return(NULL);
    cuddRef(T);
    E = cuddBddVarMapRecur(manager,cuddE(F));
    if (E == NULL) {
	Cudd_IterDerefBdd(manager, T);
	return(NULL);
    }
    cuddRef(E);

    /* Move variable that should be in this position to this position
    ** by retrieving the single var BDD for that variable, and calling
    ** cuddBddIteRecur with the T and E we just created.
    */
    index = manager->map[F->index];
    res = cuddBddIteRecur(manager,manager->vars[index],T,E);
    if (res == NULL) {
	Cudd_IterDerefBdd(manager, T);
	Cudd_IterDerefBdd(manager, E);
	return(NULL);
    }
    cuddRef(res);
    Cudd_IterDerefBdd(manager, T);
    Cudd_IterDerefBdd(manager, E);

    /* Do not keep the result if the reference count is only 1, since
    ** it will not be visited again.
    */
    if (F->ref != 1) {
	cuddCacheInsert1(manager,Cudd_bddVarMap,F,res);
    }
    cuddDeref(res);
    return(Cudd_NotCond(res,F != f));

} /* end of cuddBddVarMapRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addVectorCompose.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
cuddAddVectorComposeRecur(
  DdManager * dd /* DD manager */,
  DdHashTable * table /* computed table */,
  DdNode * f /* ADD in which to compose */,
  DdNode ** vector /* functions to substitute */,
  int  deepest /* depth of deepest substitution */)
{
    DdNode	*T,*E;
    DdNode	*res;

    statLine(dd);
    /* If we are past the deepest substitution, return f. */
    if (cuddI(dd,f->index) > deepest) {
	return(f);
    }

    if ((res = cuddHashTableLookup1(table,f)) != NULL) {
#ifdef DD_DEBUG
	addVectorComposeHits++;
#endif
	return(res);
    }

    /* Split and recur on children of this node. */
    T = cuddAddVectorComposeRecur(dd,table,cuddT(f),vector,deepest);
    if (T == NULL)  return(NULL);
    cuddRef(T);
    E = cuddAddVectorComposeRecur(dd,table,cuddE(f),vector,deepest);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);

    /* Retrieve the 0-1 ADD for the current top variable and call
    ** cuddAddIteRecur with the T and E we just created.
    */
    res = cuddAddIteRecur(dd,vector[f->index],T,E);
    if (res == NULL) {
	Cudd_RecursiveDeref(dd, T);
	Cudd_RecursiveDeref(dd, E);
	return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd, T);
    Cudd_RecursiveDeref(dd, E);

    /* Do not keep the result if the reference count is only 1, since
    ** it will not be visited again
    */
    if (f->ref != 1) {
	ptrint fanout = (ptrint) f->ref;
	cuddSatDec(fanout);
	if (!cuddHashTableInsert1(table,f,res,fanout)) {
	    Cudd_RecursiveDeref(dd, res);
	    return(NULL);
	}
    }
    cuddDeref(res);
    return(res);

} /* end of cuddAddVectorComposeRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addGeneralVectorCompose.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
cuddAddGeneralVectorComposeRecur(
  DdManager * dd /* DD manager */,
  DdHashTable * table /* computed table */,
  DdNode * f /* ADD in which to compose */,
  DdNode ** vectorOn /* functions to substitute for x_i */,
  DdNode ** vectorOff /* functions to substitute for x_i' */,
  int  deepest /* depth of deepest substitution */)
{
    DdNode	*T,*E,*t,*e;
    DdNode	*res;

    /* If we are past the deepest substitution, return f. */
    if (cuddI(dd,f->index) > deepest) {
	return(f);
    }

    if ((res = cuddHashTableLookup1(table,f)) != NULL) {
#ifdef DD_DEBUG
	addGeneralVectorComposeHits++;
#endif
	return(res);
    }

    /* Split and recur on children of this node. */
    T = cuddAddGeneralVectorComposeRecur(dd,table,cuddT(f),
					 vectorOn,vectorOff,deepest);
    if (T == NULL)  return(NULL);
    cuddRef(T);
    E = cuddAddGeneralVectorComposeRecur(dd,table,cuddE(f),
					 vectorOn,vectorOff,deepest);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd, T);
	return(NULL);
    }
    cuddRef(E);

    /* Retrieve the compose ADDs for the current top variable and call
    ** cuddAddApplyRecur with the T and E we just created.
    */
    t = cuddAddApplyRecur(dd,Cudd_addTimes,vectorOn[f->index],T);
    if (t == NULL) {
      Cudd_RecursiveDeref(dd,T);
      Cudd_RecursiveDeref(dd,E);
      return(NULL);
    }
    cuddRef(t);
    e = cuddAddApplyRecur(dd,Cudd_addTimes,vectorOff[f->index],E);
    if (e == NULL) {
      Cudd_RecursiveDeref(dd,T);
      Cudd_RecursiveDeref(dd,E);
      Cudd_RecursiveDeref(dd,t);
      return(NULL);
    }
    cuddRef(e);
    res = cuddAddApplyRecur(dd,Cudd_addPlus,t,e);
    if (res == NULL) {
      Cudd_RecursiveDeref(dd,T);
      Cudd_RecursiveDeref(dd,E);
      Cudd_RecursiveDeref(dd,t);
      Cudd_RecursiveDeref(dd,e);
      return(NULL);
    }
    cuddRef(res);
    Cudd_RecursiveDeref(dd,T);
    Cudd_RecursiveDeref(dd,E);
    Cudd_RecursiveDeref(dd,t);
    Cudd_RecursiveDeref(dd,e);

    /* Do not keep the result if the reference count is only 1, since
    ** it will not be visited again
    */
    if (f->ref != 1) {
	ptrint fanout = (ptrint) f->ref;
	cuddSatDec(fanout);
	if (!cuddHashTableInsert1(table,f,res,fanout)) {
	    Cudd_RecursiveDeref(dd, res);
	    return(NULL);
	}
    }
    cuddDeref(res);
    return(res);

} /* end of cuddAddGeneralVectorComposeRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addNonSimCompose.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
cuddAddNonSimComposeRecur(
  DdManager * dd,
  DdNode * f,
  DdNode ** vector,
  DdNode * key,
  DdNode * cube,
  int  lastsub)
{
    DdNode *f1, *f0, *key1, *key0, *cube1, *var;
    DdNode *T,*E;
    DdNode *r;
    unsigned int top, topf, topk, topc;
    unsigned int index;
    int i;
    DdNode **vect1;
    DdNode **vect0;

    statLine(dd);
    /* If we are past the deepest substitution, return f. */
    if (cube == DD_ONE(dd) || cuddIsConstant(f)) {
	return(f);
    }

    /* If problem already solved, look up answer and return. */
    r = cuddCacheLookup(dd,DD_ADD_NON_SIM_COMPOSE_TAG,f,key,cube);
    if (r != NULL) {
	return(r);
    }

    /* Find top variable. we just need to look at f, key, and cube,
    ** because all the varibles in the gi are in key.
    */
    topf = cuddI(dd,f->index);
    topk = cuddI(dd,key->index);
    top = ddMin(topf,topk);
    topc = cuddI(dd,cube->index);
    top = ddMin(top,topc);
    index = dd->invperm[top];

    /* Compute the cofactors. */
    if (topf == top) {
	f1 = cuddT(f);
	f0 = cuddE(f);
    } else {
	f1 = f0 = f;
    }
    if (topc == top) {
	cube1 = cuddT(cube);
	/* We want to eliminate vector[index] from key. Otherwise
	** cache performance is severely affected. Hence we
	** existentially quantify the variable with index "index" from key.
	*/
	var = Cudd_addIthVar(dd, (int) index);
	if (var == NULL) {
	    return(NULL);
	}
	cuddRef(var);
	key1 = cuddAddExistAbstractRecur(dd, key, var);
	if (key1 == NULL) {
	    Cudd_RecursiveDeref(dd,var);
	    return(NULL);
	}
	cuddRef(key1);
	Cudd_RecursiveDeref(dd,var);
	key0 = key1;
    } else {
	cube1 = cube;
	if (topk == top) {
	    key1 = cuddT(key);
	    key0 = cuddE(key);
	} else {
	    key1 = key0 = key;
	}
	cuddRef(key1);
    }

    /* Allocate two new vectors for the cofactors of vector. */
    vect1 = ALLOC(DdNode *,lastsub);
    if (vect1 == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	Cudd_RecursiveDeref(dd,key1);
	return(NULL);
    }
    vect0 = ALLOC(DdNode *,lastsub);
    if (vect0 == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	Cudd_RecursiveDeref(dd,key1);
	FREE(vect1);
	return(NULL);
    }

    /* Cofactor the gi. Eliminate vect1[index] and vect0[index], because
    ** we do not need them.
    */
    for (i = 0; i < lastsub; i++) {
	DdNode *gi = vector[i];
	if (gi == NULL) {
	    vect1[i] = vect0[i] = NULL;
	} else if (gi->index == index) {
	    vect1[i] = cuddT(gi);
	    vect0[i] = cuddE(gi);
	} else {
	    vect1[i] = vect0[i] = gi;
	}
    }
    vect1[index] = vect0[index] = NULL;

    /* Recur on children. */
    T = cuddAddNonSimComposeRecur(dd,f1,vect1,key1,cube1,lastsub);
    FREE(vect1);
    if (T == NULL) {
	Cudd_RecursiveDeref(dd,key1);
	FREE(vect0);
	return(NULL);
    }
    cuddRef(T);
    E = cuddAddNonSimComposeRecur(dd,f0,vect0,key0,cube1,lastsub);
    FREE(vect0);
    if (E == NULL) {
	Cudd_RecursiveDeref(dd,key1);
	Cudd_RecursiveDeref(dd,T);
	return(NULL);
    }
    cuddRef(E);
    Cudd_RecursiveDeref(dd,key1);

    /* Retrieve the 0-1 ADD for the current top variable from vector,
    ** and call cuddAddIteRecur with the T and E we just created.
    */
    r = cuddAddIteRecur(dd,vector[index],T,E);
    if (r == NULL) {
	Cudd_RecursiveDeref(dd,T);
	Cudd_RecursiveDeref(dd,E);
	return(NULL);
    }
    cuddRef(r);
    Cudd_RecursiveDeref(dd,T);
    Cudd_RecursiveDeref(dd,E);
    cuddDeref(r);

    /* Store answer to trim recursion. */
    cuddCacheInsert(dd,DD_ADD_NON_SIM_COMPOSE_TAG,f,key,cube,r);

    return(r);

} /* end of cuddAddNonSimComposeRecur */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddVectorCompose.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
cuddBddVectorComposeRecur(
  DdManager * dd /* DD manager */,
  DdHashTable * table /* computed table */,
  DdNode * f /* BDD in which to compose */,
  DdNode ** vector /* functions to be composed */,
  int deepest /* depth of the deepest substitution */)
{
    DdNode	*F,*T,*E;
    DdNode	*res;

    statLine(dd);
    F = Cudd_Regular(f);

    /* If we are past the deepest substitution, return f. */
    if (cuddI(dd,F->index) > deepest) {
	return(f);
    }

    /* If problem already solved, look up answer and return. */
    if ((res = cuddHashTableLookup1(table,F)) != NULL) {
#ifdef DD_DEBUG
	bddVectorComposeHits++;
#endif
	return(Cudd_NotCond(res,F != f));
    }

    /* Split and recur on children of this node. */
    T = cuddBddVectorComposeRecur(dd,table,cuddT(F),vector, deepest);
    if (T == NULL) return(NULL);
    cuddRef(T);
    E = cuddBddVectorComposeRecur(dd,table,cuddE(F),vector, deepest);
    if (E == NULL) {
	Cudd_IterDerefBdd(dd, T);
	return(NULL);
    }
    cuddRef(E);

    /* Call cuddBddIteRecur with the BDD that replaces the current top
    ** variable and the T and E we just created.
    */
    res = cuddBddIteRecur(dd,vector[F->index],T,E);
    if (res == NULL) {
	Cudd_IterDerefBdd(dd, T);
	Cudd_IterDerefBdd(dd, E);
	return(NULL);
    }
    cuddRef(res);
    Cudd_IterDerefBdd(dd, T);
    Cudd_IterDerefBdd(dd, E);	

    /* Do not keep the result if the reference count is only 1, since
    ** it will not be visited again.
    */
    if (F->ref != 1) {
	ptrint fanout = (ptrint) F->ref;
	cuddSatDec(fanout);
	if (!cuddHashTableInsert1(table,F,res,fanout)) {
	    Cudd_IterDerefBdd(dd, res);
	    return(NULL);
	}
    }
    cuddDeref(res);
    return(Cudd_NotCond(res,F != f));

} /* end of cuddBddVectorComposeRecur */


/**Function********************************************************************

  Synopsis    [Comparison of a function to the i-th ADD variable.]

  Description [Comparison of a function to the i-th ADD variable. Returns 1 if
  the function is the i-th ADD variable; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DD_INLINE
static int
ddIsIthAddVar(
  DdManager * dd,
  DdNode * f,
  unsigned int  i)
{
    return(f->index == i && cuddT(f) == DD_ONE(dd) && cuddE(f) == DD_ZERO(dd));

} /* end of ddIsIthAddVar */


/**Function********************************************************************

  Synopsis    [Comparison of a pair of functions to the i-th ADD variable.]

  Description [Comparison of a pair of functions to the i-th ADD
  variable. Returns 1 if the functions are the i-th ADD variable and its
  complement; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DD_INLINE
static int
ddIsIthAddVarPair(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  unsigned int  i)
{
    return(f->index == i && g->index == i && 
	   cuddT(f) == DD_ONE(dd) && cuddE(f) == DD_ZERO(dd) &&
	   cuddT(g) == DD_ZERO(dd) && cuddE(g) == DD_ONE(dd));

} /* end of ddIsIthAddVarPair */
