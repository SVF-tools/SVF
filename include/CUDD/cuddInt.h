/**CHeaderFile*****************************************************************

  FileName    [cuddInt.h]

  PackageName [cudd]

  Synopsis    [Internal data structures of the CUDD package.]

  Description []

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

  Revision    [$Id: cuddInt.h,v 1.142 2012/02/05 01:07:19 fabio Exp $]

******************************************************************************/

#ifndef _CUDDINT
#define _CUDDINT


/*---------------------------------------------------------------------------*/
/* Nested includes                                                           */
/*---------------------------------------------------------------------------*/

#ifdef DD_MIS
#include "array.h"
#include "list.h"
#include "st.h"
#include "espresso.h"
#include "node.h"
#ifdef SIS
#include "graph.h"
#include "astg.h"
#endif
#include "network.h"
#endif

#include <math.h>
#include "cudd.h"
#include "st.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
# define DD_INLINE __inline__
# if (__GNUC__ >2 || __GNUC_MINOR__ >=7)
#   define DD_UNUSED __attribute__ ((__unused__))
# else
#   define DD_UNUSED
# endif
#else
# if defined(__cplusplus)
#   define DD_INLINE inline
# else
#   define DD_INLINE
# endif
# define DD_UNUSED
#endif


/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DD_MAXREF		((DdHalfWord) ~0)

#define DD_DEFAULT_RESIZE	10	/* how many extra variables */
/* should be added when resizing */
#define DD_MEM_CHUNK		1022

/* These definitions work for CUDD_VALUE_TYPE == double */
#define DD_ONE_VAL		(1.0)
#define DD_ZERO_VAL		(0.0)
#define DD_EPSILON		(1.0e-12)

/* The definitions of +/- infinity in terms of HUGE_VAL work on
** the DECstations and on many other combinations of OS/compiler.
*/
#ifdef HAVE_IEEE_754
#  define DD_PLUS_INF_VAL	(HUGE_VAL)
#else
#  define DD_PLUS_INF_VAL	(10e301)
#  define DD_CRI_HI_MARK	(10e150)
#  define DD_CRI_LO_MARK	(-(DD_CRI_HI_MARK))
#endif
#define DD_MINUS_INF_VAL	(-(DD_PLUS_INF_VAL))

#define DD_NON_CONSTANT		((DdNode *) 1)	/* for Cudd_bddIteConstant */

/* Unique table and cache management constants. */
#define DD_MAX_SUBTABLE_DENSITY 4	/* tells when to resize a subtable */
/* gc when this percent are dead (measured w.r.t. slots, not keys)
** The first limit (LO) applies normally. The second limit applies when
** the package believes more space for the unique table (i.e., more dead
** nodes) would improve performance, and the unique table is not already
** too large. The third limit applies when memory is low.
*/
#define DD_GC_FRAC_LO		DD_MAX_SUBTABLE_DENSITY * 0.25
#define DD_GC_FRAC_HI		DD_MAX_SUBTABLE_DENSITY * 1.0
#define DD_GC_FRAC_MIN		0.2
#define DD_MIN_HIT		30	/* resize cache when hit ratio
					   above this percentage (default) */
#define DD_MAX_LOOSE_FRACTION	5 /* 1 / (max fraction of memory used for
				     unique table in fast growth mode) */
#define DD_MAX_CACHE_FRACTION	3 /* 1 / (max fraction of memory used for
				     computed table if resizing enabled) */
#define DD_STASH_FRACTION	64 /* 1 / (fraction of memory set
				      aside for emergencies) */
#define DD_MAX_CACHE_TO_SLOTS_RATIO 4 /* used to limit the cache size */

/* Variable ordering default parameter values. */
#define DD_SIFT_MAX_VAR		1000
#define DD_SIFT_MAX_SWAPS	2000000
#define DD_DEFAULT_RECOMB	0
#define DD_MAX_REORDER_GROWTH	1.2
#define DD_FIRST_REORDER	4004	/* 4 for the constants */
#define DD_DYN_RATIO		2	/* when to dynamically reorder */

/* Primes for cache hash functions. */
#define DD_P1			12582917
#define DD_P2			4256249
#define DD_P3			741457
#define DD_P4			1618033999

/* Cache tags for 3-operand operators.  These tags are stored in the
** least significant bits of the cache operand pointers according to
** the following scheme.  The tag consists of two hex digits.  Both digits
** must be even, so that they do not interfere with complementation bits.
** The least significant one is stored in Bits 3:1 of the f operand in the
** cache entry.  Bit 1 is always 1, so that we can differentiate
** three-operand operations from one- and two-operand operations.
** Therefore, the least significant digit is one of {2,6,a,e}.  The most
** significant digit occupies Bits 3:1 of the g operand in the cache
** entry.  It can by any even digit between 0 and e.  This gives a total
** of 5 bits for the tag proper, which means a maximum of 32 three-operand
** operations. */
#define DD_ADD_ITE_TAG				0x02
#define DD_BDD_AND_ABSTRACT_TAG			0x06
#define DD_BDD_XOR_EXIST_ABSTRACT_TAG		0x0a
#define DD_BDD_ITE_TAG				0x0e
#define DD_ADD_BDD_DO_INTERVAL_TAG		0x22
#define DD_BDD_CLIPPING_AND_ABSTRACT_UP_TAG	0x26
#define DD_BDD_CLIPPING_AND_ABSTRACT_DOWN_TAG	0x2a
#define DD_BDD_COMPOSE_RECUR_TAG		0x2e
#define DD_ADD_COMPOSE_RECUR_TAG		0x42
#define DD_ADD_NON_SIM_COMPOSE_TAG		0x46
#define DD_EQUIV_DC_TAG				0x4a
#define DD_ZDD_ITE_TAG				0x4e
#define DD_ADD_ITE_CONSTANT_TAG			0x62
#define DD_ADD_EVAL_CONST_TAG			0x66
#define DD_BDD_ITE_CONSTANT_TAG			0x6a
#define DD_ADD_OUT_SUM_TAG			0x6e
#define DD_BDD_LEQ_UNLESS_TAG			0x82
#define DD_ADD_TRIANGLE_TAG			0x86
#define DD_BDD_MAX_EXP_TAG			0x8a

/* Generator constants. */
#define CUDD_GEN_CUBES 0
#define CUDD_GEN_PRIMES 1
#define CUDD_GEN_NODES 2
#define CUDD_GEN_ZDD_PATHS 3
#define CUDD_GEN_EMPTY 0
#define CUDD_GEN_NONEMPTY 1


/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

struct DdGen
{
    DdManager	*manager;
    int		type;
    int		status;
    union
    {
        struct
        {
            int			*cube;
            CUDD_VALUE_TYPE	value;
        } cubes;
        struct
        {
            int			*cube;
            DdNode		*ub;
        } primes;
        struct
        {
            int                 size;
        } nodes;
    } gen;
    struct
    {
        int	sp;
        DdNode	**stack;
    } stack;
    DdNode	*node;
};


/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/* Hooks in CUDD are functions that the application registers with the
** manager so that they are called at appropriate times. The functions
** are passed the manager as argument; they should return 1 if
** successful and 0 otherwise.
*/
typedef struct DdHook  		/* hook list element */
{
    DD_HFP f; /* function to be called */
    struct DdHook *next;	/* next element in the list */
} DdHook;

#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
typedef long ptrint;
typedef unsigned long ptruint;
#else
typedef int ptrint;
typedef unsigned int ptruint;
#endif

typedef DdNode *DdNodePtr;

/* Generic local cache item. */
typedef struct DdLocalCacheItem
{
    DdNode *value;
#ifdef DD_CACHE_PROFILE
    ptrint count;
#endif
    DdNode *key[1];
} DdLocalCacheItem;

/* Local cache. */
typedef struct DdLocalCache
{
    DdLocalCacheItem *item;
    unsigned int itemsize;
    unsigned int keysize;
    unsigned int slots;
    int shift;
    double lookUps;
    double minHit;
    double hits;
    unsigned int maxslots;
    DdManager *manager;
    struct DdLocalCache *next;
} DdLocalCache;

/* Generic hash item. */
typedef struct DdHashItem
{
    struct DdHashItem *next;
    ptrint count;
    DdNode *value;
    DdNode *key[1];
} DdHashItem;

/* Local hash table */
typedef struct DdHashTable
{
    unsigned int keysize;
    unsigned int itemsize;
    DdHashItem **bucket;
    DdHashItem *nextFree;
    DdHashItem **memoryList;
    unsigned int numBuckets;
    int shift;
    unsigned int size;
    unsigned int maxsize;
    DdManager *manager;
} DdHashTable;

typedef struct DdCache
{
    DdNode *f,*g;		/* DDs */
    ptruint h;			/* either operator or DD */
    DdNode *data;		/* already constructed DD */
#ifdef DD_CACHE_PROFILE
    ptrint count;
#endif
} DdCache;

typedef struct DdSubtable  	/* subtable for one index */
{
    DdNode **nodelist;		/* hash table */
    int shift;			/* shift for hash function */
    unsigned int slots;		/* size of the hash table */
    unsigned int keys;		/* number of nodes stored in this table */
    unsigned int maxKeys;	/* slots * DD_MAX_SUBTABLE_DENSITY */
    unsigned int dead;		/* number of dead nodes in this table */
    unsigned int next;		/* index of next variable in group */
    int bindVar;		/* flag to bind this variable to its level */
    /* Fields for lazy sifting. */
    Cudd_VariableType varType;  /* variable type (ps, ns, pi) */
    int pairIndex;              /* corresponding variable index (ps <-> ns) */
    int varHandled;		/* flag: 1 means variable is already handled */
    Cudd_LazyGroupType varToBeGrouped; /* tells what grouping to apply */
} DdSubtable;

struct DdManager  	/* specialized DD symbol table */
{
    /* Constants */
    DdNode sentinel;		/* for collision lists */
    DdNode *one;		/* constant 1 */
    DdNode *zero;		/* constant 0 */
    DdNode *plusinfinity;	/* plus infinity */
    DdNode *minusinfinity;	/* minus infinity */
    DdNode *background;		/* background value */
    /* Computed Table */
    DdCache *acache;		/* address of allocated memory for cache */
    DdCache *cache;		/* the cache-based computed table */
    unsigned int cacheSlots;	/* total number of cache entries */
    int cacheShift;		/* shift value for cache hash function */
    double cacheMisses;		/* number of cache misses (since resizing) */
    double cacheHits;		/* number of cache hits (since resizing) */
    double minHit;		/* hit percentage above which to resize */
    int cacheSlack;		/* slots still available for resizing */
    unsigned int maxCacheHard;	/* hard limit for cache size */
    /* Unique Table */
    int size;			/* number of unique subtables */
    int sizeZ;			/* for ZDD */
    int maxSize;		/* max number of subtables before resizing */
    int maxSizeZ;		/* for ZDD */
    DdSubtable *subtables;	/* array of unique subtables */
    DdSubtable *subtableZ;	/* for ZDD */
    DdSubtable constants;	/* unique subtable for the constants */
    unsigned int slots;		/* total number of hash buckets */
    unsigned int keys;		/* total number of BDD and ADD nodes */
    unsigned int keysZ;		/* total number of ZDD nodes */
    unsigned int dead;		/* total number of dead BDD and ADD nodes */
    unsigned int deadZ;		/* total number of dead ZDD nodes */
    unsigned int maxLive;	/* maximum number of live nodes */
    unsigned int minDead;	/* do not GC if fewer than these dead */
    double gcFrac;		/* gc when this fraction is dead */
    int gcEnabled;		/* gc is enabled */
    unsigned int looseUpTo;	/* slow growth beyond this limit */
    /* (measured w.r.t. slots, not keys) */
    unsigned int initSlots;	/* initial size of a subtable */
    DdNode **stack;		/* stack for iterative procedures */
    double allocated;		/* number of nodes allocated */
    /* (not during reordering) */
    double reclaimed;		/* number of nodes brought back from the dead */
    int isolated;		/* isolated projection functions */
    int *perm;			/* current variable perm. (index to level) */
    int *permZ;			/* for ZDD */
    int *invperm;		/* current inv. var. perm. (level to index) */
    int *invpermZ;		/* for ZDD */
    DdNode **vars;		/* projection functions */
    int *map;			/* variable map for fast swap */
    DdNode **univ;		/* ZDD 1 for each variable */
    int linearSize;		/* number of rows and columns of linear */
    long *interact;		/* interacting variable matrix */
    long *linear;		/* linear transform matrix */
    /* Memory Management */
    DdNode **memoryList;	/* memory manager for symbol table */
    DdNode *nextFree;		/* list of free nodes */
    char *stash;		/* memory reserve */
#ifndef DD_NO_DEATH_ROW
    DdNode **deathRow;		/* queue for dereferencing */
    int deathRowDepth;		/* number of slots in the queue */
    int nextDead;		/* index in the queue */
    unsigned deadMask;		/* mask for circular index update */
#endif
    /* General Parameters */
    CUDD_VALUE_TYPE epsilon;	/* tolerance on comparisons */
    /* Dynamic Reordering Parameters */
    int reordered;		/* flag set at the end of reordering */
    unsigned int reorderings;	/* number of calls to Cudd_ReduceHeap */
    unsigned int maxReorderings;/* maximum number of calls to Cudd_ReduceHeap */
    int siftMaxVar;		/* maximum number of vars sifted */
    int siftMaxSwap;		/* maximum number of swaps per sifting */
    double maxGrowth;		/* maximum growth during reordering */
    double maxGrowthAlt;	/* alternate maximum growth for reordering */
    int reordCycle;		/* how often to apply alternate threshold */
    int autoDyn;		/* automatic dynamic reordering flag (BDD) */
    int autoDynZ;		/* automatic dynamic reordering flag (ZDD) */
    Cudd_ReorderingType autoMethod;  /* default reordering method */
    Cudd_ReorderingType autoMethodZ; /* default reordering method (ZDD) */
    int realign;		/* realign ZDD order after BDD reordering */
    int realignZ;		/* realign BDD order after ZDD reordering */
    unsigned int nextDyn;	/* reorder if this size is reached */
    unsigned int countDead;	/* if 0, count deads to trigger reordering */
    MtrNode *tree;		/* variable group tree (BDD) */
    MtrNode *treeZ;		/* variable group tree (ZDD) */
    Cudd_AggregationType groupcheck; /* used during group sifting */
    int recomb;			/* used during group sifting */
    int symmviolation;		/* used during group sifting */
    int arcviolation;		/* used during group sifting */
    int populationSize;		/* population size for GA */
    int	numberXovers;		/* number of crossovers for GA */
    unsigned int randomizeOrder; /* perturb the next reordering threshold */
    DdLocalCache *localCaches;	/* local caches currently in existence */
    char *hooks;		/* application-specific field (used by vis) */
    DdHook *preGCHook;		/* hooks to be called before GC */
    DdHook *postGCHook;		/* hooks to be called after GC */
    DdHook *preReorderingHook;	/* hooks to be called before reordering */
    DdHook *postReorderingHook;	/* hooks to be called after reordering */
    FILE *out;			/* stdout for this manager */
    FILE *err;			/* stderr for this manager */
    Cudd_ErrorType errorCode;	/* info on last error */
    unsigned long startTime;    /* start time in milliseconds */
    unsigned long timeLimit;    /* CPU time limit */
    /* Statistical counters. */
    unsigned long memused;	/* total memory allocated for the manager */
    unsigned long maxmem;	/* target maximum memory */
    unsigned long maxmemhard;	/* hard limit for maximum memory */
    int garbageCollections;	/* number of garbage collections */
    unsigned long GCTime;	/* total time spent in garbage collection */
    unsigned long reordTime;	/* total time spent in reordering */
    double totCachehits;	/* total number of cache hits */
    double totCacheMisses;	/* total number of cache misses */
    double cachecollisions;	/* number of cache collisions */
    double cacheinserts;	/* number of cache insertions */
    double cacheLastInserts;	/* insertions at the last cache resizing */
    double cachedeletions;	/* number of deletions during garbage coll. */
#ifdef DD_STATS
    double nodesFreed;		/* number of nodes returned to the free list */
    double nodesDropped;	/* number of nodes killed by dereferencing */
#endif
    unsigned int peakLiveNodes;	/* maximum number of live nodes */
#ifdef DD_UNIQUE_PROFILE
    double uniqueLookUps;	/* number of unique table lookups */
    double uniqueLinks;		/* total distance traveled in coll. chains */
#endif
#ifdef DD_COUNT
    double recursiveCalls;	/* number of recursive calls */
#ifdef DD_STATS
    double nextSample;		/* when to write next line of stats */
#endif
    double swapSteps;		/* number of elementary reordering steps */
#endif
#ifdef DD_MIS
    /* mis/verif compatibility fields */
    array_t *iton;		/* maps ids in ddNode to node_t */
    array_t *order;		/* copy of order_list */
    lsHandle handle;		/* where it is in network BDD list */
    network_t *network;
    st_table *local_order;	/* for local BDDs */
    int nvars;			/* variables used so far */
    int threshold;		/* for pseudo var threshold value*/
#endif
};

typedef struct Move
{
    DdHalfWord x;
    DdHalfWord y;
    unsigned int flags;
    int size;
    struct Move *next;
} Move;

/* Generic level queue item. */
typedef struct DdQueueItem
{
    struct DdQueueItem *next;
    struct DdQueueItem *cnext;
    void *key;
} DdQueueItem;

/* Level queue. */
typedef struct DdLevelQueue
{
    void *first;
    DdQueueItem **last;
    DdQueueItem *freelist;
    DdQueueItem **buckets;
    int levels;
    int itemsize;
    int size;
    int maxsize;
    int numBuckets;
    int shift;
} DdLevelQueue;

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**Macro***********************************************************************

  Synopsis    [Adds node to the head of the free list.]

  Description [Adds node to the head of the free list.  Does not
  deallocate memory chunks that become free.  This function is also
  used by the dynamic reordering functions.]

  SideEffects [None]

  SeeAlso     [cuddAllocNode cuddDynamicAllocNode cuddDeallocMove]

******************************************************************************/
#define cuddDeallocNode(unique,node) \
    (node)->next = (unique)->nextFree; \
    (unique)->nextFree = node;

/**Macro***********************************************************************

  Synopsis    [Adds node to the head of the free list.]

  Description [Adds node to the head of the free list.  Does not
  deallocate memory chunks that become free.  This function is also
  used by the dynamic reordering functions.]

  SideEffects [None]

  SeeAlso     [cuddDeallocNode cuddDynamicAllocNode]

******************************************************************************/
#define cuddDeallocMove(unique,node) \
    ((DdNode *)(node))->ref = 0; \
    ((DdNode *)(node))->next = (unique)->nextFree; \
    (unique)->nextFree = (DdNode *)(node);


/**Macro***********************************************************************

  Synopsis     [Increases the reference count of a node, if it is not
  saturated.]

  Description  [Increases the reference count of a node, if it is not
  saturated. This being a macro, it is faster than Cudd_Ref, but it
  cannot be used in constructs like cuddRef(a = b()).]

  SideEffects  [none]

  SeeAlso      [Cudd_Ref]

******************************************************************************/
#define cuddRef(n) cuddSatInc(Cudd_Regular(n)->ref)


/**Macro***********************************************************************

  Synopsis     [Decreases the reference count of a node, if it is not
  saturated.]

  Description  [Decreases the reference count of node. It is primarily
  used in recursive procedures to decrease the ref count of a result
  node before returning it. This accomplishes the goal of removing the
  protection applied by a previous cuddRef. This being a macro, it is
  faster than Cudd_Deref, but it cannot be used in constructs like
  cuddDeref(a = b()).]

  SideEffects  [none]

  SeeAlso      [Cudd_Deref]

******************************************************************************/
#define cuddDeref(n) cuddSatDec(Cudd_Regular(n)->ref)


/**Macro***********************************************************************

  Synopsis     [Returns 1 if the node is a constant node.]

  Description  [Returns 1 if the node is a constant node (rather than an
  internal node). All constant nodes have the same index
  (CUDD_CONST_INDEX). The pointer passed to cuddIsConstant must be regular.]

  SideEffects  [none]

  SeeAlso      [Cudd_IsConstant]

******************************************************************************/
#define cuddIsConstant(node) ((node)->index == CUDD_CONST_INDEX)


/**Macro***********************************************************************

  Synopsis     [Returns the then child of an internal node.]

  Description  [Returns the then child of an internal node. If
  <code>node</code> is a constant node, the result is unpredictable.
  The pointer passed to cuddT must be regular.]

  SideEffects  [none]

  SeeAlso      [Cudd_T]

******************************************************************************/
#define cuddT(node) ((node)->type.kids.T)


/**Macro***********************************************************************

  Synopsis     [Returns the else child of an internal node.]

  Description  [Returns the else child of an internal node. If
  <code>node</code> is a constant node, the result is unpredictable.
  The pointer passed to cuddE must be regular.]

  SideEffects  [none]

  SeeAlso      [Cudd_E]

******************************************************************************/
#define cuddE(node) ((node)->type.kids.E)


/**Macro***********************************************************************

  Synopsis     [Returns the value of a constant node.]

  Description  [Returns the value of a constant node. If
  <code>node</code> is an internal node, the result is unpredictable.
  The pointer passed to cuddV must be regular.]

  SideEffects  [none]

  SeeAlso      [Cudd_V]

******************************************************************************/
#define cuddV(node) ((node)->type.value)


/**Macro***********************************************************************

  Synopsis    [Finds the current position of variable index in the
  order.]

  Description [Finds the current position of variable index in the
  order.  This macro duplicates the functionality of Cudd_ReadPerm,
  but it does not check for out-of-bounds indices and it is more
  efficient.]

  SideEffects [none]

  SeeAlso     [Cudd_ReadPerm]

******************************************************************************/
#define	cuddI(dd,index) (((index)==CUDD_CONST_INDEX)?(int)(index):(dd)->perm[(index)])


/**Macro***********************************************************************

  Synopsis    [Finds the current position of ZDD variable index in the
  order.]

  Description [Finds the current position of ZDD variable index in the
  order.  This macro duplicates the functionality of Cudd_ReadPermZdd,
  but it does not check for out-of-bounds indices and it is more
  efficient.]

  SideEffects [none]

  SeeAlso     [Cudd_ReadPermZdd]

******************************************************************************/
#define	cuddIZ(dd,index) (((index)==CUDD_CONST_INDEX)?(int)(index):(dd)->permZ[(index)])


/**Macro***********************************************************************

  Synopsis    [Hash function for the unique table.]

  Description []

  SideEffects [none]

  SeeAlso     [ddCHash ddCHash2]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddHash(f,g,s) \
((((unsigned)(ptruint)(f) * DD_P1 + \
   (unsigned)(ptruint)(g)) * DD_P2) >> (s))
#else
#define ddHash(f,g,s) \
((((unsigned)(f) * DD_P1 + (unsigned)(g)) * DD_P2) >> (s))
#endif


/**Macro***********************************************************************

  Synopsis    [Hash function for the cache.]

  Description []

  SideEffects [none]

  SeeAlso     [ddHash ddCHash2]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddCHash(o,f,g,h,s) \
((((((unsigned)(ptruint)(f) + (unsigned)(ptruint)(o)) * DD_P1 + \
    (unsigned)(ptruint)(g)) * DD_P2 + \
   (unsigned)(ptruint)(h)) * DD_P3) >> (s))
#else
#define ddCHash(o,f,g,h,s) \
((((((unsigned)(f) + (unsigned)(o)) * DD_P1 + (unsigned)(g)) * DD_P2 + \
   (unsigned)(h)) * DD_P3) >> (s))
#endif


/**Macro***********************************************************************

  Synopsis    [Hash function for the cache for functions with two
  operands.]

  Description []

  SideEffects [none]

  SeeAlso     [ddHash ddCHash]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddCHash2(o,f,g,s) \
(((((unsigned)(ptruint)(f) + (unsigned)(ptruint)(o)) * DD_P1 + \
   (unsigned)(ptruint)(g)) * DD_P2) >> (s))
#else
#define ddCHash2(o,f,g,s) \
(((((unsigned)(f) + (unsigned)(o)) * DD_P1 + (unsigned)(g)) * DD_P2) >> (s))
#endif


/**Macro***********************************************************************

  Synopsis    [Clears the 4 least significant bits of a pointer.]

  Description []

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
#define cuddClean(p) ((DdNode *)((ptruint)(p) & ~0xf))


/**Macro***********************************************************************

  Synopsis    [Computes the minimum of two numbers.]

  Description []

  SideEffects [none]

  SeeAlso     [ddMax]

******************************************************************************/
#define ddMin(x,y) (((y) < (x)) ? (y) : (x))


/**Macro***********************************************************************

  Synopsis    [Computes the maximum of two numbers.]

  Description []

  SideEffects [none]

  SeeAlso     [ddMin]

******************************************************************************/
#define ddMax(x,y) (((y) > (x)) ? (y) : (x))


/**Macro***********************************************************************

  Synopsis    [Computes the absolute value of a number.]

  Description []

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
#define ddAbs(x) (((x)<0) ? -(x) : (x))


/**Macro***********************************************************************

  Synopsis    [Returns 1 if the absolute value of the difference of the two
  arguments x and y is less than e.]

  Description []

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
#define ddEqualVal(x,y,e) (ddAbs((x)-(y))<(e))


/**Macro***********************************************************************

  Synopsis    [Saturating increment operator.]

  Description []

  SideEffects [none]

  SeeAlso     [cuddSatDec]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define cuddSatInc(x) ((x)++)
#else
#define cuddSatInc(x) ((x) += (x) != (DdHalfWord)DD_MAXREF)
#endif


/**Macro***********************************************************************

  Synopsis    [Saturating decrement operator.]

  Description []

  SideEffects [none]

  SeeAlso     [cuddSatInc]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define cuddSatDec(x) ((x)--)
#else
#define cuddSatDec(x) ((x) -= (x) != (DdHalfWord)DD_MAXREF)
#endif


/**Macro***********************************************************************

  Synopsis    [Returns the constant 1 node.]

  Description []

  SideEffects [none]

  SeeAlso     [DD_ZERO DD_PLUS_INFINITY DD_MINUS_INFINITY]

******************************************************************************/
#define DD_ONE(dd)		((dd)->one)


/**Macro***********************************************************************

  Synopsis    [Returns the arithmetic 0 constant node.]

  Description [Returns the arithmetic 0 constant node. This is different
  from the logical zero. The latter is obtained by
  Cudd_Not(DD_ONE(dd)).]

  SideEffects [none]

  SeeAlso     [DD_ONE Cudd_Not DD_PLUS_INFINITY DD_MINUS_INFINITY]

******************************************************************************/
#define DD_ZERO(dd) ((dd)->zero)


/**Macro***********************************************************************

  Synopsis    [Returns the plus infinity constant node.]

  Description []

  SideEffects [none]

  SeeAlso     [DD_ONE DD_ZERO DD_MINUS_INFINITY]

******************************************************************************/
#define DD_PLUS_INFINITY(dd) ((dd)->plusinfinity)


/**Macro***********************************************************************

  Synopsis    [Returns the minus infinity constant node.]

  Description []

  SideEffects [none]

  SeeAlso     [DD_ONE DD_ZERO DD_PLUS_INFINITY]

******************************************************************************/
#define DD_MINUS_INFINITY(dd) ((dd)->minusinfinity)


/**Macro***********************************************************************

  Synopsis    [Enforces DD_MINUS_INF_VAL <= x <= DD_PLUS_INF_VAL.]

  Description [Enforces DD_MINUS_INF_VAL <= x <= DD_PLUS_INF_VAL.
  Furthermore, if x <= DD_MINUS_INF_VAL/2, x is set to
  DD_MINUS_INF_VAL. Similarly, if DD_PLUS_INF_VAL/2 <= x, x is set to
  DD_PLUS_INF_VAL.  Normally this macro is a NOOP. However, if
  HAVE_IEEE_754 is not defined, it makes sure that a value does not
  get larger than infinity in absolute value, and once it gets to
  infinity, stays there.  If the value overflows before this macro is
  applied, no recovery is possible.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
#ifdef HAVE_IEEE_754
#define cuddAdjust(x)
#else
#define cuddAdjust(x)		((x) = ((x) >= DD_CRI_HI_MARK) ? DD_PLUS_INF_VAL : (((x) <= DD_CRI_LO_MARK) ? DD_MINUS_INF_VAL : (x)))
#endif


/**Macro***********************************************************************

  Synopsis    [Extract the least significant digit of a double digit.]

  Description [Extract the least significant digit of a double digit. Used
  in the manipulation of arbitrary precision integers.]

  SideEffects [None]

  SeeAlso     [DD_MSDIGIT]

******************************************************************************/
#define DD_LSDIGIT(x)	((x) & DD_APA_MASK)


/**Macro***********************************************************************

  Synopsis    [Extract the most significant digit of a double digit.]

  Description [Extract the most significant digit of a double digit. Used
  in the manipulation of arbitrary precision integers.]

  SideEffects [None]

  SeeAlso     [DD_LSDIGIT]

******************************************************************************/
#define DD_MSDIGIT(x)	((x) >> DD_APA_BITS)


/**Macro***********************************************************************

  Synopsis    [Outputs a line of stats.]

  Description [Outputs a line of stats if DD_COUNT and DD_STATS are
  defined. Increments the number of recursive calls if DD_COUNT is
  defined.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
#ifdef DD_COUNT
#ifdef DD_STATS
#define statLine(dd) dd->recursiveCalls++; \
if (dd->recursiveCalls == dd->nextSample) {(void) fprintf(dd->err, \
"@%.0f: %u nodes %u live %.0f dropped %.0f reclaimed\n", dd->recursiveCalls, \
dd->keys, dd->keys - dd->dead, dd->nodesDropped, dd->reclaimed); \
dd->nextSample += 250000;}
#else
#define statLine(dd) dd->recursiveCalls++;
#endif
#else
#define statLine(dd)
#endif


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern DdNode * cuddAddIteRecur (DdManager *dd, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * cuddAddCmplRecur (DdManager *dd, DdNode *f);
extern int cuddAnnealing (DdManager *table, int lower, int upper);
extern DdNode * cuddBddExistAbstractRecur (DdManager *manager, DdNode *f, DdNode *cube);
extern DdNode * cuddBddXorExistAbstractRecur (DdManager *manager, DdNode *f, DdNode *g, DdNode *cube);
extern DdNode * cuddBddBooleanDiffRecur (DdManager *manager, DdNode *f, DdNode *var);
extern DdNode * cuddBddIteRecur (DdManager *dd, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * cuddBddIntersectRecur (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode * cuddBddAndRecur (DdManager *manager, DdNode *f, DdNode *g);
extern DdNode * cuddBddXorRecur (DdManager *manager, DdNode *f, DdNode *g);
extern int cuddInitCache (DdManager *unique, unsigned int cacheSize, unsigned int maxCacheSize);
extern void cuddCacheInsert (DdManager *table, ptruint op, DdNode *f, DdNode *g, DdNode *h, DdNode *data);
extern void cuddCacheInsert2 (DdManager *table, DdNode * (*)(DdManager *, DdNode *, DdNode *), DdNode *f, DdNode *g, DdNode *data);
extern void cuddCacheInsert1 (DdManager *table, DdNode * (*)(DdManager *, DdNode *), DdNode *f, DdNode *data);
extern DdNode * cuddCacheLookup (DdManager *table, ptruint op, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * cuddCacheLookupZdd (DdManager *table, ptruint op, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * cuddCacheLookup2 (DdManager *table, DdNode * (*)(DdManager *, DdNode *, DdNode *), DdNode *f, DdNode *g);
extern DdNode * cuddCacheLookup1 (DdManager *table, DdNode * (*)(DdManager *, DdNode *), DdNode *f);
extern DdNode * cuddCacheLookup2Zdd (DdManager *table, DdNode * (*)(DdManager *, DdNode *, DdNode *), DdNode *f, DdNode *g);
extern DdNode * cuddCacheLookup1Zdd (DdManager *table, DdNode * (*)(DdManager *, DdNode *), DdNode *f);
extern DdNode * cuddConstantLookup (DdManager *table, ptruint op, DdNode *f, DdNode *g, DdNode *h);
extern void cuddCacheResize (DdManager *table);
extern void cuddCacheFlush (DdManager *table);
extern int cuddComputeFloorLog2 (unsigned int value);
extern void cuddPrintNode (DdNode *f, FILE *fp);
extern void cuddPrintVarGroups (DdManager * dd, MtrNode * root, int zdd, int silent);
extern void cuddGetBranches (DdNode *g, DdNode **g1, DdNode **g0);
extern DdNode * cuddCofactorRecur (DdManager *dd, DdNode *f, DdNode *g);
extern int cuddExact (DdManager *table, int lower, int upper);
extern int cuddGa (DdManager *table, int lower, int upper);
extern int cuddTreeSifting (DdManager *table, Cudd_ReorderingType method);
extern int cuddZddInitUniv (DdManager *zdd);
extern void cuddZddFreeUniv (DdManager *zdd);
extern void cuddSetInteract (DdManager *table, int x, int y);
extern int cuddTestInteract (DdManager *table, int x, int y);
extern int cuddInitInteract (DdManager *table);
extern void cuddLocalCacheClearDead (DdManager *manager);
extern void cuddLocalCacheClearAll (DdManager *manager);
#ifdef DD_CACHE_PROFILE
extern int cuddLocalCacheProfile (DdLocalCache *cache);
#endif
extern DdHashTable * cuddHashTableInit (DdManager *manager, unsigned int keySize, unsigned int initSize);
extern void cuddHashTableQuit (DdHashTable *hash);
extern int cuddHashTableInsert1 (DdHashTable *hash, DdNode *f, DdNode *value, ptrint count);
extern DdNode * cuddHashTableLookup1 (DdHashTable *hash, DdNode *f);
extern int cuddLinearAndSifting (DdManager *table, int lower, int upper);
extern int cuddLinearInPlace (DdManager * table, int  x, int  y);
extern void cuddUpdateInteractionMatrix (DdManager * table, int  xindex, int  yindex);
extern int cuddInitLinear (DdManager *table);
extern int cuddResizeLinear (DdManager *table);
extern void cuddReclaim (DdManager *table, DdNode *n);
extern void cuddReclaimZdd (DdManager *table, DdNode *n);
extern void cuddClearDeathRow (DdManager *table);
extern void cuddShrinkDeathRow (DdManager *table);
extern DdNode * cuddDynamicAllocNode (DdManager *table);
extern int cuddSifting (DdManager *table, int lower, int upper);
extern int cuddSwapping (DdManager *table, int lower, int upper, Cudd_ReorderingType heuristic);
extern int cuddNextHigh (DdManager *table, int x);
extern int cuddNextLow (DdManager *table, int x);
extern int cuddSwapInPlace (DdManager *table, int x, int y);
extern int cuddBddAlignToZdd (DdManager *table);
extern DdNode * cuddBddMakePrime (DdManager *dd, DdNode *cube, DdNode *f);
#ifdef ST_INCLUDED
#endif
extern int cuddSymmCheck (DdManager *table, int x, int y);
extern int cuddSymmSifting (DdManager *table, int lower, int upper);
extern int cuddSymmSiftingConv (DdManager *table, int lower, int upper);
extern DdNode * cuddAllocNode (DdManager *unique);
extern DdManager * cuddInitTable (unsigned int numVars, unsigned int numVarsZ, unsigned int numSlots, unsigned int looseUpTo);
extern void cuddFreeTable (DdManager *unique);
extern int cuddGarbageCollect (DdManager *unique, int clearCache);
extern DdNode * cuddZddGetNode (DdManager *zdd, int id, DdNode *T, DdNode *E);
extern DdNode * cuddZddGetNodeIVO (DdManager *dd, int index, DdNode *g, DdNode *h);
extern DdNode * cuddUniqueInter (DdManager *unique, int index, DdNode *T, DdNode *E);
extern DdNode * cuddUniqueInterIVO (DdManager *unique, int index, DdNode *T, DdNode *E);
extern DdNode * cuddUniqueInterZdd (DdManager *unique, int index, DdNode *T, DdNode *E);
extern DdNode * cuddUniqueConst (DdManager *unique, CUDD_VALUE_TYPE value);
extern void cuddRehash (DdManager *unique, int i);
extern int cuddResizeTableZdd (DdManager *unique, int index);
extern void cuddSlowTableGrowth (DdManager *unique);
#ifdef ST_INCLUDED
extern int cuddCollectNodes (DdNode *f, st_table *visited);
#endif
extern DdNodePtr * cuddNodeArray (DdNode *f, int *n);
extern int cuddWindowReorder (DdManager *table, int low, int high, Cudd_ReorderingType submethod);
extern DdNode	* cuddZddProduct (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode	* cuddZddUnateProduct (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode	* cuddZddWeakDiv (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode	* cuddZddWeakDivF (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode	* cuddZddDivide (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode	* cuddZddDivideF (DdManager *dd, DdNode *f, DdNode *g);
extern int cuddZddGetCofactors3 (DdManager *dd, DdNode *f, int v, DdNode **f1, DdNode **f0, DdNode **fd);
extern int cuddZddGetCofactors2 (DdManager *dd, DdNode *f, int v, DdNode **f1, DdNode **f0);
extern DdNode	* cuddZddComplement (DdManager *dd, DdNode *node);
extern int cuddZddGetPosVarIndex(DdManager * dd, int index);
extern int cuddZddGetNegVarIndex(DdManager * dd, int index);
extern int cuddZddGetPosVarLevel(DdManager * dd, int index);
extern int cuddZddGetNegVarLevel(DdManager * dd, int index);
extern int cuddZddTreeSifting (DdManager *table, Cudd_ReorderingType method);
extern DdNode	* cuddZddIsop (DdManager *dd, DdNode *L, DdNode *U, DdNode **zdd_I);
extern DdNode	* cuddBddIsop (DdManager *dd, DdNode *L, DdNode *U);
extern DdNode	* cuddMakeBddFromZddCover (DdManager *dd, DdNode *node);
extern int cuddZddLinearSifting (DdManager *table, int lower, int upper);
extern int cuddZddAlignToBdd (DdManager *table);
extern int cuddZddNextHigh (DdManager *table, int x);
extern int cuddZddNextLow (DdManager *table, int x);
extern int cuddZddUniqueCompare (int *ptr_x, int *ptr_y);
extern int cuddZddSwapInPlace (DdManager *table, int x, int y);
extern int cuddZddSwapping (DdManager *table, int lower, int upper, Cudd_ReorderingType heuristic);
extern int cuddZddSifting (DdManager *table, int lower, int upper);
extern DdNode * cuddZddIte (DdManager *dd, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * cuddZddUnion (DdManager *zdd, DdNode *P, DdNode *Q);
extern DdNode * cuddZddIntersect (DdManager *zdd, DdNode *P, DdNode *Q);
extern DdNode * cuddZddDiff (DdManager *zdd, DdNode *P, DdNode *Q);
extern DdNode * cuddZddChangeAux (DdManager *zdd, DdNode *P, DdNode *zvar);
extern DdNode * cuddZddSubset1 (DdManager *dd, DdNode *P, int var);
extern DdNode * cuddZddSubset0 (DdManager *dd, DdNode *P, int var);
extern int cuddZddSymmCheck (DdManager *table, int x, int y);
extern int cuddZddSymmSifting (DdManager *table, int lower, int upper);
extern int cuddZddSymmSiftingConv (DdManager *table, int lower, int upper);
extern int cuddZddP (DdManager *zdd, DdNode *f);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* _CUDDINT */
