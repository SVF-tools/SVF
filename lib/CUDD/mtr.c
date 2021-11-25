/**CFile***********************************************************************

  FileName    [mtrBasic.c]

  PackageName [mtr]

  Synopsis    [Basic manipulation of multiway branching trees.]

  Description [External procedures included in this module:
	    <ul>
	    <li> Mtr_AllocNode()
	    <li> Mtr_DeallocNode()
	    <li> Mtr_InitTree()
	    <li> Mtr_FreeTree()
	    <li> Mtr_CopyTree()
	    <li> Mtr_MakeFirstChild()
	    <li> Mtr_MakeLastChild()
	    <li> Mtr_CreateFirstChild()
	    <li> Mtr_CreateLastChild()
	    <li> Mtr_MakeNextSibling()
	    <li> Mtr_PrintTree()
	    </ul>
	    ]

  SeeAlso     [cudd package]

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
#include "CUDD/mtr.h"


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
static char rcsid[] MTR_UNUSED = "$Id: mtrBasic.c,v 1.15 2012/02/05 01:06:19 fabio Exp $";
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

  Synopsis    [Allocates new tree node.]

  Description [Allocates new tree node. Returns pointer to node.]

  SideEffects [None]

  SeeAlso     [Mtr_DeallocNode]

******************************************************************************/
MtrNode *
Mtr_AllocNode(void)
{
    MtrNode *node;

    node = ALLOC(MtrNode,1);
    return node;

} /* Mtr_AllocNode */


/**Function********************************************************************

  Synopsis    [Deallocates tree node.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_AllocNode]

******************************************************************************/
void
Mtr_DeallocNode(
        MtrNode * node /* node to be deallocated */)
{
    FREE(node);
    return;

} /* end of Mtr_DeallocNode */


/**Function********************************************************************

  Synopsis    [Initializes tree with one node.]

  Description [Initializes tree with one node. Returns pointer to node.]

  SideEffects [None]

  SeeAlso     [Mtr_FreeTree Mtr_InitGroupTree]

******************************************************************************/
MtrNode *
Mtr_InitTree(void)
{
    MtrNode *node;

    node = Mtr_AllocNode();
    if (node == NULL) return(NULL);

    node->parent = node->child = node->elder = node->younger = NULL;
    node->flags = 0;

    return(node);

} /* end of Mtr_InitTree */


/**Function********************************************************************

  Synopsis    [Disposes of tree rooted at node.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_InitTree]

******************************************************************************/
void
Mtr_FreeTree(
        MtrNode * node)
{
    if (node == NULL) return;
    if (! MTR_TEST(node,MTR_TERMINAL)) Mtr_FreeTree(node->child);
    Mtr_FreeTree(node->younger);
    Mtr_DeallocNode(node);
    return;

} /* end of Mtr_FreeTree */


/**Function********************************************************************

  Synopsis    [Makes a copy of tree.]

  Description [Makes a copy of tree. If parameter expansion is greater
  than 1, it will expand the tree by that factor. It is an error for
  expansion to be less than 1. Returns a pointer to the copy if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Mtr_InitTree]

******************************************************************************/
MtrNode *
Mtr_CopyTree(
        MtrNode * node,
        int  expansion)
{
    MtrNode *copy;

    if (node == NULL) return(NULL);
    if (expansion < 1) return(NULL);
    copy = Mtr_AllocNode();
    if (copy == NULL) return(NULL);
    copy->parent = copy->elder = copy->child = copy->younger = NULL;
    if (node->child != NULL) {
        copy->child = Mtr_CopyTree(node->child, expansion);
        if (copy->child == NULL) {
            Mtr_DeallocNode(copy);
            return(NULL);
        }
    }
    if (node->younger != NULL) {
        copy->younger = Mtr_CopyTree(node->younger, expansion);
        if (copy->younger == NULL) {
            Mtr_FreeTree(copy);
            return(NULL);
        }
    }
    copy->flags = node->flags;
    copy->low = node->low * expansion;
    copy->size = node->size * expansion;
    copy->index = node->index * expansion;
    if (copy->younger) copy->younger->elder = copy;
    if (copy->child) {
        MtrNode *auxnode = copy->child;
        while (auxnode != NULL) {
            auxnode->parent = copy;
            auxnode = auxnode->younger;
        }
    }
    return(copy);

} /* end of Mtr_CopyTree */


/**Function********************************************************************

  Synopsis    [Prints a tree, one node per line.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_PrintGroups]

******************************************************************************/
void
Mtr_PrintTree(
        MtrNode * node)
{
    if (node == NULL) return;
    (void) fprintf(stdout,
#if SIZEOF_VOID_P == 8
            "N=0x%-8lx C=0x%-8lx Y=0x%-8lx E=0x%-8lx P=0x%-8lx F=%x L=%u S=%u\n",
    (unsigned long) node, (unsigned long) node->child,
    (unsigned long) node->younger, (unsigned long) node->elder,
    (unsigned long) node->parent, node->flags, node->low, node->size);
#else
                   "N=0x%-8x C=0x%-8x Y=0x%-8x E=0x%-8x P=0x%-8x F=%x L=%hu S=%hu\n",
                   (unsigned) node, (unsigned) node->child,
                   (unsigned) node->younger, (unsigned) node->elder,
                   (unsigned) node->parent, node->flags, node->low, node->size);
#endif
    if (!MTR_TEST(node,MTR_TERMINAL)) Mtr_PrintTree(node->child);
    Mtr_PrintTree(node->younger);
    return;

} /* end of Mtr_PrintTree */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
/**CFile***********************************************************************

  FileName    [mtrGroup.c]

  PackageName [mtr]

  Synopsis    [Functions to support group specification for reordering.]

  Description [External procedures included in this module:
	    <ul>
	    <li> Mtr_InitGroupTree()
	    <li> Mtr_MakeGroup()
	    <li> Mtr_DissolveGroup()
	    <li> Mtr_FindGroup()
	    <li> Mtr_SwapGroups()
            <li> Mtr_ReorderGroups()
	    <li> Mtr_PrintGroups()
	    <li> Mtr_ReadGroups()
	    </ul>
	Static procedures included in this module:
	    <ul>
	    <li> mtrShiftHL
	    </ul>
	    ]

  SeeAlso     [cudd package]

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

//#ifndef lint
//static char rcsid[] MTR_UNUSED = "$Id: mtrGroup.c,v 1.21 2012/02/05 01:06:19 fabio Exp $";
//#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Allocate new tree.]

  Description [Allocate new tree with one node, whose low and size
  fields are specified by the lower and size parameters.
  Returns pointer to tree root.]

  SideEffects [None]

  SeeAlso     [Mtr_InitTree Mtr_FreeTree]

******************************************************************************/
MtrNode *
Mtr_InitGroupTree(
        int  lower,
        int  size)
{
    MtrNode *root;

    root = Mtr_InitTree();
    if (root == NULL) return(NULL);
    root->flags = MTR_DEFAULT;
    root->low = lower;
    root->size = size;
    return(root);

} /* end of Mtr_InitGroupTree */


/**Function********************************************************************

  Synopsis    [Makes a new group with size leaves starting at low.]

  Description [Makes a new group with size leaves starting at low.
  If the new group intersects an existing group, it must
  either contain it or be contained by it.  This procedure relies on
  the low and size fields of each node. It also assumes that the
  children of each node are sorted in order of increasing low.  In
  case of a valid request, the flags of the new group are set to the
  value passed in `flags.'  Returns the pointer to the root of the new
  group upon successful termination; NULL otherwise. If the group
  already exists, the pointer to its root is returned.]

  SideEffects [None]

  SeeAlso     [Mtr_DissolveGroup Mtr_ReadGroups Mtr_FindGroup]

******************************************************************************/
MtrNode *
Mtr_MakeGroup(
        MtrNode * root /* root of the group tree */,
        unsigned int  low /* lower bound of the group */,
        unsigned int  size /* size of the group */,
        unsigned int  flags /* flags for the new group */)
{
    MtrNode *node,
            *first,
            *last,
            *previous,
            *newn;

    /* Sanity check. */
    if (size == 0)
        return(NULL);

    /* Check whether current group includes new group.  This check is
    ** necessary at the top-level call.  In the subsequent calls it is
    ** redundant. */
    if (low < (unsigned int) root->low ||
        low + size > (unsigned int) (root->low + root->size))
        return(NULL);

    /* At this point we know that the new group is contained
    ** in the group of root. We have two possible cases here:
    **  - root is a terminal node;
    **  - root has children.       */

    /* Root has no children: create a new group. */
    if (root->child == NULL) {
        newn = Mtr_AllocNode();
        if (newn == NULL) return(NULL);	/* out of memory */
        newn->low = low;
        newn->size = size;
        newn->flags = flags;
        newn->parent = root;
        newn->elder = newn->younger = newn->child = NULL;
        root->child = newn;
        return(newn);
    }

    /* Root has children: Find all children of root that are included
    ** in the new group.  If the group of any child entirely contains
    ** the new group, call Mtr_MakeGroup recursively. */
    previous = NULL;
    first = root->child; /* guaranteed to be non-NULL */
    while (first != NULL && low >= (unsigned int) (first->low + first->size)) {
        previous = first;
        first = first->younger;
    }
    if (first == NULL) {
        /* We have scanned the entire list and we need to append a new
        ** child at the end of it.  Previous points to the last child
        ** of root. */
        newn = Mtr_AllocNode();
        if (newn == NULL) return(NULL);	/* out of memory */
        newn->low = low;
        newn->size = size;
        newn->flags = flags;
        newn->parent = root;
        newn->elder = previous;
        previous->younger = newn;
        newn->younger = newn->child = NULL;
        return(newn);
    }
    /* Here first is non-NULL and low < first->low + first->size. */
    if (low >= (unsigned int) first->low &&
        low + size <= (unsigned int) (first->low + first->size)) {
        /* The new group is contained in the group of first. */
        newn = Mtr_MakeGroup(first, low, size, flags);
        return(newn);
    } else if (low + size <= first->low) {
        /* The new group is entirely contained in the gap between
        ** previous and first. */
        newn = Mtr_AllocNode();
        if (newn == NULL) return(NULL);	/* out of memory */
        newn->low = low;
        newn->size = size;
        newn->flags = flags;
        newn->child = NULL;
        newn->parent = root;
        newn->elder = previous;
        newn->younger = first;
        first->elder = newn;
        if (previous != NULL) {
            previous->younger = newn;
        } else {
            root->child = newn;
        }
        return(newn);
    } else if (low < (unsigned int) first->low &&
               low + size < (unsigned int) (first->low + first->size)) {
        /* Trying to cut an existing group: not allowed. */
        return(NULL);
    } else if (low > first->low) {
        /* The new group neither is contained in the group of first
        ** (this was tested above) nor contains it. It is therefore
        ** trying to cut an existing group: not allowed. */
        return(NULL);
    }

    /* First holds the pointer to the first child contained in the new
    ** group. Here low <= first->low and low + size >= first->low +
    ** first->size.  One of the two inequalities is strict. */
    last = first->younger;
    while (last != NULL &&
           (unsigned int) (last->low + last->size) < low + size) {
        last = last->younger;
    }
    if (last == NULL) {
        /* All the chilren of root from first onward become children
        ** of the new group. */
        newn = Mtr_AllocNode();
        if (newn == NULL) return(NULL);	/* out of memory */
        newn->low = low;
        newn->size = size;
        newn->flags = flags;
        newn->child = first;
        newn->parent = root;
        newn->elder = previous;
        newn->younger = NULL;
        first->elder = NULL;
        if (previous != NULL) {
            previous->younger = newn;
        } else {
            root->child = newn;
        }
        last = first;
        while (last != NULL) {
            last->parent = newn;
            last = last->younger;
        }
        return(newn);
    }

    /* Here last != NULL and low + size <= last->low + last->size. */
    if (low + size - 1 >= (unsigned int) last->low &&
        low + size < (unsigned int) (last->low + last->size)) {
        /* Trying to cut an existing group: not allowed. */
        return(NULL);
    }

    /* First and last point to the first and last of the children of
    ** root that are included in the new group. Allocate a new node
    ** and make all children of root between first and last chidren of
    ** the new node.  Previous points to the child of root immediately
    ** preceeding first. If it is NULL, then first is the first child
    ** of root. */
    newn = Mtr_AllocNode();
    if (newn == NULL) return(NULL);	/* out of memory */
    newn->low = low;
    newn->size = size;
    newn->flags = flags;
    newn->child = first;
    newn->parent = root;
    if (previous == NULL) {
        root->child = newn;
    } else {
        previous->younger = newn;
    }
    newn->elder = previous;
    newn->younger = last->younger;
    if (last->younger != NULL) {
        last->younger->elder = newn;
    }
    last->younger = NULL;
    first->elder = NULL;
    for (node = first; node != NULL; node = node->younger) {
        node->parent = newn;
    }

    return(newn);

} /* end of Mtr_MakeGroup */


/**Function********************************************************************

  Synopsis [Finds a group with size leaves starting at low, if it exists.]

  Description [Finds a group with size leaves starting at low, if it
  exists.  This procedure relies on the low and size fields of each
  node. It also assumes that the children of each node are sorted in
  order of increasing low.  Returns the pointer to the root of the
  group upon successful termination; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
MtrNode *
Mtr_FindGroup(
        MtrNode * root /* root of the group tree */,
        unsigned int  low /* lower bound of the group */,
        unsigned int  size /* upper bound of the group */)
{
    MtrNode *node;

#ifdef MTR_DEBUG
    /* We cannot have a non-empty proper subgroup of a singleton set. */
    assert(!MTR_TEST(root,MTR_TERMINAL));
#endif

    /* Sanity check. */
    if (size < 1) return(NULL);

    /* Check whether current group includes the group sought.  This
    ** check is necessary at the top-level call.  In the subsequent
    ** calls it is redundant. */
    if (low < (unsigned int) root->low ||
        low + size > (unsigned int) (root->low + root->size))
        return(NULL);

    if (root->size == size && root->low == low)
        return(root);

    if (root->child == NULL)
        return(NULL);

    /* Find all chidren of root that are included in the new group. If
    ** the group of any child entirely contains the new group, call
    ** Mtr_MakeGroup recursively.  */
    node = root->child;
    while (low >= (unsigned int) (node->low + node->size)) {
        node = node->younger;
    }
    if (low + size <= (unsigned int) (node->low + node->size)) {
        /* The group is contained in the group of node. */
        node = Mtr_FindGroup(node, low, size);
        return(node);
    } else {
        return(NULL);
    }

} /* end of Mtr_FindGroup */



/**Function********************************************************************

  Synopsis    [Fix variable tree at the end of tree sifting.]

  Description [Fix the levels in the variable tree sorting siblings
  according to them.  It should be called on a non-NULL tree.  It then
  maintains this invariant.  It applies insertion sorting to the list of
  siblings  The order is determined by permutation, which is used to find
  the new level of the node index.  Index must refer to the first variable
  in the group.]

  SideEffects [The tree is modified.]

  SeeAlso     []

******************************************************************************/
void
Mtr_ReorderGroups(
        MtrNode *treenode,
        int *permutation)
{
    MtrNode *auxnode;
    /* Initialize sorted list to first element. */
    MtrNode *sorted = treenode;
    sorted->low = permutation[sorted->index];
    if (sorted->child != NULL)
        Mtr_ReorderGroups(sorted->child, permutation);

    auxnode = treenode->younger;
    while (auxnode != NULL) {
        MtrNode *rightplace;
        MtrNode *moving = auxnode;
        auxnode->low = permutation[auxnode->index];
        if (auxnode->child != NULL)
            Mtr_ReorderGroups(auxnode->child, permutation);
        rightplace = auxnode->elder;
        /* Find insertion point. */
        while (rightplace != NULL && auxnode->low < rightplace->low)
            rightplace = rightplace->elder;
        auxnode = auxnode->younger;
        if (auxnode != NULL) {
            auxnode->elder = moving->elder;
            auxnode->elder->younger = auxnode;
        } else {
            moving->elder->younger = NULL;
        }
        if (rightplace == NULL) { /* Move to head of sorted list. */
            sorted->elder = moving;
            moving->elder = NULL;
            moving->younger = sorted;
            sorted = moving;
        } else { /* Splice. */
            moving->elder = rightplace;
            moving->younger = rightplace->younger;
            if (rightplace->younger != NULL)
                rightplace->younger->elder = moving;
            rightplace->younger = moving;
        }
    }
    /* Fix parent. */
    if (sorted->parent != NULL)
        sorted->parent->child = sorted;

} /* end of Mtr_ReorderGroups */


/**Function********************************************************************

  Synopsis    [Prints the groups as a parenthesized list.]

  Description [Prints the groups as a parenthesized list. After each
  group, the group's flag are printed, preceded by a `|'.  For each
  flag (except MTR_TERMINAL) a character is printed.
  <ul>
  <li>F: MTR_FIXED
  <li>N: MTR_NEWNODE
  <li>S: MTR_SOFT
  </ul>
  The second argument, silent, if different from 0, causes
  Mtr_PrintGroups to only check the syntax of the group tree.
  ]

  SideEffects [None]

  SeeAlso     [Mtr_PrintTree]

******************************************************************************/
void
Mtr_PrintGroups(
        MtrNode * root /* root of the group tree */,
        int  silent /* flag to check tree syntax only */)
{
    MtrNode *node;

    assert(root != NULL);
    assert(root->younger == NULL || root->younger->elder == root);
    assert(root->elder == NULL || root->elder->younger == root);
#if SIZEOF_VOID_P == 8
    if (!silent) (void) printf("(%u",root->low);
#else
    if (!silent) (void) printf("(%hu",root->low);
#endif
    if (MTR_TEST(root,MTR_TERMINAL) || root->child == NULL) {
        if (!silent) (void) printf(",");
    } else {
        node = root->child;
        while (node != NULL) {
            assert(node->low >= root->low && (int) (node->low + node->size) <= (int) (root->low + root->size));
            assert(node->parent == root);
            Mtr_PrintGroups(node,silent);
            node = node->younger;
        }
    }
    if (!silent) {
#if SIZEOF_VOID_P == 8
        (void) printf("%u", root->low + root->size - 1);
#else
        (void) printf("%hu", root->low + root->size - 1);
#endif
        if (root->flags != MTR_DEFAULT) {
            (void) printf("|");
            if (MTR_TEST(root,MTR_FIXED)) (void) printf("F");
            if (MTR_TEST(root,MTR_NEWNODE)) (void) printf("N");
            if (MTR_TEST(root,MTR_SOFT)) (void) printf("S");
        }
        (void) printf(")");
        if (root->parent == NULL) (void) printf("\n");
    }
    assert((root->flags &~(MTR_TERMINAL | MTR_SOFT | MTR_FIXED | MTR_NEWNODE)) == 0);
    return;

} /* end of Mtr_PrintGroups */


/**Function********************************************************************

  Synopsis    [Prints the variable order  as a parenthesized list.]

  Description [Prints the variable order as a parenthesized list. After each
  group, the group's flag are printed, preceded by a `|'.  For each
  flag (except MTR_TERMINAL) a character is printed.
  <ul>
  <li>F: MTR_FIXED
  <li>N: MTR_NEWNODE
  <li>S: MTR_SOFT
  </ul>
  The second argument, gives the map from levels to variable indices.
  ]

  SideEffects [None]

  SeeAlso     [Mtr_PrintGroups]

******************************************************************************/
int
Mtr_PrintGroupedOrder(
        MtrNode * root /* root of the group tree */,
        int *invperm /* map from levels to indices */,
        FILE *fp /* output file */)
{
    MtrNode *child;
    MtrHalfWord level;
    int retval;

    assert(root != NULL);
    assert(root->younger == NULL || root->younger->elder == root);
    assert(root->elder == NULL || root->elder->younger == root);
    retval = fprintf(fp,"(");
    if (retval == EOF) return(0);
    level = root->low;
    child = root->child;
    while (child != NULL) {
        assert(child->low >= root->low && (child->low + child->size) <= (root->low + root->size));
        assert(child->parent == root);
        while (level < child->low) {
            retval = fprintf(fp,"%d%s", invperm[level], (level < root->low + root->size - 1) ? "," : "");
            if (retval == EOF) return(0);
            level++;
        }
        retval = Mtr_PrintGroupedOrder(child,invperm,fp);
        if (retval == 0) return(0);
        level += child->size;
        if (level < root->low + root->size - 1) {
            retval = fprintf(fp,",");
            if (retval == EOF) return(0);
        }
        child = child->younger;
    }
    while (level < root->low + root->size) {
        retval = fprintf(fp,"%d%s", invperm[level], (level < root->low + root->size - 1) ? "," : "");
        if (retval == EOF) return(0);
        level++;
    }
    if (root->flags != MTR_DEFAULT) {
        retval = fprintf(fp,"|");
        if (retval == EOF) return(0);
        if (MTR_TEST(root,MTR_FIXED)) {
            retval = fprintf(fp,"F");
            if (retval == EOF) return(0);
        }
        if (MTR_TEST(root,MTR_NEWNODE)) {
            retval = fprintf(fp,"N");
            if (retval == EOF) return(0);
        }
        if (MTR_TEST(root,MTR_SOFT)) {
            retval = fprintf(fp,"S");
            if (retval == EOF) return(0);
        }
    }
    retval = fprintf(fp,")");
    if (retval == EOF) return(0);
    if (root->parent == NULL) {
        retval = fprintf(fp,"\n");
        if (retval == EOF) return(0);
    }
    assert((root->flags &~(MTR_SOFT | MTR_FIXED | MTR_NEWNODE)) == 0);
    return(1);

} /* end of Mtr_PrintGroupedOrder */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

