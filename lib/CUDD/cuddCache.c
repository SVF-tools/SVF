/**CFile***********************************************************************

  FileName    [cuddCache.c]

  PackageName [cudd]

  Synopsis    [Functions for cache insertion and lookup.]

  Description [Internal procedures included in this module:
		<ul>
		<li> cuddInitCache()
		<li> cuddCacheInsert()
		<li> cuddCacheInsert2()
		<li> cuddCacheLookup()
		<li> cuddCacheLookupZdd()
		<li> cuddCacheLookup2()
		<li> cuddCacheLookup2Zdd()
		<li> cuddConstantLookup()
		<li> cuddCacheProfile()
		<li> cuddCacheResize()
		<li> cuddCacheFlush()
		<li> cuddComputeFloorLog2()
		</ul>
	    Static procedures included in this module:
		<ul>
		</ul> ]

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

#ifdef DD_CACHE_PROFILE
#define DD_HYSTO_BINS 8
#endif

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
static char rcsid[] DD_UNUSED = "$Id: cuddCache.c,v 1.36 2012/02/05 01:07:18 fabio Exp $";
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

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Initializes the computed table.]

  Description [Initializes the computed table. It is called by
  Cudd_Init. Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Init]

******************************************************************************/
int
cuddInitCache(
  DdManager * unique /* unique table */,
  unsigned int cacheSize /* initial size of the cache */,
  unsigned int maxCacheSize /* cache size beyond which no resizing occurs */)
{
    int i;
    unsigned int logSize;
#ifndef DD_CACHE_PROFILE
    DdNodePtr *mem;
    ptruint offset;
#endif

    /* Round cacheSize to largest power of 2 not greater than the requested
    ** initial cache size. */
    logSize = cuddComputeFloorLog2(ddMax(cacheSize,unique->slots/2));
    cacheSize = 1 << logSize;
    unique->acache = ALLOC(DdCache,cacheSize+1);
    if (unique->acache == NULL) {
	unique->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    /* If the size of the cache entry is a power of 2, we want to
    ** enforce alignment to that power of two. This happens when
    ** DD_CACHE_PROFILE is not defined. */
#ifdef DD_CACHE_PROFILE
    unique->cache = unique->acache;
    unique->memused += (cacheSize) * sizeof(DdCache);
#else
    mem = (DdNodePtr *) unique->acache;
    offset = (ptruint) mem & (sizeof(DdCache) - 1);
    mem += (sizeof(DdCache) - offset) / sizeof(DdNodePtr);
    unique->cache = (DdCache *) mem;
    assert(((ptruint) unique->cache & (sizeof(DdCache) - 1)) == 0);
    unique->memused += (cacheSize+1) * sizeof(DdCache);
#endif
    unique->cacheSlots = cacheSize;
    unique->cacheShift = sizeof(int) * 8 - logSize;
    unique->maxCacheHard = maxCacheSize;
    /* If cacheSlack is non-negative, we can resize. */
    unique->cacheSlack = (int) ddMin(maxCacheSize,
	DD_MAX_CACHE_TO_SLOTS_RATIO*unique->slots) -
	2 * (int) cacheSize;
    Cudd_SetMinHit(unique,DD_MIN_HIT);
    /* Initialize to avoid division by 0 and immediate resizing. */
    unique->cacheMisses = (double) (int) (cacheSize * unique->minHit + 1);
    unique->cacheHits = 0;
    unique->totCachehits = 0;
    /* The sum of cacheMisses and totCacheMisses is always correct,
    ** even though cacheMisses is larger than it should for the reasons
    ** explained above. */
    unique->totCacheMisses = -unique->cacheMisses;
    unique->cachecollisions = 0;
    unique->cacheinserts = 0;
    unique->cacheLastInserts = 0;
    unique->cachedeletions = 0;

    /* Initialize the cache */
    for (i = 0; (unsigned) i < cacheSize; i++) {
	unique->cache[i].h = 0; /* unused slots */
	unique->cache[i].data = NULL; /* invalid entry */
#ifdef DD_CACHE_PROFILE
	unique->cache[i].count = 0;
#endif
    }

    return(1);

} /* end of cuddInitCache */


/**Function********************************************************************

  Synopsis    [Inserts a result in the cache for a function with three
  operands.]

  Description [Inserts a result in the cache for a function with three
  operands.  The operator tag (see CUDD/cuddInt.h for details) is split and stored
  into unused bits of the first two pointers.]

  SideEffects [None]

  SeeAlso     [cuddCacheInsert2 cuddCacheInsert1]

******************************************************************************/
void
cuddCacheInsert(
  DdManager * table,
  ptruint op,
  DdNode * f,
  DdNode * g,
  DdNode * h,
  DdNode * data)
{
    int posn;
    register DdCache *entry;
    ptruint uf, ug, uh;

    uf = (ptruint) f | (op & 0xe);
    ug = (ptruint) g | (op >> 4);
    uh = (ptruint) h;

    posn = ddCHash2(uh,uf,ug,table->cacheShift);
    entry = &table->cache[posn];

    table->cachecollisions += entry->data != NULL;
    table->cacheinserts++;

    entry->f    = (DdNode *) uf;
    entry->g    = (DdNode *) ug;
    entry->h    = uh;
    entry->data = data;
#ifdef DD_CACHE_PROFILE
    entry->count++;
#endif

} /* end of cuddCacheInsert */


/**Function********************************************************************

  Synopsis    [Inserts a result in the cache for a function with two
  operands.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddCacheInsert cuddCacheInsert1]

******************************************************************************/
void
cuddCacheInsert2(
  DdManager * table,
  DD_CTFP op,
  DdNode * f,
  DdNode * g,
  DdNode * data)
{
    int posn;
    register DdCache *entry;

    posn = ddCHash2(op,f,g,table->cacheShift);
    entry = &table->cache[posn];

    if (entry->data != NULL) {
	table->cachecollisions++;
    }
    table->cacheinserts++;

    entry->f = f;
    entry->g = g;
    entry->h = (ptruint) op;
    entry->data = data;
#ifdef DD_CACHE_PROFILE
    entry->count++;
#endif

} /* end of cuddCacheInsert2 */


/**Function********************************************************************

  Synopsis    [Inserts a result in the cache for a function with two
  operands.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddCacheInsert cuddCacheInsert2]

******************************************************************************/
void
cuddCacheInsert1(
  DdManager * table,
  DD_CTFP1 op,
  DdNode * f,
  DdNode * data)
{
    int posn;
    register DdCache *entry;

    posn = ddCHash2(op,f,f,table->cacheShift);
    entry = &table->cache[posn];

    if (entry->data != NULL) {
	table->cachecollisions++;
    }
    table->cacheinserts++;

    entry->f = f;
    entry->g = f;
    entry->h = (ptruint) op;
    entry->data = data;
#ifdef DD_CACHE_PROFILE
    entry->count++;
#endif

} /* end of cuddCacheInsert1 */


/**Function********************************************************************

  Synopsis    [Looks up in the cache for the result of op applied to f,
  g, and h.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup2 cuddCacheLookup1]

******************************************************************************/
DdNode *
cuddCacheLookup(
  DdManager * table,
  ptruint op,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    int posn;
    DdCache *en,*cache;
    DdNode *data;
    ptruint uf, ug, uh;

    uf = (ptruint) f | (op & 0xe);
    ug = (ptruint) g | (op >> 4);
    uh = (ptruint) h;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif

    posn = ddCHash2(uh,uf,ug,table->cacheShift);
    en = &cache[posn];
    if (en->data != NULL && en->f==(DdNodePtr)uf && en->g==(DdNodePtr)ug &&
	en->h==uh) {
	data = Cudd_Regular(en->data);
	table->cacheHits++;
	if (data->ref == 0) {
	    cuddReclaim(table,data);
	}
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0 &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddCacheLookup */


/**Function********************************************************************

  Synopsis    [Looks up in the cache for the result of op applied to f,
  g, and h.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup2Zdd cuddCacheLookup1Zdd]

******************************************************************************/
DdNode *
cuddCacheLookupZdd(
  DdManager * table,
  ptruint op,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    int posn;
    DdCache *en,*cache;
    DdNode *data;
    ptruint uf, ug, uh;

    uf = (ptruint) f | (op & 0xe);
    ug = (ptruint) g | (op >> 4);
    uh = (ptruint) h;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif

    posn = ddCHash2(uh,uf,ug,table->cacheShift);
    en = &cache[posn];
    if (en->data != NULL && en->f==(DdNodePtr)uf && en->g==(DdNodePtr)ug &&
	en->h==uh) {
	data = Cudd_Regular(en->data);
	table->cacheHits++;
	if (data->ref == 0) {
	    cuddReclaimZdd(table,data);
	}
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0 &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddCacheLookupZdd */


/**Function********************************************************************

  Synopsis    [Looks up in the cache for the result of op applied to f
  and g.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup cuddCacheLookup1]

******************************************************************************/
DdNode *
cuddCacheLookup2(
  DdManager * table,
  DD_CTFP op,
  DdNode * f,
  DdNode * g)
{
    int posn;
    DdCache *en,*cache;
    DdNode *data;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif

    posn = ddCHash2(op,f,g,table->cacheShift);
    en = &cache[posn];
    if (en->data != NULL && en->f==f && en->g==g && en->h==(ptruint)op) {
	data = Cudd_Regular(en->data);
	table->cacheHits++;
	if (data->ref == 0) {
	    cuddReclaim(table,data);
	}
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0 &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddCacheLookup2 */


/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup cuddCacheLookup2]

******************************************************************************/
DdNode *
cuddCacheLookup1(
  DdManager * table,
  DD_CTFP1 op,
  DdNode * f)
{
    int posn;
    DdCache *en,*cache;
    DdNode *data;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif

    posn = ddCHash2(op,f,f,table->cacheShift);
    en = &cache[posn];
    if (en->data != NULL && en->f==f && en->h==(ptruint)op) {
	data = Cudd_Regular(en->data);
	table->cacheHits++;
	if (data->ref == 0) {
	    cuddReclaim(table,data);
	}
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0 &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddCacheLookup1 */


/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f
  and g.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookupZdd cuddCacheLookup1Zdd]

******************************************************************************/
DdNode *
cuddCacheLookup2Zdd(
  DdManager * table,
  DD_CTFP op,
  DdNode * f,
  DdNode * g)
{
    int posn;
    DdCache *en,*cache;
    DdNode *data;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif

    posn = ddCHash2(op,f,g,table->cacheShift);
    en = &cache[posn];
    if (en->data != NULL && en->f==f && en->g==g && en->h==(ptruint)op) {
	data = Cudd_Regular(en->data);
	table->cacheHits++;
	if (data->ref == 0) {
	    cuddReclaimZdd(table,data);
	}
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0 &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddCacheLookup2Zdd */


/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookupZdd cuddCacheLookup2Zdd]

******************************************************************************/
DdNode *
cuddCacheLookup1Zdd(
  DdManager * table,
  DD_CTFP1 op,
  DdNode * f)
{
    int posn;
    DdCache *en,*cache;
    DdNode *data;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif

    posn = ddCHash2(op,f,f,table->cacheShift);
    en = &cache[posn];
    if (en->data != NULL && en->f==f && en->h==(ptruint)op) {
	data = Cudd_Regular(en->data);
	table->cacheHits++;
	if (data->ref == 0) {
	    cuddReclaimZdd(table,data);
	}
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0  &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddCacheLookup1Zdd */


/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f,
  g, and h.]

  Description [Looks up in the cache for the result of op applied to f,
  g, and h. Assumes that the calling procedure (e.g.,
  Cudd_bddIteConstant) is only interested in whether the result is
  constant or not. Returns the result if found (possibly
  DD_NON_CONSTANT); otherwise it returns NULL.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup]

******************************************************************************/
DdNode *
cuddConstantLookup(
  DdManager * table,
  ptruint op,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    int posn;
    DdCache *en,*cache;
    ptruint uf, ug, uh;

    uf = (ptruint) f | (op & 0xe);
    ug = (ptruint) g | (op >> 4);
    uh = (ptruint) h;

    cache = table->cache;
#ifdef DD_DEBUG
    if (cache == NULL) {
	return(NULL);
    }
#endif
    posn = ddCHash2(uh,uf,ug,table->cacheShift);
    en = &cache[posn];

    /* We do not reclaim here because the result should not be
     * referenced, but only tested for being a constant.
     */
    if (en->data != NULL &&
	en->f == (DdNodePtr)uf && en->g == (DdNodePtr)ug && en->h == uh) {
	table->cacheHits++;
	return(en->data);
    }

    /* Cache miss: decide whether to resize. */
    table->cacheMisses++;

    if (table->cacheSlack >= 0 &&
	table->cacheHits > table->cacheMisses * table->minHit) {
	cuddCacheResize(table);
    }

    return(NULL);

} /* end of cuddConstantLookup */


/**Function********************************************************************

  Synopsis    [Computes and prints a profile of the cache usage.]

  Description [Computes and prints a profile of the cache usage.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddCacheProfile(
  DdManager * table,
  FILE * fp)
{
    DdCache *cache = table->cache;
    int slots = table->cacheSlots;
    int nzeroes = 0;
    int i, retval;
    double exUsed;

#ifdef DD_CACHE_PROFILE
    double count, mean, meansq, stddev, expected;
    long max, min;
    int imax, imin;
    double *hystogramQ, *hystogramR; /* histograms by quotient and remainder */
    int nbins = DD_HYSTO_BINS;
    int bin;
    long thiscount;
    double totalcount, exStddev;

    meansq = mean = expected = 0.0;
    max = min = (long) cache[0].count;
    imax = imin = 0;
    totalcount = 0.0;

    hystogramQ = ALLOC(double, nbins);
    if (hystogramQ == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    hystogramR = ALLOC(double, nbins);
    if (hystogramR == NULL) {
	FREE(hystogramQ);
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (i = 0; i < nbins; i++) {
	hystogramQ[i] = 0;
	hystogramR[i] = 0;
    }

    for (i = 0; i < slots; i++) {
	thiscount = (long) cache[i].count;
	if (thiscount > max) {
	    max = thiscount;
	    imax = i;
	}
	if (thiscount < min) {
	    min = thiscount;
	    imin = i;
	}
	if (thiscount == 0) {
	    nzeroes++;
	}
	count = (double) thiscount;
	mean += count;
	meansq += count * count;
	totalcount += count;
	expected += count * (double) i;
	bin = (i * nbins) / slots;
	hystogramQ[bin] += (double) thiscount;
	bin = i % nbins;
	hystogramR[bin] += (double) thiscount;
    }
    mean /= (double) slots;
    meansq /= (double) slots;

    /* Compute the standard deviation from both the data and the
    ** theoretical model for a random distribution. */
    stddev = sqrt(meansq - mean*mean);
    exStddev = sqrt((1 - 1/(double) slots) * totalcount / (double) slots);

    retval = fprintf(fp,"Cache average accesses = %g\n",  mean);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache access standard deviation = %g ", stddev);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"(expected = %g)\n", exStddev);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache max accesses = %ld for slot %d\n", max, imax);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache min accesses = %ld for slot %d\n", min, imin);
    if (retval == EOF) return(0);
    exUsed = 100.0 * (1.0 - exp(-totalcount / (double) slots));
    retval = fprintf(fp,"Cache used slots = %.2f%% (expected %.2f%%)\n",
		     100.0 - (double) nzeroes * 100.0 / (double) slots,
		     exUsed);
    if (retval == EOF) return(0);

    if (totalcount > 0) {
	expected /= totalcount;
	retval = fprintf(fp,"Cache access hystogram for %d bins", nbins);
	if (retval == EOF) return(0);
	retval = fprintf(fp," (expected bin value = %g)\nBy quotient:",
			 expected);
	if (retval == EOF) return(0);
	for (i = nbins - 1; i>=0; i--) {
	    retval = fprintf(fp," %.0f", hystogramQ[i]);
	    if (retval == EOF) return(0);
	}
	retval = fprintf(fp,"\nBy residue: ");
	if (retval == EOF) return(0);
	for (i = nbins - 1; i>=0; i--) {
	    retval = fprintf(fp," %.0f", hystogramR[i]);
	    if (retval == EOF) return(0);
	}
	retval = fprintf(fp,"\n");
	if (retval == EOF) return(0);
    }

    FREE(hystogramQ);
    FREE(hystogramR);
#else
    for (i = 0; i < slots; i++) {
	nzeroes += cache[i].h == 0;
    }
    exUsed = 100.0 *
	(1.0 - exp(-(table->cacheinserts - table->cacheLastInserts) /
		   (double) slots));
    retval = fprintf(fp,"Cache used slots = %.2f%% (expected %.2f%%)\n",
		     100.0 - (double) nzeroes * 100.0 / (double) slots,
		     exUsed);
    if (retval == EOF) return(0);
#endif
    return(1);

} /* end of cuddCacheProfile */


/**Function********************************************************************

  Synopsis    [Resizes the cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddCacheResize(
  DdManager * table)
{
    DdCache *cache, *oldcache, *oldacache, *entry, *old;
    int i;
    int posn, shift;
    unsigned int slots, oldslots;
    double offset;
    int moved = 0;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;
#ifndef DD_CACHE_PROFILE
    ptruint misalignment;
    DdNodePtr *mem;
#endif

    oldcache = table->cache;
    oldacache = table->acache;
    oldslots = table->cacheSlots;
    slots = table->cacheSlots = oldslots << 1;

#ifdef DD_VERBOSE
    (void) fprintf(table->err,"Resizing the cache from %d to %d entries\n",
		   oldslots, slots);
    (void) fprintf(table->err,
		   "\thits = %g\tmisses = %g\thit ratio = %5.3f\n",
		   table->cacheHits, table->cacheMisses,
		   table->cacheHits / (table->cacheHits + table->cacheMisses));
#endif

    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    table->acache = cache = ALLOC(DdCache,slots+1);
    MMoutOfMemory = saveHandler;
    /* If we fail to allocate the new table we just give up. */
    if (cache == NULL) {
#ifdef DD_VERBOSE
	(void) fprintf(table->err,"Resizing failed. Giving up.\n");
#endif
	table->cacheSlots = oldslots;
	table->acache = oldacache;
	/* Do not try to resize again. */
	table->maxCacheHard = oldslots - 1;
	table->cacheSlack = - (int) (oldslots + 1);
	return;
    }
    /* If the size of the cache entry is a power of 2, we want to
    ** enforce alignment to that power of two. This happens when
    ** DD_CACHE_PROFILE is not defined. */
#ifdef DD_CACHE_PROFILE
    table->cache = cache;
#else
    mem = (DdNodePtr *) cache;
    misalignment = (ptruint) mem & (sizeof(DdCache) - 1);
    mem += (sizeof(DdCache) - misalignment) / sizeof(DdNodePtr);
    table->cache = cache = (DdCache *) mem;
    assert(((ptruint) table->cache & (sizeof(DdCache) - 1)) == 0);
#endif
    shift = --(table->cacheShift);
    table->memused += (slots - oldslots) * sizeof(DdCache);
    table->cacheSlack -= slots; /* need these many slots to double again */

    /* Clear new cache. */
    for (i = 0; (unsigned) i < slots; i++) {
	cache[i].data = NULL;
	cache[i].h = 0;
#ifdef DD_CACHE_PROFILE
	cache[i].count = 0;
#endif
    }

    /* Copy from old cache to new one. */
    for (i = 0; (unsigned) i < oldslots; i++) {
	old = &oldcache[i];
	if (old->data != NULL) {
	    posn = ddCHash2(old->h,old->f,old->g,shift);
	    entry = &cache[posn];
	    entry->f = old->f;
	    entry->g = old->g;
	    entry->h = old->h;
	    entry->data = old->data;
#ifdef DD_CACHE_PROFILE
	    entry->count = 1;
#endif
	    moved++;
	}
    }

    FREE(oldacache);

    /* Reinitialize measurements so as to avoid division by 0 and
    ** immediate resizing.
    */
    offset = (double) (int) (slots * table->minHit + 1);
    table->totCacheMisses += table->cacheMisses - offset;
    table->cacheMisses = offset;
    table->totCachehits += table->cacheHits;
    table->cacheHits = 0;
    table->cacheLastInserts = table->cacheinserts - (double) moved;

} /* end of cuddCacheResize */


/**Function********************************************************************

  Synopsis    [Flushes the cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddCacheFlush(
  DdManager * table)
{
    int i, slots;
    DdCache *cache;

    slots = table->cacheSlots;
    cache = table->cache;
    for (i = 0; i < slots; i++) {
	table->cachedeletions += cache[i].data != NULL;
	cache[i].data = NULL;
    }
    table->cacheLastInserts = table->cacheinserts;

    return;

} /* end of cuddCacheFlush */


/**Function********************************************************************

  Synopsis    [Returns the floor of the logarithm to the base 2.]

  Description [Returns the floor of the logarithm to the base 2.
  The input value is assumed to be greater than 0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddComputeFloorLog2(
  unsigned int value)
{
    int floorLog = 0;
#ifdef DD_DEBUG
    assert(value > 0);
#endif
    while (value > 1) {
	floorLog++;
	value >>= 1;
    }
    return(floorLog);

} /* end of cuddComputeFloorLog2 */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
