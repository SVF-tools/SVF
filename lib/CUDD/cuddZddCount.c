/**CFile***********************************************************************

  FileName    [cuddZddCount.c]

  PackageName [cudd]

  Synopsis    [Procedures to count the number of minterms of a ZDD.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_zddCount();
		    <li> Cudd_zddCountDouble();
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    </ul>
	       Static procedures included in this module:
		    <ul>
       		    <li> cuddZddCountStep();
		    <li> cuddZddCountDoubleStep();
		    <li> st_zdd_count_dbl_free()
		    <li> st_zdd_countfree()
		    </ul>
	      ]

  SeeAlso     []

  Author      [Hyong-Kyoon Shin, In-Ho Moon]

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddCount.c,v 1.15 2012/02/05 01:07:19 fabio Exp $";
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

static int cuddZddCountStep (DdNode *P, st_table *table, DdNode *base, DdNode *empty);
static double cuddZddCountDoubleStep (DdNode *P, st_table *table, DdNode *base, DdNode *empty);
static enum st_retval st_zdd_countfree (char *key, char *value, char *arg);
static enum st_retval st_zdd_count_dbl_free (char *key, char *value, char *arg);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Counts the number of minterms in a ZDD.]

  Description [Returns an integer representing the number of minterms
  in a ZDD.]

  SideEffects [None]

  SeeAlso     [Cudd_zddCountDouble]

******************************************************************************/
int
Cudd_zddCount(
  DdManager * zdd,
  DdNode * P)
{
    st_table	*table;
    int		res;
    DdNode	*base, *empty;

    base  = DD_ONE(zdd);
    empty = DD_ZERO(zdd);
    table = st_init_table(st_ptrcmp, st_ptrhash);
    if (table == NULL) return(CUDD_OUT_OF_MEM);
    res = cuddZddCountStep(P, table, base, empty);
    if (res == CUDD_OUT_OF_MEM) {
	zdd->errorCode = CUDD_MEMORY_OUT;
    }
    st_foreach(table, st_zdd_countfree, NIL(char));
    st_free_table(table);

    return(res);

} /* end of Cudd_zddCount */


/**Function********************************************************************

  Synopsis [Counts the number of minterms of a ZDD.]

  Description [Counts the number of minterms of a ZDD. The result is
  returned as a double. If the procedure runs out of memory, it
  returns (double) CUDD_OUT_OF_MEM. This procedure is used in
  Cudd_zddCountMinterm.]

  SideEffects [None]

  SeeAlso     [Cudd_zddCountMinterm Cudd_zddCount]

******************************************************************************/
double
Cudd_zddCountDouble(
  DdManager * zdd,
  DdNode * P)
{
    st_table	*table;
    double	res;
    DdNode	*base, *empty;

    base  = DD_ONE(zdd);
    empty = DD_ZERO(zdd);
    table = st_init_table(st_ptrcmp, st_ptrhash);
    if (table == NULL) return((double)CUDD_OUT_OF_MEM);
    res = cuddZddCountDoubleStep(P, table, base, empty);
    if (res == (double)CUDD_OUT_OF_MEM) {
	zdd->errorCode = CUDD_MEMORY_OUT;
    }
    st_foreach(table, st_zdd_count_dbl_free, NIL(char));
    st_free_table(table);

    return(res);

} /* end of Cudd_zddCountDouble */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddCount.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddZddCountStep(
  DdNode * P,
  st_table * table,
  DdNode * base,
  DdNode * empty)
{
    int		res;
    int		*dummy;

    if (P == empty)
	return(0);
    if (P == base)
	return(1);

    /* Check cache. */
    if (st_lookup(table, P, &dummy)) {
	res = *dummy;
	return(res);
    }

    res = cuddZddCountStep(cuddE(P), table, base, empty) +
	cuddZddCountStep(cuddT(P), table, base, empty);

    dummy = ALLOC(int, 1);
    if (dummy == NULL) {
	return(CUDD_OUT_OF_MEM);
    }
    *dummy = res;
    if (st_insert(table, (char *)P, (char *)dummy) == ST_OUT_OF_MEM) {
	FREE(dummy);
	return(CUDD_OUT_OF_MEM);
    }

    return(res);

} /* end of cuddZddCountStep */


/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddCountDouble.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static double
cuddZddCountDoubleStep(
  DdNode * P,
  st_table * table,
  DdNode * base,
  DdNode * empty)
{
    double	res;
    double	*dummy;

    if (P == empty)
	return((double)0.0);
    if (P == base)
	return((double)1.0);

    /* Check cache */
    if (st_lookup(table, P, &dummy)) {
	res = *dummy;
	return(res);
    }

    res = cuddZddCountDoubleStep(cuddE(P), table, base, empty) +
	cuddZddCountDoubleStep(cuddT(P), table, base, empty);

    dummy = ALLOC(double, 1);
    if (dummy == NULL) {
	return((double)CUDD_OUT_OF_MEM);
    }
    *dummy = res;
    if (st_insert(table, (char *)P, (char *)dummy) == ST_OUT_OF_MEM) {
	FREE(dummy);
	return((double)CUDD_OUT_OF_MEM);
    }

    return(res);

} /* end of cuddZddCountDoubleStep */


/**Function********************************************************************

  Synopsis [Frees the memory associated with the computed table of
  Cudd_zddCount.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static enum st_retval
st_zdd_countfree(
  char * key,
  char * value,
  char * arg)
{
    int	*d;

    d = (int *)value;
    FREE(d);
    return(ST_CONTINUE);

} /* end of st_zdd_countfree */


/**Function********************************************************************

  Synopsis [Frees the memory associated with the computed table of
  Cudd_zddCountDouble.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static enum st_retval
st_zdd_count_dbl_free(
  char * key,
  char * value,
  char * arg)
{
    double	*d;

    d = (double *)value;
    FREE(d);
    return(ST_CONTINUE);

} /* end of st_zdd_count_dbl_free */
