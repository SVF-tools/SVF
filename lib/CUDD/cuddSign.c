/**CFile***********************************************************************

  FileName    [cuddSign.c]

  PackageName [cudd]

  Synopsis    [Computation of signatures.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_CofMinterm();
		    </ul>
		Static procedures included in this module:
		    <ul>
		    <li> ddCofMintermAux()
		    </ul>
		    ]

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
static char rcsid[] DD_UNUSED = "$Id: cuddSign.c,v 1.24 2012/02/05 01:07:19 fabio Exp $";
#endif

static int    size;

#ifdef DD_STATS
static int num_calls;	/* should equal 2n-1 (n is the # of nodes) */
static int table_mem;
#endif


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static double * ddCofMintermAux (DdManager *dd, DdNode *node, st_table *table);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Computes the fraction of minterms in the on-set of all the
  positive cofactors of a BDD or ADD.]

  Description [Computes the fraction of minterms in the on-set of all
  the positive cofactors of DD. Returns the pointer to an array of
  doubles if successful; NULL otherwise. The array has as many
  positions as there are BDD variables in the manager plus one. The
  last position of the array contains the fraction of the minterms in
  the ON-set of the function represented by the BDD or ADD. The other
  positions of the array hold the variable signatures.]

  SideEffects [None]

******************************************************************************/
double *
Cudd_CofMinterm(
  DdManager * dd,
  DdNode * node)
{
    st_table	*table;
    double	*values;
    double	*result = NULL;
    int		i, firstLevel;

#ifdef DD_STATS
    unsigned long startTime;
    startTime = util_cpu_time();
    num_calls = 0;
    table_mem = sizeof(st_table);
#endif

    table = st_init_table(st_ptrcmp, st_ptrhash);
    if (table == NULL) {
	(void) fprintf(dd->err,
		       "out-of-memory, couldn't measure DD cofactors.\n");
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    size = dd->size;
    values = ddCofMintermAux(dd, node, table);
    if (values != NULL) {
	result = ALLOC(double,size + 1);
	if (result != NULL) {
#ifdef DD_STATS
	    table_mem += (size + 1) * sizeof(double);
#endif
	    if (Cudd_IsConstant(node))
		firstLevel = 1;
	    else
		firstLevel = cuddI(dd,Cudd_Regular(node)->index);
	    for (i = 0; i < size; i++) {
		if (i >= cuddI(dd,Cudd_Regular(node)->index)) {
		    result[dd->invperm[i]] = values[i - firstLevel];
		} else {
		    result[dd->invperm[i]] = values[size - firstLevel];
		}
	    }
	    result[size] = values[size - firstLevel];
	} else {
	    dd->errorCode = CUDD_MEMORY_OUT;
	}
    }

#ifdef DD_STATS
    table_mem += table->num_bins * sizeof(st_table_entry *);
#endif
    if (Cudd_Regular(node)->ref == 1) FREE(values);
    st_foreach(table, cuddStCountfree, NULL);
    st_free_table(table);
#ifdef DD_STATS
    (void) fprintf(dd->out,"Number of calls: %d\tTable memory: %d bytes\n",
		  num_calls, table_mem);
    (void) fprintf(dd->out,"Time to compute measures: %s\n",
		  util_print_time(util_cpu_time() - startTime));
#endif
    if (result == NULL) {
	(void) fprintf(dd->out,
		       "out-of-memory, couldn't measure DD cofactors.\n");
	dd->errorCode = CUDD_MEMORY_OUT;
    }
    return(result);

} /* end of Cudd_CofMinterm */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Recursive Step for Cudd_CofMinterm function.]

  Description [Traverses the DD node and computes the fraction of
  minterms in the on-set of all positive cofactors simultaneously.
  It allocates an array with two more entries than there are
  variables below the one labeling the node.  One extra entry (the
  first in the array) is for the variable labeling the node. The other
  entry (the last one in the array) holds the fraction of minterms of
  the function rooted at node.  Each other entry holds the value for
  one cofactor. The array is put in a symbol table, to avoid repeated
  computation, and its address is returned by the procedure, for use
  by the caller.  Returns a pointer to the array of cofactor measures.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static double *
ddCofMintermAux(
  DdManager * dd,
  DdNode * node,
  st_table * table)
{
    DdNode	*N;		/* regular version of node */
    DdNode	*Nv, *Nnv;
    double	*values;
    double	*valuesT, *valuesE;
    int		i;
    int		localSize, localSizeT, localSizeE;
    double	vT, vE;

    statLine(dd);
#ifdef DD_STATS
    num_calls++;
#endif

    if (st_lookup(table, node, &values)) {
	return(values);
    }

    N = Cudd_Regular(node);
    if (cuddIsConstant(N)) {
	localSize = 1;
    } else {
	localSize = size - cuddI(dd,N->index) + 1;
    }
    values = ALLOC(double, localSize);
    if (values == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    if (cuddIsConstant(N)) {
	if (node == DD_ZERO(dd) || node == Cudd_Not(DD_ONE(dd))) {
	    values[0] = 0.0;
	} else {
	    values[0] = 1.0;
	}
    } else {
	Nv = Cudd_NotCond(cuddT(N),N!=node);
	Nnv = Cudd_NotCond(cuddE(N),N!=node);

	valuesT = ddCofMintermAux(dd, Nv, table);
	if (valuesT == NULL) return(NULL);
	valuesE = ddCofMintermAux(dd, Nnv, table);
	if (valuesE == NULL) return(NULL);

	if (Cudd_IsConstant(Nv)) {
	    localSizeT = 1;
	} else {
	    localSizeT = size - cuddI(dd,Cudd_Regular(Nv)->index) + 1;
	}
	if (Cudd_IsConstant(Nnv)) {
	    localSizeE = 1;
	} else {
	    localSizeE = size - cuddI(dd,Cudd_Regular(Nnv)->index) + 1;
	}
	values[0] = valuesT[localSizeT - 1];
	for (i = 1; i < localSize; i++) {
	    if (i >= cuddI(dd,Cudd_Regular(Nv)->index) - cuddI(dd,N->index)) {
		vT = valuesT[i - cuddI(dd,Cudd_Regular(Nv)->index) +
			    cuddI(dd,N->index)];
	    } else {
		vT = valuesT[localSizeT - 1];
	    }
	    if (i >= cuddI(dd,Cudd_Regular(Nnv)->index) - cuddI(dd,N->index)) {
		vE = valuesE[i - cuddI(dd,Cudd_Regular(Nnv)->index) +
			    cuddI(dd,N->index)];
	    } else {
		vE = valuesE[localSizeE - 1];
	    }
	    values[i] = (vT + vE) / 2.0;
	}
	if (Cudd_Regular(Nv)->ref == 1) FREE(valuesT);
	if (Cudd_Regular(Nnv)->ref == 1) FREE(valuesE);
    }

    if (N->ref > 1) {
	if (st_add_direct(table, (char *) node, (char *) values) == ST_OUT_OF_MEM) {
	    FREE(values);
	    return(NULL);
	}
#ifdef DD_STATS
	table_mem += localSize * sizeof(double) + sizeof(st_table_entry);
#endif
    }
    return(values);

} /* end of ddCofMintermAux */
