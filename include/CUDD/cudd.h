/**CHeaderFile*****************************************************************

  FileName    [cudd.h]

  PackageName [cudd]

  Synopsis    [The University of Colorado decision diagram package.]

  Description [External functions and data strucures of the CUDD package.
  <ul>
  <li> To turn on the gathering of statistics, define DD_STATS.
  <li> To link with mis, define DD_MIS.
  </ul>
  Modified by Abelardo Pardo to interface it to VIS.
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

  Revision    [$Id: cudd.h,v 1.180 2012/02/05 01:07:18 fabio Exp $]

******************************************************************************/

#ifndef _CUDD
#define _CUDD

/*---------------------------------------------------------------------------*/
/* Nested includes                                                           */
/*---------------------------------------------------------------------------*/

#include "mtr.h"
#include "epd.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef SIZEOF_VOID_P
#define SIZEOF_VOID_P 4
#endif
#ifndef SIZEOF_INT
#define SIZEOF_INT 4
#endif
#ifndef SIZEOF_LONG
#define SIZEOF_LONG 4
#endif

#define CUDD_TRUE 1

#define CUDD_VALUE_TYPE		double
#define CUDD_OUT_OF_MEM		-1
/* The sizes of the subtables and the cache must be powers of two. */
#define CUDD_UNIQUE_SLOTS	256	/* initial size of subtables */
#define CUDD_CACHE_SLOTS	262144	/* default size of the cache */

/* Constants for residue functions. */

/* CUDD_MAXINDEX is defined in such a way that on 32-bit and 64-bit
** machines one can cast an index to (int) without generating a negative
** number.
*/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define CUDD_MAXINDEX		(((DdHalfWord) ~0) >> 1)
#else
#define CUDD_MAXINDEX		((DdHalfWord) ~0)
#endif

/* CUDD_CONST_INDEX is the index of constant nodes.  Currently this
** is a synonim for CUDD_MAXINDEX. */
#define CUDD_CONST_INDEX	CUDD_MAXINDEX

/* These constants define the digits used in the representation of
** arbitrary precision integers.  The configurations tested use 8, 16,
** and 32 bits for each digit.  The typedefs should be in agreement
** with these definitions.
*/
#if SIZEOF_LONG == 8
#define DD_APA_BITS	32
#define DD_APA_BASE	(1L << DD_APA_BITS)
#define DD_APA_HEXPRINT	"%08x"
#else
#define DD_APA_BITS	16
#define DD_APA_BASE	(1 << DD_APA_BITS)
#define DD_APA_HEXPRINT	"%04x"
#endif
#define DD_APA_MASK	(DD_APA_BASE - 1)

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/**Enum************************************************************************

  Synopsis    [Type of reordering algorithm.]

  Description [Type of reordering algorithm.]

******************************************************************************/
typedef enum
{
    CUDD_REORDER_SAME,
    CUDD_REORDER_NONE,
    CUDD_REORDER_RANDOM,
    CUDD_REORDER_RANDOM_PIVOT,
    CUDD_REORDER_SIFT,
    CUDD_REORDER_SIFT_CONVERGE,
    CUDD_REORDER_SYMM_SIFT,
    CUDD_REORDER_SYMM_SIFT_CONV,
    CUDD_REORDER_WINDOW2,
    CUDD_REORDER_WINDOW3,
    CUDD_REORDER_WINDOW4,
    CUDD_REORDER_WINDOW2_CONV,
    CUDD_REORDER_WINDOW3_CONV,
    CUDD_REORDER_WINDOW4_CONV,
    CUDD_REORDER_GROUP_SIFT,
    CUDD_REORDER_GROUP_SIFT_CONV,
    CUDD_REORDER_ANNEALING,
    CUDD_REORDER_GENETIC,
    CUDD_REORDER_LINEAR,
    CUDD_REORDER_LINEAR_CONVERGE,
    CUDD_REORDER_LAZY_SIFT,
    CUDD_REORDER_EXACT
} Cudd_ReorderingType;


/**Enum************************************************************************

  Synopsis    [Type of aggregation methods.]

  Description [Type of aggregation methods.]

******************************************************************************/
typedef enum
{
    CUDD_NO_CHECK,
    CUDD_GROUP_CHECK,
    CUDD_GROUP_CHECK2,
    CUDD_GROUP_CHECK3,
    CUDD_GROUP_CHECK4,
    CUDD_GROUP_CHECK5,
    CUDD_GROUP_CHECK6,
    CUDD_GROUP_CHECK7,
    CUDD_GROUP_CHECK8,
    CUDD_GROUP_CHECK9
} Cudd_AggregationType;


/**Enum************************************************************************

  Synopsis    [Type of hooks.]

  Description [Type of hooks.]

******************************************************************************/
typedef enum
{
    CUDD_PRE_GC_HOOK,
    CUDD_POST_GC_HOOK,
    CUDD_PRE_REORDERING_HOOK,
    CUDD_POST_REORDERING_HOOK
} Cudd_HookType;


/**Enum************************************************************************

  Synopsis    [Type of error codes.]

  Description [Type of  error codes.]

******************************************************************************/
typedef enum
{
    CUDD_NO_ERROR,
    CUDD_MEMORY_OUT,
    CUDD_TOO_MANY_NODES,
    CUDD_MAX_MEM_EXCEEDED,
    CUDD_TIMEOUT_EXPIRED,
    CUDD_INVALID_ARG,
    CUDD_INTERNAL_ERROR
} Cudd_ErrorType;


/**Enum************************************************************************

  Synopsis    [Group type for lazy sifting.]

  Description [Group type for lazy sifting.]

******************************************************************************/
typedef enum
{
    CUDD_LAZY_NONE,
    CUDD_LAZY_SOFT_GROUP,
    CUDD_LAZY_HARD_GROUP,
    CUDD_LAZY_UNGROUP
} Cudd_LazyGroupType;


/**Enum************************************************************************

  Synopsis    [Variable type.]

  Description [Variable type. Currently used only in lazy sifting.]

******************************************************************************/
typedef enum
{
    CUDD_VAR_PRIMARY_INPUT,
    CUDD_VAR_PRESENT_STATE,
    CUDD_VAR_NEXT_STATE
} Cudd_VariableType;


#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
typedef unsigned int   DdHalfWord;
#else
typedef unsigned short DdHalfWord;
#endif

typedef struct DdNode DdNode;

typedef struct DdChildren
{
    struct DdNode *T;
    struct DdNode *E;
} DdChildren;

/* The DdNode structure is the only one exported out of the package */
struct DdNode
{
    DdHalfWord index;
    DdHalfWord ref;		/* reference count */
    DdNode *next;		/* next pointer for unique table */
    union
    {
        CUDD_VALUE_TYPE value;	/* for constant nodes */
        DdChildren kids;	/* for internal nodes */
    } type;
};

typedef struct DdManager DdManager;

typedef struct DdGen DdGen;

/* These typedefs for arbitrary precision arithmetic should agree with
** the corresponding constant definitions above. */
#if SIZEOF_LONG == 8
typedef unsigned int DdApaDigit;
typedef unsigned long int DdApaDoubleDigit;
#else
typedef unsigned short int DdApaDigit;
typedef unsigned int DdApaDoubleDigit;
#endif

/* Return type for function computing two-literal clauses. */
typedef struct DdTlcInfo DdTlcInfo;

/* Type of hook function. */
typedef int (*DD_HFP)(DdManager *, const char *, void *);
/* Type of priority function */
/* Type of apply operator. */
/* Type of monadic apply operator. */
/* Types of cache tag functions. */
typedef DdNode * (*DD_CTFP)(DdManager *, DdNode *, DdNode *);
typedef DdNode * (*DD_CTFP1)(DdManager *, DdNode *);
/* Type of memory-out function. */
typedef void (*DD_OOMFP)(long);
/* Type of comparison function for qsort. */
typedef int (*DD_QSFP)(const void *, const void *);

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**Macro***********************************************************************

  Synopsis     [Returns 1 if the node is a constant node.]

  Description  [Returns 1 if the node is a constant node (rather than an
  internal node). All constant nodes have the same index
  (CUDD_CONST_INDEX). The pointer passed to Cudd_IsConstant may be either
  regular or complemented.]

  SideEffects  [none]

  SeeAlso      []

******************************************************************************/
#define Cudd_IsConstant(node) ((Cudd_Regular(node))->index == CUDD_CONST_INDEX)


/**Macro***********************************************************************

  Synopsis     [Complements a DD.]

  Description  [Complements a DD by flipping the complement attribute of
  the pointer (the least significant bit).]

  SideEffects  [none]

  SeeAlso      [Cudd_NotCond]

******************************************************************************/
#define Cudd_Not(node) ((DdNode *)((long)(node) ^ 01))


/**Macro***********************************************************************

  Synopsis     [Complements a DD if a condition is true.]

  Description  [Complements a DD if condition c is true; c should be
  either 0 or 1, because it is used directly (for efficiency). If in
  doubt on the values c may take, use "(c) ? Cudd_Not(node) : node".]

  SideEffects  [none]

  SeeAlso      [Cudd_Not]

******************************************************************************/
#define Cudd_NotCond(node,c) ((DdNode *)((long)(node) ^ (c)))


/**Macro***********************************************************************

  Synopsis     [Returns the regular version of a pointer.]

  Description  []

  SideEffects  [none]

  SeeAlso      [Cudd_Complement Cudd_IsComplement]

******************************************************************************/
#define Cudd_Regular(node) ((DdNode *)((unsigned long)(node) & ~01))


/**Macro***********************************************************************

  Synopsis     [Returns the complemented version of a pointer.]

  Description  []

  SideEffects  [none]

  SeeAlso      [Cudd_Regular Cudd_IsComplement]

******************************************************************************/
#define Cudd_Complement(node) ((DdNode *)((unsigned long)(node) | 01))


/**Macro***********************************************************************

  Synopsis     [Returns 1 if a pointer is complemented.]

  Description  []

  SideEffects  [none]

  SeeAlso      [Cudd_Regular Cudd_Complement]

******************************************************************************/
#define Cudd_IsComplement(node)	((int) ((long) (node) & 01))


/**Macro***********************************************************************

  Synopsis     [Returns the then child of an internal node.]

  Description  [Returns the then child of an internal node. If
  <code>node</code> is a constant node, the result is unpredictable.]

  SideEffects  [none]

  SeeAlso      [Cudd_E Cudd_V]

******************************************************************************/
#define Cudd_T(node) ((Cudd_Regular(node))->type.kids.T)


/**Macro***********************************************************************

  Synopsis     [Returns the else child of an internal node.]

  Description  [Returns the else child of an internal node. If
  <code>node</code> is a constant node, the result is unpredictable.]

  SideEffects  [none]

  SeeAlso      [Cudd_T Cudd_V]

******************************************************************************/
#define Cudd_E(node) ((Cudd_Regular(node))->type.kids.E)


/**Macro***********************************************************************

  Synopsis     [Returns the value of a constant node.]

  Description  [Returns the value of a constant node. If
  <code>node</code> is an internal node, the result is unpredictable.]

  SideEffects  [none]

  SeeAlso      [Cudd_T Cudd_E]

******************************************************************************/


/**Macro***********************************************************************

  Synopsis     [Returns the current position in the order of variable
  index.]

  Description [Returns the current position in the order of variable
  index. This macro is obsolete and is kept for compatibility. New
  applications should use Cudd_ReadPerm instead.]

  SideEffects  [none]

  SeeAlso      [Cudd_ReadPerm]

******************************************************************************/
#define Cudd_ReadIndex(dd,index) (Cudd_ReadPerm(dd,index))


/**Macro***********************************************************************

  Synopsis     [Iterates over the cubes of a decision diagram.]

  Description  [Iterates over the cubes of a decision diagram f.
  <ul>
  <li> DdManager *manager;
  <li> DdNode *f;
  <li> DdGen *gen;
  <li> int *cube;
  <li> CUDD_VALUE_TYPE value;
  </ul>
  Cudd_ForeachCube allocates and frees the generator. Therefore the
  application should not try to do that. Also, the cube is freed at the
  end of Cudd_ForeachCube and hence is not available outside of the loop.<p>
  CAUTION: It is assumed that dynamic reordering will not occur while
  there are open generators. It is the user's responsibility to make sure
  that dynamic reordering does not occur. As long as new nodes are not created
  during generation, and dynamic reordering is not called explicitly,
  dynamic reordering will not occur. Alternatively, it is sufficient to
  disable dynamic reordering. It is a mistake to dispose of a diagram
  on which generation is ongoing.]

  SideEffects  [none]

  SeeAlso      [Cudd_ForeachNode Cudd_FirstCube Cudd_NextCube Cudd_GenFree
  Cudd_IsGenEmpty Cudd_AutodynDisable]

******************************************************************************/
#define Cudd_ForeachCube(manager, f, gen, cube, value)\
    for((gen) = Cudd_FirstCube(manager, f, &cube, &value);\
	Cudd_IsGenEmpty(gen) ? Cudd_GenFree(gen) : CUDD_TRUE;\
	(void) Cudd_NextCube(gen, &cube, &value))


/**Macro***********************************************************************

  Synopsis     [Iterates over the primes of a Boolean function.]

  Description  [Iterates over the primes of a Boolean function producing
  a prime and irredundant cover.
  <ul>
  <li> DdManager *manager;
  <li> DdNode *l;
  <li> DdNode *u;
  <li> DdGen *gen;
  <li> int *cube;
  </ul>
  The Boolean function is described by an upper bound and a lower bound.  If
  the function is completely specified, the two bounds coincide.
  Cudd_ForeachPrime allocates and frees the generator.  Therefore the
  application should not try to do that.  Also, the cube is freed at the
  end of Cudd_ForeachPrime and hence is not available outside of the loop.<p>
  CAUTION: It is a mistake to change a diagram on which generation is ongoing.]

  SideEffects  [none]

  SeeAlso      [Cudd_ForeachCube Cudd_FirstPrime Cudd_NextPrime Cudd_GenFree
  Cudd_IsGenEmpty]

******************************************************************************/
#define Cudd_ForeachPrime(manager, l, u, gen, cube)\
    for((gen) = Cudd_FirstPrime(manager, l, u, &cube);\
	Cudd_IsGenEmpty(gen) ? Cudd_GenFree(gen) : CUDD_TRUE;\
	(void) Cudd_NextPrime(gen, &cube))


/**Macro***********************************************************************

  Synopsis     [Iterates over the nodes of a decision diagram.]

  Description  [Iterates over the nodes of a decision diagram f.
  <ul>
  <li> DdManager *manager;
  <li> DdNode *f;
  <li> DdGen *gen;
  <li> DdNode *node;
  </ul>
  The nodes are returned in a seemingly random order.
  Cudd_ForeachNode allocates and frees the generator. Therefore the
  application should not try to do that.<p>
  CAUTION: It is assumed that dynamic reordering will not occur while
  there are open generators. It is the user's responsibility to make sure
  that dynamic reordering does not occur. As long as new nodes are not created
  during generation, and dynamic reordering is not called explicitly,
  dynamic reordering will not occur. Alternatively, it is sufficient to
  disable dynamic reordering. It is a mistake to dispose of a diagram
  on which generation is ongoing.]

  SideEffects  [none]

  SeeAlso      [Cudd_ForeachCube Cudd_FirstNode Cudd_NextNode Cudd_GenFree
  Cudd_IsGenEmpty Cudd_AutodynDisable]

******************************************************************************/
#define Cudd_ForeachNode(manager, f, gen, node)\
    for((gen) = Cudd_FirstNode(manager, f, &node);\
	Cudd_IsGenEmpty(gen) ? Cudd_GenFree(gen) : CUDD_TRUE;\
	(void) Cudd_NextNode(gen, &node))


/**Macro***********************************************************************

  Synopsis     [Iterates over the paths of a ZDD.]

  Description  [Iterates over the paths of a ZDD f.
  <ul>
  <li> DdManager *manager;
  <li> DdNode *f;
  <li> DdGen *gen;
  <li> int *path;
  </ul>
  Cudd_zddForeachPath allocates and frees the generator. Therefore the
  application should not try to do that. Also, the path is freed at the
  end of Cudd_zddForeachPath and hence is not available outside of the loop.<p>
  CAUTION: It is assumed that dynamic reordering will not occur while
  there are open generators.  It is the user's responsibility to make sure
  that dynamic reordering does not occur.  As long as new nodes are not created
  during generation, and dynamic reordering is not called explicitly,
  dynamic reordering will not occur.  Alternatively, it is sufficient to
  disable dynamic reordering.  It is a mistake to dispose of a diagram
  on which generation is ongoing.]

  SideEffects  [none]

  SeeAlso      [Cudd_zddFirstPath Cudd_zddNextPath Cudd_GenFree
  Cudd_IsGenEmpty Cudd_AutodynDisable]

******************************************************************************/
#define Cudd_zddForeachPath(manager, f, gen, path)\
    for((gen) = Cudd_zddFirstPath(manager, f, &path);\
	Cudd_IsGenEmpty(gen) ? Cudd_GenFree(gen) : CUDD_TRUE;\
	(void) Cudd_zddNextPath(gen, &path))



/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern DdNode * Cudd_bddIthVar (DdManager *dd, int i);
extern DdNode * Cudd_ReadOne (DdManager *dd);
extern DdNode * Cudd_ReadLogicZero (DdManager *dd);
extern void Cudd_SetMinHit (DdManager *dd, unsigned int hr);
extern int Cudd_ReadSize (DdManager *dd);
extern void Cudd_FreeTree (DdManager *dd);
extern void Cudd_FreeZddTree (DdManager *dd);
extern int Cudd_ReadPerm (DdManager *dd, int i);
extern CUDD_VALUE_TYPE Cudd_ReadEpsilon (DdManager *dd);
extern void Cudd_SetEpsilon (DdManager *dd, CUDD_VALUE_TYPE ep);
extern unsigned long Cudd_ReadMemoryInUse (DdManager *dd);
extern long Cudd_ReadNodeCount (DdManager *dd);
extern int Cudd_RemoveHook (DdManager *dd, DD_HFP f, Cudd_HookType where);
extern DdNode * Cudd_addIteConstant (DdManager *dd, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * Cudd_addEvalConst (DdManager *dd, DdNode *f, DdNode *g);
extern int Cudd_addLeq (DdManager * dd, DdNode * f, DdNode * g);
extern DdNode * Cudd_addCmpl (DdManager *dd, DdNode *f);
extern DdNode * Cudd_bddExistAbstract (DdManager *manager, DdNode *f, DdNode *cube);
extern int Cudd_bddVarIsDependent (DdManager *dd, DdNode *f, DdNode *var);
extern DdNode * Cudd_bddIteConstant (DdManager *dd, DdNode *f, DdNode *g, DdNode *h);
extern DdNode * Cudd_bddIntersect (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode * Cudd_bddAnd (DdManager *dd, DdNode *f, DdNode *g);
extern DdNode * Cudd_bddAndLimit (DdManager *dd, DdNode *f, DdNode *g, unsigned int limit);
extern DdNode * Cudd_bddOrLimit (DdManager *dd, DdNode *f, DdNode *g, unsigned int limit);
extern DdNode * Cudd_bddXor (DdManager *dd, DdNode *f, DdNode *g);
extern int Cudd_bddLeq (DdManager *dd, DdNode *f, DdNode *g);
extern int Cudd_DebugCheck (DdManager *table);
extern int Cudd_CheckKeys (DdManager *table);
extern DdNode * Cudd_Cofactor (DdManager *dd, DdNode *f, DdNode *g);
extern int Cudd_CheckCube (DdManager *dd, DdNode *g);
extern DdManager * Cudd_Init (unsigned int numVars, unsigned int numVarsZ, unsigned int numSlots, unsigned int cacheSize, unsigned long maxMemory);
extern void Cudd_Quit (DdManager *unique);
extern int Cudd_PrintLinear (DdManager *table);
extern void Cudd_Ref (DdNode *n);
extern void Cudd_RecursiveDeref (DdManager *table, DdNode *n);
extern void Cudd_IterDerefBdd (DdManager *table, DdNode *n);
extern void Cudd_RecursiveDerefZdd (DdManager *table, DdNode *n);
extern void Cudd_Deref (DdNode *node);
extern int Cudd_ReduceHeap (DdManager *table, Cudd_ReorderingType heuristic, int minsize);
extern DdNode * Cudd_LargestCube (DdManager *manager, DdNode *f, int *length);
extern DdNode * Cudd_Decreasing (DdManager *dd, DdNode *f, int i);
extern int Cudd_EquivDC (DdManager *dd, DdNode *F, DdNode *G, DdNode *D);
extern int Cudd_bddLeqUnless (DdManager *dd, DdNode *f, DdNode *g, DdNode *D);
extern int Cudd_EqualSupNorm (DdManager *dd, DdNode *f, DdNode *g, CUDD_VALUE_TYPE tolerance, int pr);
extern DdNode * Cudd_bddMakePrime (DdManager *dd, DdNode *cube, DdNode *f);
extern double Cudd_CountMinterm (DdManager *manager, DdNode *node, int nvars);
extern DdGen * Cudd_FirstCube (DdManager *dd, DdNode *f, int **cube, CUDD_VALUE_TYPE *value);
extern int Cudd_NextCube (DdGen *gen, int **cube, CUDD_VALUE_TYPE *value);
extern DdGen * Cudd_FirstPrime(DdManager *dd, DdNode *l, DdNode *u, int **cube);
extern int Cudd_NextPrime(DdGen *gen, int **cube);
extern int Cudd_BddToCubeArray (DdManager *dd, DdNode *cube, int *array);
extern DdGen * Cudd_FirstNode (DdManager *dd, DdNode *f, DdNode **node);
extern int Cudd_NextNode (DdGen *gen, DdNode **node);
extern int Cudd_GenFree (DdGen *gen);
extern int Cudd_IsGenEmpty (DdGen *gen);
extern long Cudd_Random (void);
extern void Cudd_Srandom (long seed);
extern void Cudd_OutOfMem (long size);
extern int Cudd_zddReduceHeap (DdManager *table, Cudd_ReorderingType heuristic, int minsize);
extern DdNode * Cudd_zddDiffConst (DdManager *zdd, DdNode *P, DdNode *Q);
extern DdGen * Cudd_zddFirstPath (DdManager *zdd, DdNode *f, int **path);
extern int Cudd_zddNextPath (DdGen *gen, int **path);
extern int Cudd_bddReadPairIndex (DdManager *dd, int index);
extern int Cudd_bddIsVarToBeGrouped (DdManager *dd, int index);
extern int Cudd_bddIsVarToBeUngrouped (DdManager *dd, int index);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* _CUDD */
