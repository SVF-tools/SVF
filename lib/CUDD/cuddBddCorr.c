/**CFile***********************************************************************

  FileName    [cuddBddCorr.c]

  PackageName [cudd]

  Synopsis    [Correlation between BDDs.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_bddCorrelation()
		<li> Cudd_bddCorrelationWeights()
		</ul>
	    Static procedures included in this module:
		<ul>
		<li> bddCorrelationAux()
		<li> bddCorrelationWeightsAux()
		<li> CorrelCompare()
		<li> CorrelHash()
		<li> CorrelCleanUp()
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

typedef struct hashEntry {
    DdNode *f;
    DdNode *g;
} HashEntry;


/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddBddCorr.c,v 1.15 2012/02/05 01:07:18 fabio Exp $";
#endif

#ifdef CORREL_STATS
static	int	num_calls;
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

static double bddCorrelationAux (DdManager *dd, DdNode *f, DdNode *g, st_table *table);
static double bddCorrelationWeightsAux (DdManager *dd, DdNode *f, DdNode *g, double *prob, st_table *table);
static int CorrelCompare (const char *key1, const char *key2);
static int CorrelHash (char *key, int modulus);
static enum st_retval CorrelCleanUp (char *key, char *value, char *arg);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Computes the correlation of f and g.]

  Description [Computes the correlation of f and g. If f == g, their
  correlation is 1. If f == g', their correlation is 0.  Returns the
  fraction of minterms in the ON-set of the EXNOR of f and g.  If it
  runs out of memory, returns (double)CUDD_OUT_OF_MEM.]

  SideEffects [None]

  SeeAlso     [Cudd_bddCorrelationWeights]

******************************************************************************/
double
Cudd_bddCorrelation(
  DdManager * manager,
  DdNode * f,
  DdNode * g)
{

    st_table	*table;
    double	correlation;

#ifdef CORREL_STATS
    num_calls = 0;
#endif

    table = st_init_table(CorrelCompare,CorrelHash);
    if (table == NULL) return((double)CUDD_OUT_OF_MEM);
    correlation = bddCorrelationAux(manager,f,g,table);
    st_foreach(table, CorrelCleanUp, NIL(char));
    st_free_table(table);
    return(correlation);

} /* end of Cudd_bddCorrelation */


/**Function********************************************************************

  Synopsis    [Computes the correlation of f and g for given input
  probabilities.]

  Description [Computes the correlation of f and g for given input
  probabilities. On input, prob\[i\] is supposed to contain the
  probability of the i-th input variable to be 1.
  If f == g, their correlation is 1. If f == g', their
  correlation is 0.  Returns the probability that f and g have the same
  value. If it runs out of memory, returns (double)CUDD_OUT_OF_MEM. The
  correlation of f and the constant one gives the probability of f.]

  SideEffects [None]

  SeeAlso     [Cudd_bddCorrelation]

******************************************************************************/
double
Cudd_bddCorrelationWeights(
  DdManager * manager,
  DdNode * f,
  DdNode * g,
  double * prob)
{

    st_table	*table;
    double	correlation;

#ifdef CORREL_STATS
    num_calls = 0;
#endif

    table = st_init_table(CorrelCompare,CorrelHash);
    if (table == NULL) return((double)CUDD_OUT_OF_MEM);
    correlation = bddCorrelationWeightsAux(manager,f,g,prob,table);
    st_foreach(table, CorrelCleanUp, NIL(char));
    st_free_table(table);
    return(correlation);

} /* end of Cudd_bddCorrelationWeights */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddCorrelation.]

  Description [Performs the recursive step of Cudd_bddCorrelation.
  Returns the fraction of minterms in the ON-set of the EXNOR of f and
  g.]

  SideEffects [None]

  SeeAlso     [bddCorrelationWeightsAux]

******************************************************************************/
static double
bddCorrelationAux(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  st_table * table)
{
    DdNode	*Fv, *Fnv, *G, *Gv, *Gnv;
    double	min, *pmin, min1, min2, *dummy;
    HashEntry	*entry;
    unsigned int topF, topG;

    statLine(dd);
#ifdef CORREL_STATS
    num_calls++;
#endif

    /* Terminal cases: only work for BDDs. */
    if (f == g) return(1.0);
    if (f == Cudd_Not(g)) return(0.0);

    /* Standardize call using the following properties:
    **     (f EXNOR g)   = (g EXNOR f)
    **     (f' EXNOR g') = (f EXNOR g).
    */
    if (f > g) {
	DdNode *tmp = f;
	f = g; g = tmp;
    }
    if (Cudd_IsComplement(f)) {
	f = Cudd_Not(f);
	g = Cudd_Not(g);
    }
    /* From now on, f is regular. */
    
    entry = ALLOC(HashEntry,1);
    if (entry == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(CUDD_OUT_OF_MEM);
    }
    entry->f = f; entry->g = g;

    /* We do not use the fact that
    ** correlation(f,g') = 1 - correlation(f,g)
    ** to minimize the risk of cancellation.
    */
    if (st_lookup(table, entry, &dummy)) {
	min = *dummy;
	FREE(entry);
	return(min);
    }

    G = Cudd_Regular(g);
    topF = cuddI(dd,f->index); topG = cuddI(dd,G->index);
    if (topF <= topG) { Fv = cuddT(f); Fnv = cuddE(f); } else { Fv = Fnv = f; }
    if (topG <= topF) { Gv = cuddT(G); Gnv = cuddE(G); } else { Gv = Gnv = G; }

    if (g != G) {
	Gv = Cudd_Not(Gv);
	Gnv = Cudd_Not(Gnv);
    }

    min1 = bddCorrelationAux(dd, Fv, Gv, table) / 2.0;
    if (min1 == (double)CUDD_OUT_OF_MEM) {
	FREE(entry);
	return(CUDD_OUT_OF_MEM);
    }
    min2 = bddCorrelationAux(dd, Fnv, Gnv, table) / 2.0; 
    if (min2 == (double)CUDD_OUT_OF_MEM) {
	FREE(entry);
	return(CUDD_OUT_OF_MEM);
    }
    min = (min1+min2);
    
    pmin = ALLOC(double,1);
    if (pmin == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return((double)CUDD_OUT_OF_MEM);
    }
    *pmin = min;

    if (st_insert(table,(char *)entry, (char *)pmin) == ST_OUT_OF_MEM) {
	FREE(entry);
	FREE(pmin);
	return((double)CUDD_OUT_OF_MEM);
    }
    return(min);

} /* end of bddCorrelationAux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddCorrelationWeigths.]

  Description []

  SideEffects [None]

  SeeAlso     [bddCorrelationAux]

******************************************************************************/
static double
bddCorrelationWeightsAux(
  DdManager * dd,
  DdNode * f,
  DdNode * g,
  double * prob,
  st_table * table)
{
    DdNode	*Fv, *Fnv, *G, *Gv, *Gnv;
    double	min, *pmin, min1, min2, *dummy;
    HashEntry	*entry;
    int		topF, topG, index;

    statLine(dd);
#ifdef CORREL_STATS
    num_calls++;
#endif

    /* Terminal cases: only work for BDDs. */
    if (f == g) return(1.0);
    if (f == Cudd_Not(g)) return(0.0);

    /* Standardize call using the following properties:
    **     (f EXNOR g)   = (g EXNOR f)
    **     (f' EXNOR g') = (f EXNOR g).
    */
    if (f > g) {
	DdNode *tmp = f;
	f = g; g = tmp;
    }
    if (Cudd_IsComplement(f)) {
	f = Cudd_Not(f);
	g = Cudd_Not(g);
    }
    /* From now on, f is regular. */
    
    entry = ALLOC(HashEntry,1);
    if (entry == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return((double)CUDD_OUT_OF_MEM);
    }
    entry->f = f; entry->g = g;

    /* We do not use the fact that
    ** correlation(f,g') = 1 - correlation(f,g)
    ** to minimize the risk of cancellation.
    */
    if (st_lookup(table, entry, &dummy)) {
	min = *dummy;
	FREE(entry);
	return(min);
    }

    G = Cudd_Regular(g);
    topF = cuddI(dd,f->index); topG = cuddI(dd,G->index);
    if (topF <= topG) {
	Fv = cuddT(f); Fnv = cuddE(f);
	index = f->index;
    } else {
	Fv = Fnv = f;
	index = G->index;
    }
    if (topG <= topF) { Gv = cuddT(G); Gnv = cuddE(G); } else { Gv = Gnv = G; }

    if (g != G) {
	Gv = Cudd_Not(Gv);
	Gnv = Cudd_Not(Gnv);
    }

    min1 = bddCorrelationWeightsAux(dd, Fv, Gv, prob, table) * prob[index];
    if (min1 == (double)CUDD_OUT_OF_MEM) {
	FREE(entry);
	return((double)CUDD_OUT_OF_MEM);
    }
    min2 = bddCorrelationWeightsAux(dd, Fnv, Gnv, prob, table) * (1.0 - prob[index]); 
    if (min2 == (double)CUDD_OUT_OF_MEM) {
	FREE(entry);
	return((double)CUDD_OUT_OF_MEM);
    }
    min = (min1+min2);
    
    pmin = ALLOC(double,1);
    if (pmin == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return((double)CUDD_OUT_OF_MEM);
    }
    *pmin = min;

    if (st_insert(table,(char *)entry, (char *)pmin) == ST_OUT_OF_MEM) {
	FREE(entry);
	FREE(pmin);
	return((double)CUDD_OUT_OF_MEM);
    }
    return(min);

} /* end of bddCorrelationWeightsAux */


/**Function********************************************************************

  Synopsis    [Compares two hash table entries.]

  Description [Compares two hash table entries. Returns 0 if they are
  identical; 1 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
CorrelCompare(
  const char * key1,
  const char * key2)
{
    HashEntry *entry1;
    HashEntry *entry2;

    entry1 = (HashEntry *) key1;
    entry2 = (HashEntry *) key2;
    if (entry1->f != entry2->f || entry1->g != entry2->g) return(1);

    return(0);

} /* end of CorrelCompare */


/**Function********************************************************************

  Synopsis    [Hashes a hash table entry.]

  Description [Hashes a hash table entry. It is patterned after
  st_strhash. Returns a value between 0 and modulus.]

  SideEffects [None]

******************************************************************************/
static int
CorrelHash(
  char * key,
  int  modulus)
{
    HashEntry *entry;
    int val = 0;

    entry = (HashEntry *) key;
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
    val = ((int) ((long)entry->f))*997 + ((int) ((long)entry->g));
#else
    val = ((int) entry->f)*997 + ((int) entry->g);
#endif

    return ((val < 0) ? -val : val) % modulus;

} /* end of CorrelHash */


/**Function********************************************************************

  Synopsis    [Frees memory associated with hash table.]

  Description [Frees memory associated with hash table. Returns
  ST_CONTINUE.]

  SideEffects [None]

******************************************************************************/
static enum st_retval
CorrelCleanUp(
  char * key,
  char * value,
  char * arg)
{
    double	*d;
    HashEntry *entry;

    entry = (HashEntry *) key;
    FREE(entry);
    d = (double *)value;
    FREE(d);
    return ST_CONTINUE;

} /* end of CorrelCleanUp */

