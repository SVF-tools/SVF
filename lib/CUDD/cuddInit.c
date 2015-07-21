/**CFile***********************************************************************

  FileName    [cuddInit.c]

  PackageName [cudd]

  Synopsis    [Functions to initialize and shut down the DD manager.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_Init()
		<li> Cudd_Quit()
		</ul>
	       Internal procedures included in this module:
		<ul>
		<li> cuddZddInitUniv()
		<li> cuddZddFreeUniv()
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
static char rcsid[] DD_UNUSED = "$Id: cuddInit.c,v 1.34 2012/02/05 01:07:19 fabio Exp $";
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

  Synopsis    [Creates a new DD manager.]

  Description [Creates a new DD manager, initializes the table, the
  basic constants and the projection functions. If maxMemory is 0,
  Cudd_Init decides suitable values for the maximum size of the cache
  and for the limit for fast unique table growth based on the available
  memory. Returns a pointer to the manager if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Quit]

******************************************************************************/
DdManager *
Cudd_Init(
  unsigned int numVars /* initial number of BDD variables (i.e., subtables) */,
  unsigned int numVarsZ /* initial number of ZDD variables (i.e., subtables) */,
  unsigned int numSlots /* initial size of the unique tables */,
  unsigned int cacheSize /* initial size of the cache */,
  unsigned long maxMemory /* target maximum memory occupation */)
{
    DdManager *unique;
    int i,result;
    DdNode *one, *zero;
    unsigned int maxCacheSize;
    unsigned int looseUpTo;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    if (maxMemory == 0) {
	maxMemory = getSoftDataLimit();
    }
    looseUpTo = (unsigned int) ((maxMemory / sizeof(DdNode)) /
				DD_MAX_LOOSE_FRACTION);
    unique = cuddInitTable(numVars,numVarsZ,numSlots,looseUpTo);
    if (unique == NULL) return(NULL);
    unique->maxmem = (unsigned long) maxMemory / 10 * 9;
    maxCacheSize = (unsigned int) ((maxMemory / sizeof(DdCache)) /
				   DD_MAX_CACHE_FRACTION);
    result = cuddInitCache(unique,cacheSize,maxCacheSize);
    if (result == 0) return(NULL);

    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    unique->stash = ALLOC(char,(maxMemory / DD_STASH_FRACTION) + 4);
    MMoutOfMemory = saveHandler;
    if (unique->stash == NULL) {
	(void) fprintf(unique->err,"Unable to set aside memory\n");
    }

    /* Initialize constants. */
    unique->one = cuddUniqueConst(unique,1.0);
    if (unique->one == NULL) return(0);
    cuddRef(unique->one);
    unique->zero = cuddUniqueConst(unique,0.0);
    if (unique->zero == NULL) return(0);
    cuddRef(unique->zero);
#ifdef HAVE_IEEE_754
    if (DD_PLUS_INF_VAL != DD_PLUS_INF_VAL * 3 ||
	DD_PLUS_INF_VAL != DD_PLUS_INF_VAL / 3) {
	(void) fprintf(unique->err,"Warning: Crippled infinite values\n");
	(void) fprintf(unique->err,"Recompile without -DHAVE_IEEE_754\n");
    }
#endif
    unique->plusinfinity = cuddUniqueConst(unique,DD_PLUS_INF_VAL);
    if (unique->plusinfinity == NULL) return(0);
    cuddRef(unique->plusinfinity);
    unique->minusinfinity = cuddUniqueConst(unique,DD_MINUS_INF_VAL);
    if (unique->minusinfinity == NULL) return(0);
    cuddRef(unique->minusinfinity);
    unique->background = unique->zero;

    /* The logical zero is different from the CUDD_VALUE_TYPE zero! */
    one = unique->one;
    zero = Cudd_Not(one);
    /* Create the projection functions. */
    unique->vars = ALLOC(DdNodePtr,unique->maxSize);
    if (unique->vars == NULL) {
	unique->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    for (i = 0; i < unique->size; i++) {
	unique->vars[i] = cuddUniqueInter(unique,i,one,zero);
	if (unique->vars[i] == NULL) return(0);
	cuddRef(unique->vars[i]);
    }

    if (unique->sizeZ)
	cuddZddInitUniv(unique);

    unique->memused += sizeof(DdNode *) * unique->maxSize;

    return(unique);

} /* end of Cudd_Init */


/**Function********************************************************************

  Synopsis    [Deletes resources associated with a DD manager.]

  Description [Deletes resources associated with a DD manager and
  resets the global statistical counters. (Otherwise, another manaqger
  subsequently created would inherit the stats of this one.)]

  SideEffects [None]

  SeeAlso     [Cudd_Init]

******************************************************************************/
void
Cudd_Quit(
  DdManager * unique)
{
    if (unique->stash != NULL) FREE(unique->stash);
    cuddFreeTable(unique);

} /* end of Cudd_Quit */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Initializes the ZDD universe.]

  Description [Initializes the ZDD universe. Returns 1 if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddZddFreeUniv]

******************************************************************************/
int
cuddZddInitUniv(
  DdManager * zdd)
{
    DdNode	*p, *res;
    int		i;

    zdd->univ = ALLOC(DdNodePtr, zdd->sizeZ);
    if (zdd->univ == NULL) {
	zdd->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }

    res = DD_ONE(zdd);
    cuddRef(res);
    for (i = zdd->sizeZ - 1; i >= 0; i--) {
	unsigned int index = zdd->invpermZ[i];
	p = res;
	res = cuddUniqueInterZdd(zdd, index, p, p);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(zdd,p);
	    FREE(zdd->univ);
	    return(0);
	}
	cuddRef(res);
	cuddDeref(p);
	zdd->univ[i] = res;
    }

#ifdef DD_VERBOSE
    cuddZddP(zdd, zdd->univ[0]);
#endif

    return(1);

} /* end of cuddZddInitUniv */


/**Function********************************************************************

  Synopsis    [Frees the ZDD universe.]

  Description [Frees the ZDD universe.]

  SideEffects [None]

  SeeAlso     [cuddZddInitUniv]

******************************************************************************/
void
cuddZddFreeUniv(
  DdManager * zdd)
{
    if (zdd->univ) {
	Cudd_RecursiveDerefZdd(zdd, zdd->univ[0]);
	FREE(zdd->univ);
    }

} /* end of cuddZddFreeUniv */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

