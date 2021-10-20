/**CHeaderFile*****************************************************************

  FileName    [mtr.h]

  PackageName [mtr]

  Synopsis    [Multiway-branch tree manipulation]

  Description [This package provides two layers of functions. Functions
  of the lower level manipulate multiway-branch trees, implemented
  according to the classical scheme whereby each node points to its
  first child and its previous and next siblings. These functions are
  collected in mtrBasic.c.<p>
  Functions of the upper layer deal with group trees, that is the trees
  used by group sifting to represent the grouping of variables. These
  functions are collected in mtrGroup.c.]

  SeeAlso     [The CUDD package documentation; specifically on group
  sifting.]

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

  Revision    [$Id: mtr.h,v 1.17 2012/02/05 01:06:19 fabio Exp $]

******************************************************************************/

#ifndef __MTR
#define __MTR

/*---------------------------------------------------------------------------*/
/* Nested includes                                                           */
/*---------------------------------------------------------------------------*/

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

#if defined(__GNUC__)
#define MTR_INLINE __inline__
# if (__GNUC__ >2 || __GNUC_MINOR__ >=7)
#   define MTR_UNUSED __attribute__ ((unused))
# else
#   define MTR_UNUSED
# endif
#else
#define MTR_INLINE
#define MTR_UNUSED
#endif

/* Flag definitions */
#define MTR_DEFAULT	0x00000000
#define MTR_TERMINAL	0x00000001
#define MTR_SOFT	0x00000002
#define MTR_FIXED	0x00000004
#define MTR_NEWNODE	0x00000008

/* MTR_MAXHIGH is defined in such a way that on 32-bit and 64-bit
** machines one can cast a value to (int) without generating a negative
** number.
*/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define MTR_MAXHIGH	(((MtrHalfWord) ~0) >> 1)
#else
#define MTR_MAXHIGH	((MtrHalfWord) ~0)
#endif


/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
typedef unsigned int   MtrHalfWord;
#else
typedef unsigned short MtrHalfWord;
#endif

typedef struct MtrNode {
    MtrHalfWord flags;
    MtrHalfWord low;
    MtrHalfWord size;
    MtrHalfWord index;
    struct MtrNode *parent;
    struct MtrNode *child;
    struct MtrNode *elder;
    struct MtrNode *younger;
} MtrNode;


/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/* Flag manipulation macros */
#define MTR_SET(node, flag)	(node->flags |= (flag))
#define MTR_RESET(node, flag)	(node->flags &= ~ (flag))
#define MTR_TEST(node, flag)	(node->flags & (flag))


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern MtrNode * Mtr_AllocNode (void);
extern void Mtr_DeallocNode (MtrNode *node);
extern MtrNode * Mtr_InitTree (void);
extern void Mtr_FreeTree (MtrNode *node);
extern MtrNode * Mtr_CopyTree (MtrNode *node, int expansion);
extern void Mtr_PrintTree (MtrNode *node);
extern MtrNode * Mtr_InitGroupTree (int lower, int size);
extern MtrNode * Mtr_MakeGroup (MtrNode *root, unsigned int low, unsigned int high, unsigned int flags);
extern MtrNode * Mtr_FindGroup (MtrNode *root, unsigned int low, unsigned int high);
extern void Mtr_ReorderGroups(MtrNode *treenode, int *permutation);
  
extern void Mtr_PrintGroups (MtrNode *root, int silent);
  extern int Mtr_PrintGroupedOrder(MtrNode * root, int *invperm, FILE *fp);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __MTR */
