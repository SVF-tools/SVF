/**CFile***********************************************************************

  FileName    [cuddLCache.c]

  PackageName [cudd]

  Synopsis    [Functions for local caches.]

  Description [Internal procedures included in this module:
		<ul>
		<li> cuddLocalCacheInit()
		<li> cuddLocalCacheQuit()
		<li> cuddLocalCacheInsert()
		<li> cuddLocalCacheLookup()
		<li> cuddLocalCacheClearDead()
		<li> cuddLocalCacheClearAll()
		<li> cuddLocalCacheProfile()
		<li> cuddHashTableInit()
		<li> cuddHashTableQuit()
		<li> cuddHashTableGenericQuit()
		<li> cuddHashTableInsert()
		<li> cuddHashTableLookup()
		<li> cuddHashTableGenericInsert()
		<li> cuddHashTableGenericLookup()
		<li> cuddHashTableInsert2()
		<li> cuddHashTableLookup2()
		<li> cuddHashTableInsert3()
		<li> cuddHashTableLookup3()
		</ul>
	    Static procedures included in this module:
		<ul>
		<li> cuddLocalCacheResize()
		<li> ddLCHash()
		<li> cuddLocalCacheAddToList()
		<li> cuddLocalCacheRemoveFromList()
		<li> cuddHashTableResize()
		<li> cuddHashTableAlloc()
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

#define DD_MAX_HASHTABLE_DENSITY 2	/* tells when to resize a table */

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
static char rcsid[] DD_UNUSED = "$Id: cuddLCache.c,v 1.27 2012/02/05 01:07:19 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**Macro***********************************************************************

  Synopsis    [Computes hash function for keys of one operand.]

  Description []

  SideEffects [None]

  SeeAlso     [ddLCHash3 ddLCHash]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddLCHash1(f,shift) \
(((unsigned)(ptruint)(f) * DD_P1) >> (shift))
#else
#define ddLCHash1(f,shift) \
(((unsigned)(f) * DD_P1) >> (shift))
#endif


/**Macro***********************************************************************

  Synopsis    [Computes hash function for keys of two operands.]

  Description []

  SideEffects [None]

  SeeAlso     [ddLCHash3 ddLCHash]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddLCHash2(f,g,shift) \
((((unsigned)(ptruint)(f) * DD_P1 + \
   (unsigned)(ptruint)(g)) * DD_P2) >> (shift))
#else
#define ddLCHash2(f,g,shift) \
((((unsigned)(f) * DD_P1 + (unsigned)(g)) * DD_P2) >> (shift))
#endif


/**Macro***********************************************************************

  Synopsis    [Computes hash function for keys of three operands.]

  Description []

  SideEffects [None]

  SeeAlso     [ddLCHash2 ddLCHash]

******************************************************************************/
#define ddLCHash3(f,g,h,shift) ddCHash2(f,g,h,shift)


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void cuddLocalCacheResize (DdLocalCache *cache);
DD_INLINE static unsigned int ddLCHash (DdNodePtr *key, unsigned int keysize, int shift);
static void cuddLocalCacheAddToList (DdLocalCache *cache);
static void cuddLocalCacheRemoveFromList (DdLocalCache *cache);
static int cuddHashTableResize (DdHashTable *hash);
DD_INLINE static DdHashItem * cuddHashTableAlloc (DdHashTable *hash);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Initializes a local computed table.]

  Description [Initializes a computed table.  Returns a pointer the
  the new local cache in case of success; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddInitCache]

******************************************************************************/
DdLocalCache *
cuddLocalCacheInit(
  DdManager * manager /* manager */,
  unsigned int  keySize /* size of the key (number of operands) */,
  unsigned int  cacheSize /* Initial size of the cache */,
  unsigned int  maxCacheSize /* Size of the cache beyond which no resizing occurs */)
{
    DdLocalCache *cache;
    int logSize;

    cache = ALLOC(DdLocalCache,1);
    if (cache == NULL) {
	manager->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    cache->manager = manager;
    cache->keysize = keySize;
    cache->itemsize = (keySize + 1) * sizeof(DdNode *);
#ifdef DD_CACHE_PROFILE
    cache->itemsize += sizeof(ptrint);
#endif
    logSize = cuddComputeFloorLog2(ddMax(cacheSize,manager->slots/2));
    cacheSize = 1 << logSize;
    cache->item = (DdLocalCacheItem *)
	ALLOC(char, cacheSize * cache->itemsize);
    if (cache->item == NULL) {
	manager->errorCode = CUDD_MEMORY_OUT;
	FREE(cache);
	return(NULL);
    }
    cache->slots = cacheSize;
    cache->shift = sizeof(int) * 8 - logSize;
    cache->maxslots = ddMin(maxCacheSize,manager->slots);
    cache->minHit = manager->minHit;
    /* Initialize to avoid division by 0 and immediate resizing. */
    cache->lookUps = (double) (int) (cacheSize * cache->minHit + 1);
    cache->hits = 0;
    manager->memused += cacheSize * cache->itemsize + sizeof(DdLocalCache);

    /* Initialize the cache. */
    memset(cache->item, 0, cacheSize * cache->itemsize);

    /* Add to manager's list of local caches for GC. */
    cuddLocalCacheAddToList(cache);

    return(cache);

} /* end of cuddLocalCacheInit */


/**Function********************************************************************

  Synopsis    [Shuts down a local computed table.]

  Description [Initializes the computed table. It is called by
  Cudd_Init. Returns a pointer the the new local cache in case of
  success; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddLocalCacheInit]

******************************************************************************/
void
cuddLocalCacheQuit(
  DdLocalCache * cache /* cache to be shut down */)
{
    cache->manager->memused -=
	cache->slots * cache->itemsize + sizeof(DdLocalCache);
    cuddLocalCacheRemoveFromList(cache);
    FREE(cache->item);
    FREE(cache);

    return;

} /* end of cuddLocalCacheQuit */


/**Function********************************************************************

  Synopsis    [Inserts a result in a local cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddLocalCacheInsert(
  DdLocalCache * cache,
  DdNodePtr * key,
  DdNode * value)
{
    unsigned int posn;
    DdLocalCacheItem *entry;

    posn = ddLCHash(key,cache->keysize,cache->shift);
    entry = (DdLocalCacheItem *) ((char *) cache->item +
				  posn * cache->itemsize);
    memcpy(entry->key,key,cache->keysize * sizeof(DdNode *));
    entry->value = value;
#ifdef DD_CACHE_PROFILE
    entry->count++;
#endif

} /* end of cuddLocalCacheInsert */


/**Function********************************************************************

  Synopsis [Looks up in a local cache.]

  Description [Looks up in a local cache. Returns the result if found;
  it returns NULL if no result is found.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
cuddLocalCacheLookup(
  DdLocalCache * cache,
  DdNodePtr * key)
{
    unsigned int posn;
    DdLocalCacheItem *entry;
    DdNode *value;

    cache->lookUps++;
    posn = ddLCHash(key,cache->keysize,cache->shift);
    entry = (DdLocalCacheItem *) ((char *) cache->item +
				  posn * cache->itemsize);
    if (entry->value != NULL &&
	memcmp(key,entry->key,cache->keysize*sizeof(DdNode *)) == 0) {
	cache->hits++;
	value = Cudd_Regular(entry->value);
	if (value->ref == 0) {
	    cuddReclaim(cache->manager,value);
	}
	return(entry->value);
    }

    /* Cache miss: decide whether to resize */

    if (cache->slots < cache->maxslots &&
	cache->hits > cache->lookUps * cache->minHit) {
	cuddLocalCacheResize(cache);
    }

    return(NULL);

} /* end of cuddLocalCacheLookup */


/**Function********************************************************************

  Synopsis [Clears the dead entries of the local caches of a manager.]

  Description [Clears the dead entries of the local caches of a manager.
  Used during garbage collection.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddLocalCacheClearDead(
  DdManager * manager)
{
    DdLocalCache *cache = manager->localCaches;
    unsigned int keysize;
    unsigned int itemsize;
    unsigned int slots;
    DdLocalCacheItem *item;
    DdNodePtr *key;
    unsigned int i, j;

    while (cache != NULL) {
	keysize = cache->keysize;
	itemsize = cache->itemsize;
	slots = cache->slots;
	item = cache->item;
	for (i = 0; i < slots; i++) {
	    if (item->value != NULL) {
		if (Cudd_Regular(item->value)->ref == 0) {
		    item->value = NULL;
		} else {
		    key = item->key;
		    for (j = 0; j < keysize; j++) {
			if (Cudd_Regular(key[j])->ref == 0) {
			    item->value = NULL;
			    break;
			}
		    }
		}
	    }
	    item = (DdLocalCacheItem *) ((char *) item + itemsize);
	}
	cache = cache->next;
    }
    return;

} /* end of cuddLocalCacheClearDead */


/**Function********************************************************************

  Synopsis [Clears the local caches of a manager.]

  Description [Clears the local caches of a manager.
  Used before reordering.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddLocalCacheClearAll(
  DdManager * manager)
{
    DdLocalCache *cache = manager->localCaches;

    while (cache != NULL) {
	memset(cache->item, 0, cache->slots * cache->itemsize);
	cache = cache->next;
    }
    return;

} /* end of cuddLocalCacheClearAll */


#ifdef DD_CACHE_PROFILE

#define DD_HYSTO_BINS 8

/**Function********************************************************************

  Synopsis    [Computes and prints a profile of a local cache usage.]

  Description [Computes and prints a profile of a local cache usage.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddLocalCacheProfile(
  DdLocalCache * cache)
{
    double count, mean, meansq, stddev, expected;
    long max, min;
    int imax, imin;
    int i, retval, slots;
    long *hystogram;
    int nbins = DD_HYSTO_BINS;
    int bin;
    long thiscount;
    double totalcount;
    int nzeroes;
    DdLocalCacheItem *entry;
    FILE *fp = cache->manager->out;

    slots = cache->slots;

    meansq = mean = expected = 0.0;
    max = min = (long) cache->item[0].count;
    imax = imin = nzeroes = 0;
    totalcount = 0.0;

    hystogram = ALLOC(long, nbins);
    if (hystogram == NULL) {
	return(0);
    }
    for (i = 0; i < nbins; i++) {
	hystogram[i] = 0;
    }

    for (i = 0; i < slots; i++) {
	entry = (DdLocalCacheItem *) ((char *) cache->item +
				      i * cache->itemsize);
	thiscount = (long) entry->count;
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
	hystogram[bin] += thiscount;
    }
    mean /= (double) slots;
    meansq /= (double) slots;
    stddev = sqrt(meansq - mean*mean);

    retval = fprintf(fp,"Cache stats: slots = %d average = %g ", slots, mean);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"standard deviation = %g\n", stddev);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache max accesses = %ld for slot %d\n", max, imax);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache min accesses = %ld for slot %d\n", min, imin);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache unused slots = %d\n", nzeroes);
    if (retval == EOF) return(0);

    if (totalcount) {
	expected /= totalcount;
	retval = fprintf(fp,"Cache access hystogram for %d bins", nbins);
	if (retval == EOF) return(0);
	retval = fprintf(fp," (expected bin value = %g)\n# ", expected);
	if (retval == EOF) return(0);
	for (i = nbins - 1; i>=0; i--) {
	    retval = fprintf(fp,"%ld ", hystogram[i]);
	    if (retval == EOF) return(0);
	}
	retval = fprintf(fp,"\n");
	if (retval == EOF) return(0);
    }

    FREE(hystogram);
    return(1);

} /* end of cuddLocalCacheProfile */
#endif


/**Function********************************************************************

  Synopsis    [Initializes a hash table.]

  Description [Initializes a hash table. Returns a pointer to the new
  table if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableQuit]

******************************************************************************/
DdHashTable *
cuddHashTableInit(
  DdManager * manager,
  unsigned int  keySize,
  unsigned int  initSize)
{
    DdHashTable *hash;
    int logSize;

    hash = ALLOC(DdHashTable, 1);
    if (hash == NULL) {
	manager->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }
    hash->keysize = keySize;
    hash->manager = manager;
    hash->memoryList = NULL;
    hash->nextFree = NULL;
    hash->itemsize = (keySize + 1) * sizeof(DdNode *) +
	sizeof(ptrint) + sizeof(DdHashItem *);
    /* We have to guarantee that the shift be < 32. */
    if (initSize < 2) initSize = 2;
    logSize = cuddComputeFloorLog2(initSize);
    hash->numBuckets = 1 << logSize;
    hash->shift = sizeof(int) * 8 - logSize;
    hash->bucket = ALLOC(DdHashItem *, hash->numBuckets);
    if (hash->bucket == NULL) {
	manager->errorCode = CUDD_MEMORY_OUT;
	FREE(hash);
	return(NULL);
    }
    memset(hash->bucket, 0, hash->numBuckets * sizeof(DdHashItem *));
    hash->size = 0;
    hash->maxsize = hash->numBuckets * DD_MAX_HASHTABLE_DENSITY;
    return(hash);

} /* end of cuddHashTableInit */


/**Function********************************************************************

  Synopsis    [Shuts down a hash table.]

  Description [Shuts down a hash table, dereferencing all the values.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInit]

******************************************************************************/
void
cuddHashTableQuit(
  DdHashTable * hash)
{
    unsigned int i;
    DdManager *dd = hash->manager;
    DdHashItem *bucket;
    DdHashItem **memlist, **nextmem;
    unsigned int numBuckets = hash->numBuckets;

    for (i = 0; i < numBuckets; i++) {
	bucket = hash->bucket[i];
	while (bucket != NULL) {
	    Cudd_RecursiveDeref(dd, bucket->value);
	    bucket = bucket->next;
	}
    }

    memlist = hash->memoryList;
    while (memlist != NULL) {
	nextmem = (DdHashItem **) memlist[0];
	FREE(memlist);
	memlist = nextmem;
    }

    FREE(hash->bucket);
    FREE(hash);

    return;

} /* end of cuddHashTableQuit */


/**Function********************************************************************

  Synopsis    [Shuts down a hash table.]

  Description [Shuts down a hash table, when the values are not DdNode
  pointers.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInit]

******************************************************************************/
void
cuddHashTableGenericQuit(
  DdHashTable * hash)
{
#ifdef __osf__
#pragma pointer_size save
#pragma pointer_size short
#endif
    DdHashItem **memlist, **nextmem;

    memlist = hash->memoryList;
    while (memlist != NULL) {
	nextmem = (DdHashItem **) memlist[0];
	FREE(memlist);
	memlist = nextmem;
    }

    FREE(hash->bucket);
    FREE(hash);
#ifdef __osf__
#pragma pointer_size restore
#endif

    return;

} /* end of cuddHashTableGenericQuit */


/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key has more than
  three pointers.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [[cuddHashTableInsert1 cuddHashTableInsert2 cuddHashTableInsert3
  cuddHashTableLookup]

******************************************************************************/
int
cuddHashTableInsert(
  DdHashTable * hash,
  DdNodePtr * key,
  DdNode * value,
  ptrint count)
{
    int result;
    unsigned int posn;
    DdHashItem *item;
    unsigned int i;

#ifdef DD_DEBUG
    assert(hash->keysize > 3);
#endif

    if (hash->size > hash->maxsize) {
	result = cuddHashTableResize(hash);
	if (result == 0) return(0);
    }
    item = cuddHashTableAlloc(hash);
    if (item == NULL) return(0);
    hash->size++;
    item->value = value;
    cuddRef(value);
    item->count = count;
    for (i = 0; i < hash->keysize; i++) {
	item->key[i] = key[i];
    }
    posn = ddLCHash(key,hash->keysize,hash->shift);
    item->next = hash->bucket[posn];
    hash->bucket[posn] = item;

    return(1);

} /* end of cuddHashTableInsert */


/**Function********************************************************************

  Synopsis    [Looks up a key in a hash table.]

  Description [Looks up a key consisting of more than three pointers
  in a hash table.  Returns the value associated to the key if there
  is an entry for the given key in the table; NULL otherwise. If the
  entry is present, its reference counter is decremented if not
  saturated. If the counter reaches 0, the value of the entry is
  dereferenced, and the entry is returned to the free list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup1 cuddHashTableLookup2 cuddHashTableLookup3
  cuddHashTableInsert]

******************************************************************************/
DdNode *
cuddHashTableLookup(
  DdHashTable * hash,
  DdNodePtr * key)
{
    unsigned int posn;
    DdHashItem *item, *prev;
    unsigned int i, keysize;

#ifdef DD_DEBUG
    assert(hash->keysize > 3);
#endif

    posn = ddLCHash(key,hash->keysize,hash->shift);
    item = hash->bucket[posn];
    prev = NULL;

    keysize = hash->keysize;
    while (item != NULL) {
	DdNodePtr *key2 = item->key;
	int equal = 1;
	for (i = 0; i < keysize; i++) {
	    if (key[i] != key2[i]) {
		equal = 0;
		break;
	    }
	}
	if (equal) {
	    DdNode *value = item->value;
	    cuddSatDec(item->count);
	    if (item->count == 0) {
		cuddDeref(value);
		if (prev == NULL) {
		    hash->bucket[posn] = item->next;
		} else {
		    prev->next = item->next;
		}
		item->next = hash->nextFree;
		hash->nextFree = item;
		hash->size--;
	    }
	    return(value);
	}
	prev = item;
	item = item->next;
    }
    return(NULL);

} /* end of cuddHashTableLookup */


/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is one pointer.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert cuddHashTableInsert2 cuddHashTableInsert3
  cuddHashTableLookup1]

******************************************************************************/
int
cuddHashTableInsert1(
  DdHashTable * hash,
  DdNode * f,
  DdNode * value,
  ptrint count)
{
    int result;
    unsigned int posn;
    DdHashItem *item;

#ifdef DD_DEBUG
    assert(hash->keysize == 1);
#endif

    if (hash->size > hash->maxsize) {
	result = cuddHashTableResize(hash);
	if (result == 0) return(0);
    }
    item = cuddHashTableAlloc(hash);
    if (item == NULL) return(0);
    hash->size++;
    item->value = value;
    cuddRef(value);
    item->count = count;
    item->key[0] = f;
    posn = ddLCHash1(f,hash->shift);
    item->next = hash->bucket[posn];
    hash->bucket[posn] = item;

    return(1);

} /* end of cuddHashTableInsert1 */


/**Function********************************************************************

  Synopsis    [Looks up a key consisting of one pointer in a hash table.]

  Description [Looks up a key consisting of one pointer in a hash table.
  Returns the value associated to the key if there is an entry for the given
  key in the table; NULL otherwise. If the entry is present, its reference
  counter is decremented if not saturated. If the counter reaches 0, the
  value of the entry is dereferenced, and the entry is returned to the free
  list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup cuddHashTableLookup2 cuddHashTableLookup3
  cuddHashTableInsert1]

******************************************************************************/
DdNode *
cuddHashTableLookup1(
  DdHashTable * hash,
  DdNode * f)
{
    unsigned int posn;
    DdHashItem *item, *prev;

#ifdef DD_DEBUG
    assert(hash->keysize == 1);
#endif

    posn = ddLCHash1(f,hash->shift);
    item = hash->bucket[posn];
    prev = NULL;

    while (item != NULL) {
	DdNodePtr *key = item->key;
	if (f == key[0]) {
	    DdNode *value = item->value;
	    cuddSatDec(item->count);
	    if (item->count == 0) {
		cuddDeref(value);
		if (prev == NULL) {
		    hash->bucket[posn] = item->next;
		} else {
		    prev->next = item->next;
		}
		item->next = hash->nextFree;
		hash->nextFree = item;
		hash->size--;
	    }
	    return(value);
	}
	prev = item;
	item = item->next;
    }
    return(NULL);

} /* end of cuddHashTableLookup1 */


/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is one
  pointer and the value is not a DdNode pointer.  Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert1 cuddHashTableGenericLookup]

******************************************************************************/
int
cuddHashTableGenericInsert(
  DdHashTable * hash,
  DdNode * f,
  void * value)
{
    int result;
    unsigned int posn;
    DdHashItem *item;

#ifdef DD_DEBUG
    assert(hash->keysize == 1);
#endif

    if (hash->size > hash->maxsize) {
	result = cuddHashTableResize(hash);
	if (result == 0) return(0);
    }
    item = cuddHashTableAlloc(hash);
    if (item == NULL) return(0);
    hash->size++;
    item->value = (DdNode *) value;
    item->count = 0;
    item->key[0] = f;
    posn = ddLCHash1(f,hash->shift);
    item->next = hash->bucket[posn];
    hash->bucket[posn] = item;

    return(1);

} /* end of cuddHashTableGenericInsert */


/**Function********************************************************************

  Synopsis    [Looks up a key consisting of one pointer in a hash table.]

  Description [Looks up a key consisting of one pointer in a hash
  table when the value is not a DdNode pointer.  Returns the value
  associated to the key if there is an entry for the given key in the
  table; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup1 cuddHashTableGenericInsert]

******************************************************************************/
void *
cuddHashTableGenericLookup(
  DdHashTable * hash,
  DdNode * f)
{
    unsigned int posn;
    DdHashItem *item;

#ifdef DD_DEBUG
    assert(hash->keysize == 1);
#endif

    posn = ddLCHash1(f,hash->shift);
    item = hash->bucket[posn];

    while (item != NULL) {
	if (f == item->key[0]) {
            return ((void *) item->value);
	}
	item = item->next;
    }
    return(NULL);

} /* end of cuddHashTableGenericLookup */


/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is
  composed of two pointers. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert cuddHashTableInsert1 cuddHashTableInsert3
  cuddHashTableLookup2]

******************************************************************************/
int
cuddHashTableInsert2(
  DdHashTable * hash,
  DdNode * f,
  DdNode * g,
  DdNode * value,
  ptrint count)
{
    int result;
    unsigned int posn;
    DdHashItem *item;

#ifdef DD_DEBUG
    assert(hash->keysize == 2);
#endif

    if (hash->size > hash->maxsize) {
	result = cuddHashTableResize(hash);
	if (result == 0) return(0);
    }
    item = cuddHashTableAlloc(hash);
    if (item == NULL) return(0);
    hash->size++;
    item->value = value;
    cuddRef(value);
    item->count = count;
    item->key[0] = f;
    item->key[1] = g;
    posn = ddLCHash2(f,g,hash->shift);
    item->next = hash->bucket[posn];
    hash->bucket[posn] = item;

    return(1);

} /* end of cuddHashTableInsert2 */


/**Function********************************************************************

  Synopsis    [Looks up a key consisting of two pointers in a hash table.]

  Description [Looks up a key consisting of two pointer in a hash table.
  Returns the value associated to the key if there is an entry for the given
  key in the table; NULL otherwise. If the entry is present, its reference
  counter is decremented if not saturated. If the counter reaches 0, the
  value of the entry is dereferenced, and the entry is returned to the free
  list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup cuddHashTableLookup1 cuddHashTableLookup3
  cuddHashTableInsert2]

******************************************************************************/
DdNode *
cuddHashTableLookup2(
  DdHashTable * hash,
  DdNode * f,
  DdNode * g)
{
    unsigned int posn;
    DdHashItem *item, *prev;

#ifdef DD_DEBUG
    assert(hash->keysize == 2);
#endif

    posn = ddLCHash2(f,g,hash->shift);
    item = hash->bucket[posn];
    prev = NULL;

    while (item != NULL) {
	DdNodePtr *key = item->key;
	if ((f == key[0]) && (g == key[1])) {
	    DdNode *value = item->value;
	    cuddSatDec(item->count);
	    if (item->count == 0) {
		cuddDeref(value);
		if (prev == NULL) {
		    hash->bucket[posn] = item->next;
		} else {
		    prev->next = item->next;
		}
		item->next = hash->nextFree;
		hash->nextFree = item;
		hash->size--;
	    }
	    return(value);
	}
	prev = item;
	item = item->next;
    }
    return(NULL);

} /* end of cuddHashTableLookup2 */


/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is
  composed of three pointers. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert cuddHashTableInsert1 cuddHashTableInsert2
  cuddHashTableLookup3]

******************************************************************************/
int
cuddHashTableInsert3(
  DdHashTable * hash,
  DdNode * f,
  DdNode * g,
  DdNode * h,
  DdNode * value,
  ptrint count)
{
    int result;
    unsigned int posn;
    DdHashItem *item;

#ifdef DD_DEBUG
    assert(hash->keysize == 3);
#endif

    if (hash->size > hash->maxsize) {
	result = cuddHashTableResize(hash);
	if (result == 0) return(0);
    }
    item = cuddHashTableAlloc(hash);
    if (item == NULL) return(0);
    hash->size++;
    item->value = value;
    cuddRef(value);
    item->count = count;
    item->key[0] = f;
    item->key[1] = g;
    item->key[2] = h;
    posn = ddLCHash3(f,g,h,hash->shift);
    item->next = hash->bucket[posn];
    hash->bucket[posn] = item;

    return(1);

} /* end of cuddHashTableInsert3 */


/**Function********************************************************************

  Synopsis    [Looks up a key consisting of three pointers in a hash table.]

  Description [Looks up a key consisting of three pointers in a hash table.
  Returns the value associated to the key if there is an entry for the given
  key in the table; NULL otherwise. If the entry is present, its reference
  counter is decremented if not saturated. If the counter reaches 0, the
  value of the entry is dereferenced, and the entry is returned to the free
  list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup cuddHashTableLookup1 cuddHashTableLookup2
  cuddHashTableInsert3]

******************************************************************************/
DdNode *
cuddHashTableLookup3(
  DdHashTable * hash,
  DdNode * f,
  DdNode * g,
  DdNode * h)
{
    unsigned int posn;
    DdHashItem *item, *prev;

#ifdef DD_DEBUG
    assert(hash->keysize == 3);
#endif

    posn = ddLCHash3(f,g,h,hash->shift);
    item = hash->bucket[posn];
    prev = NULL;

    while (item != NULL) {
	DdNodePtr *key = item->key;
	if ((f == key[0]) && (g == key[1]) && (h == key[2])) {
	    DdNode *value = item->value;
	    cuddSatDec(item->count);
	    if (item->count == 0) {
		cuddDeref(value);
		if (prev == NULL) {
		    hash->bucket[posn] = item->next;
		} else {
		    prev->next = item->next;
		}
		item->next = hash->nextFree;
		hash->nextFree = item;
		hash->size--;
	    }
	    return(value);
	}
	prev = item;
	item = item->next;
    }
    return(NULL);

} /* end of cuddHashTableLookup3 */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Resizes a local cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
cuddLocalCacheResize(
  DdLocalCache * cache)
{
    DdLocalCacheItem *item, *olditem, *entry, *old;
    int i, shift;
    unsigned int posn;
    unsigned int slots, oldslots;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    olditem = cache->item;
    oldslots = cache->slots;
    slots = cache->slots = oldslots << 1;

#ifdef DD_VERBOSE
    (void) fprintf(cache->manager->err,
		   "Resizing local cache from %d to %d entries\n",
		   oldslots, slots);
    (void) fprintf(cache->manager->err,
		   "\thits = %.0f\tlookups = %.0f\thit ratio = %5.3f\n",
		   cache->hits, cache->lookUps, cache->hits / cache->lookUps);
#endif

    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    cache->item = item =
	(DdLocalCacheItem *) ALLOC(char, slots * cache->itemsize);
    MMoutOfMemory = saveHandler;
    /* If we fail to allocate the new table we just give up. */
    if (item == NULL) {
#ifdef DD_VERBOSE
	(void) fprintf(cache->manager->err,"Resizing failed. Giving up.\n");
#endif
	cache->slots = oldslots;
	cache->item = olditem;
	/* Do not try to resize again. */
	cache->maxslots = oldslots - 1;
	return;
    }
    shift = --(cache->shift);
    cache->manager->memused += (slots - oldslots) * cache->itemsize;

    /* Clear new cache. */
    memset(item, 0, slots * cache->itemsize);

    /* Copy from old cache to new one. */
    for (i = 0; (unsigned) i < oldslots; i++) {
	old = (DdLocalCacheItem *) ((char *) olditem + i * cache->itemsize);
	if (old->value != NULL) {
	    posn = ddLCHash(old->key,cache->keysize,shift);
	    entry = (DdLocalCacheItem *) ((char *) item +
					  posn * cache->itemsize);
	    memcpy(entry->key,old->key,cache->keysize*sizeof(DdNode *));
	    entry->value = old->value;
	}
    }

    FREE(olditem);

    /* Reinitialize measurements so as to avoid division by 0 and
    ** immediate resizing.
    */
    cache->lookUps = (double) (int) (slots * cache->minHit + 1);
    cache->hits = 0;

} /* end of cuddLocalCacheResize */


/**Function********************************************************************

  Synopsis    [Computes the hash value for a local cache.]

  Description [Computes the hash value for a local cache. Returns the
  bucket index.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DD_INLINE
static unsigned int
ddLCHash(
  DdNodePtr * key,
  unsigned int keysize,
  int shift)
{
    unsigned int val = (unsigned int) (ptrint) key[0] * DD_P2;
    unsigned int i;

    for (i = 1; i < keysize; i++) {
	val = val * DD_P1 + (int) (ptrint) key[i];
    }

    return(val >> shift);

} /* end of ddLCHash */


/**Function********************************************************************

  Synopsis    [Inserts a local cache in the manager list.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
cuddLocalCacheAddToList(
  DdLocalCache * cache)
{
    DdManager *manager = cache->manager;

    cache->next = manager->localCaches;
    manager->localCaches = cache;
    return;

} /* end of cuddLocalCacheAddToList */


/**Function********************************************************************

  Synopsis    [Removes a local cache from the manager list.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
cuddLocalCacheRemoveFromList(
  DdLocalCache * cache)
{
    DdManager *manager = cache->manager;
    DdLocalCache **prevCache, *nextCache;

    prevCache = &(manager->localCaches);
    nextCache = manager->localCaches;

    while (nextCache != NULL) {
	if (nextCache == cache) {
	    *prevCache = nextCache->next;
	    return;
	}
	prevCache = &(nextCache->next);
	nextCache = nextCache->next;
    }
    return;			/* should never get here */

} /* end of cuddLocalCacheRemoveFromList */


/**Function********************************************************************

  Synopsis    [Resizes a hash table.]

  Description [Resizes a hash table. Returns 1 if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert]

******************************************************************************/
static int
cuddHashTableResize(
  DdHashTable * hash)
{
    int j;
    unsigned int posn;
    DdHashItem *item;
    DdHashItem *next;
    DdNode **key;
    int numBuckets;
    DdHashItem **buckets;
    DdHashItem **oldBuckets = hash->bucket;
    int shift;
    int oldNumBuckets = hash->numBuckets;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;

    /* Compute the new size of the table. */
    numBuckets = oldNumBuckets << 1;
    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    buckets = ALLOC(DdHashItem *, numBuckets);
    MMoutOfMemory = saveHandler;
    if (buckets == NULL) {
	hash->maxsize <<= 1;
	return(1);
    }

    hash->bucket = buckets;
    hash->numBuckets = numBuckets;
    shift = --(hash->shift);
    hash->maxsize <<= 1;
    memset(buckets, 0, numBuckets * sizeof(DdHashItem *));
    if (hash->keysize == 1) {
	for (j = 0; j < oldNumBuckets; j++) {
	    item = oldBuckets[j];
	    while (item != NULL) {
		next = item->next;
		key = item->key;
		posn = ddLCHash2(key[0], key[0], shift);
		item->next = buckets[posn];
		buckets[posn] = item;
		item = next;
	    }
	}
    } else if (hash->keysize == 2) {
	for (j = 0; j < oldNumBuckets; j++) {
	    item = oldBuckets[j];
	    while (item != NULL) {
		next = item->next;
		key = item->key;
		posn = ddLCHash2(key[0], key[1], shift);
		item->next = buckets[posn];
		buckets[posn] = item;
		item = next;
	    }
	}
    } else if (hash->keysize == 3) {
	for (j = 0; j < oldNumBuckets; j++) {
	    item = oldBuckets[j];
	    while (item != NULL) {
		next = item->next;
		key = item->key;
		posn = ddLCHash3(key[0], key[1], key[2], shift);
		item->next = buckets[posn];
		buckets[posn] = item;
		item = next;
	    }
	}
    } else {
	for (j = 0; j < oldNumBuckets; j++) {
	    item = oldBuckets[j];
	    while (item != NULL) {
		next = item->next;
		posn = ddLCHash(item->key, hash->keysize, shift);
		item->next = buckets[posn];
		buckets[posn] = item;
		item = next;
	    }
	}
    }
    FREE(oldBuckets);
    return(1);

} /* end of cuddHashTableResize */


/**Function********************************************************************

  Synopsis    [Fast storage allocation for items in a hash table.]

  Description [Fast storage allocation for items in a hash table. The
  first 4 bytes of a chunk contain a pointer to the next block; the
  rest contains DD_MEM_CHUNK spaces for hash items.  Returns a pointer to
  a new item if successful; NULL is memory is full.]

  SideEffects [None]

  SeeAlso     [cuddAllocNode cuddDynamicAllocNode]

******************************************************************************/
DD_INLINE
static DdHashItem *
cuddHashTableAlloc(
  DdHashTable * hash)
{
    int i;
    unsigned int itemsize = hash->itemsize;
    extern DD_OOMFP MMoutOfMemory;
    DD_OOMFP saveHandler;
    DdHashItem **mem, *thisOne, *next, *item;

    if (hash->nextFree == NULL) {
	saveHandler = MMoutOfMemory;
	MMoutOfMemory = Cudd_OutOfMem;
	mem = (DdHashItem **) ALLOC(char,(DD_MEM_CHUNK+1) * itemsize);
	MMoutOfMemory = saveHandler;
	if (mem == NULL) {
	    if (hash->manager->stash != NULL) {
		FREE(hash->manager->stash);
		hash->manager->stash = NULL;
		/* Inhibit resizing of tables. */
		hash->manager->maxCacheHard = hash->manager->cacheSlots - 1;
		hash->manager->cacheSlack = - (int) (hash->manager->cacheSlots + 1);
		for (i = 0; i < hash->manager->size; i++) {
		    hash->manager->subtables[i].maxKeys <<= 2;
		}
		hash->manager->gcFrac = 0.2;
		hash->manager->minDead =
		    (unsigned) (0.2 * (double) hash->manager->slots);
		mem = (DdHashItem **) ALLOC(char,(DD_MEM_CHUNK+1) * itemsize);
	    }
	    if (mem == NULL) {
		(*MMoutOfMemory)((long)((DD_MEM_CHUNK + 1) * itemsize));
		hash->manager->errorCode = CUDD_MEMORY_OUT;
		return(NULL);
	    }
	}

	mem[0] = (DdHashItem *) hash->memoryList;
	hash->memoryList = mem;

	thisOne = (DdHashItem *) ((char *) mem + itemsize);
	hash->nextFree = thisOne;
	for (i = 1; i < DD_MEM_CHUNK; i++) {
	    next = (DdHashItem *) ((char *) thisOne + itemsize);
	    thisOne->next = next;
	    thisOne = next;
	}

	thisOne->next = NULL;

    }
    item = hash->nextFree;
    hash->nextFree = item->next;
    return(item);

} /* end of cuddHashTableAlloc */
