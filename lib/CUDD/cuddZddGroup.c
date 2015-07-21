/**CFile***********************************************************************

  FileName    [cuddZddGroup.c]

  PackageName [cudd]

  Synopsis    [Functions for ZDD group sifting.]

  Description [External procedures included in this file:
		<ul>
		<li> Cudd_MakeZddTreeNode()
		</ul>
	Internal procedures included in this file:
		<ul>
		<li> cuddZddTreeSifting()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> zddTreeSiftingAux()
		<li> zddCountInternalMtrNodes()
		<li> zddReorderChildren()
		<li> zddFindNodeHiLo()
		<li> zddUniqueCompareGroup()
		<li> zddGroupSifting()
		<li> zddGroupSiftingAux()
		<li> zddGroupSiftingUp()
		<li> zddGroupSiftingDown()
		<li> zddGroupMove()
		<li> zddGroupMoveBackward()
		<li> zddGroupSiftingBackward()
		<li> zddMergeGroups()
		</ul>]

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddGroup.c,v 1.22 2012/02/05 01:07:19 fabio Exp $";
#endif

static	int	*entry;
extern	int	zddTotalNumberSwapping;
#ifdef DD_STATS
static  int     extsymmcalls;
static  int     extsymm;
static  int     secdiffcalls;
static  int     secdiff;
static  int     secdiffmisfire;
#endif
#ifdef DD_DEBUG
static	int	pr = 0;	/* flag to enable printing while debugging */
			/* by depositing a 1 into it */
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int zddTreeSiftingAux (DdManager *table, MtrNode *treenode, Cudd_ReorderingType method);
#ifdef DD_STATS
static int zddCountInternalMtrNodes (DdManager *table, MtrNode *treenode);
#endif
static int zddReorderChildren (DdManager *table, MtrNode *treenode, Cudd_ReorderingType method);
static void zddFindNodeHiLo (DdManager *table, MtrNode *treenode, int *lower, int *upper);
static int zddUniqueCompareGroup (int *ptrX, int *ptrY);
static int zddGroupSifting (DdManager *table, int lower, int upper);
static int zddGroupSiftingAux (DdManager *table, int x, int xLow, int xHigh);
static int zddGroupSiftingUp (DdManager *table, int y, int xLow, Move **moves);
static int zddGroupSiftingDown (DdManager *table, int x, int xHigh, Move **moves);
static int zddGroupMove (DdManager *table, int x, int y, Move **moves);
static int zddGroupMoveBackward (DdManager *table, int x, int y);
static int zddGroupSiftingBackward (DdManager *table, Move *moves, int size);
static void zddMergeGroups (DdManager *table, MtrNode *treenode, int low, int high);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Creates a new ZDD variable group.]

  Description [Creates a new ZDD variable group. The group starts at
  variable and contains size variables. The parameter low is the index
  of the first variable. If the variable already exists, its current
  position in the order is known to the manager. If the variable does
  not exist yet, the position is assumed to be the same as the index.
  The group tree is created if it does not exist yet.
  Returns a pointer to the group if successful; NULL otherwise.]

  SideEffects [The ZDD variable tree is changed.]

  SeeAlso     [Cudd_MakeTreeNode]

******************************************************************************/
MtrNode *
Cudd_MakeZddTreeNode(
  DdManager * dd /* manager */,
  unsigned int  low /* index of the first group variable */,
  unsigned int  size /* number of variables in the group */,
  unsigned int  type /* MTR_DEFAULT or MTR_FIXED */)
{
    MtrNode *group;
    MtrNode *tree;
    unsigned int level;

    /* If the variable does not exist yet, the position is assumed to be
    ** the same as the index. Therefore, applications that rely on
    ** Cudd_bddNewVarAtLevel or Cudd_addNewVarAtLevel to create new
    ** variables have to create the variables before they group them.
    */
    level = (low < (unsigned int) dd->sizeZ) ? dd->permZ[low] : low;

    if (level + size - 1> (int) MTR_MAXHIGH)
	return(NULL);

    /* If the tree does not exist yet, create it. */
    tree = dd->treeZ;
    if (tree == NULL) {
	dd->treeZ = tree = Mtr_InitGroupTree(0, dd->sizeZ);
	if (tree == NULL)
	    return(NULL);
	tree->index = dd->invpermZ[0];
    }

    /* Extend the upper bound of the tree if necessary. This allows the
    ** application to create groups even before the variables are created.
    */
    tree->size = ddMax(tree->size, level + size);

    /* Create the group. */
    group = Mtr_MakeGroup(tree, level, size, type);
    if (group == NULL)
	return(NULL);

    /* Initialize the index field to the index of the variable currently
    ** in position low. This field will be updated by the reordering
    ** procedure to provide a handle to the group once it has been moved.
    */
    group->index = (MtrHalfWord) low;

    return(group);

} /* end of Cudd_MakeZddTreeNode */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Tree sifting algorithm for ZDDs.]

  Description [Tree sifting algorithm for ZDDs. Assumes that a tree
  representing a group hierarchy is passed as a parameter. It then
  reorders each group in postorder fashion by calling
  zddTreeSiftingAux.  Assumes that no dead nodes are present.  Returns
  1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int
cuddZddTreeSifting(
  DdManager * table /* DD table */,
  Cudd_ReorderingType method /* reordering method for the groups of leaves */)
{
    int i;
    int nvars;
    int result;
    int tempTree;

    /* If no tree is provided we create a temporary one in which all
    ** variables are in a single group. After reordering this tree is
    ** destroyed.
    */
    tempTree = table->treeZ == NULL;
    if (tempTree) {
	table->treeZ = Mtr_InitGroupTree(0,table->sizeZ);
	table->treeZ->index = table->invpermZ[0];
    }
    nvars = table->sizeZ;

#ifdef DD_DEBUG
    if (pr > 0 && !tempTree)
	(void) fprintf(table->out,"cuddZddTreeSifting:");
    Mtr_PrintGroups(table->treeZ,pr <= 0);
#endif
#if 0
    /* Debugging code. */
    if (table->tree && table->treeZ) {
	(void) fprintf(table->out,"\n");
	Mtr_PrintGroups(table->tree, 0);
	cuddPrintVarGroups(table,table->tree,0,0);
	for (i = 0; i < table->size; i++) {
	    (void) fprintf(table->out,"%s%d",
			   (i == 0) ? "" : ",", table->invperm[i]);
	}
	(void) fprintf(table->out,"\n");
	for (i = 0; i < table->size; i++) {
	    (void) fprintf(table->out,"%s%d",
			   (i == 0) ? "" : ",", table->perm[i]);
	}
	(void) fprintf(table->out,"\n\n");
	Mtr_PrintGroups(table->treeZ,0);
	cuddPrintVarGroups(table,table->treeZ,1,0);
	for (i = 0; i < table->sizeZ; i++) {
	    (void) fprintf(table->out,"%s%d",
			   (i == 0) ? "" : ",", table->invpermZ[i]);
	}
	(void) fprintf(table->out,"\n");
	for (i = 0; i < table->sizeZ; i++) {
	    (void) fprintf(table->out,"%s%d",
			   (i == 0) ? "" : ",", table->permZ[i]);
	}
	(void) fprintf(table->out,"\n");
    }
    /* End of debugging code. */
#endif
#ifdef DD_STATS
    extsymmcalls = 0;
    extsymm = 0;
    secdiffcalls = 0;
    secdiff = 0;
    secdiffmisfire = 0;

    (void) fprintf(table->out,"\n");
    if (!tempTree)
	(void) fprintf(table->out,"#:IM_NODES  %8d: group tree nodes\n",
		       zddCountInternalMtrNodes(table,table->treeZ));
#endif

    /* Initialize the group of each subtable to itself. Initially
    ** there are no groups. Groups are created according to the tree
    ** structure in postorder fashion.
    */
    for (i = 0; i < nvars; i++)
	table->subtableZ[i].next = i;

    /* Reorder. */
    result = zddTreeSiftingAux(table, table->treeZ, method);

#ifdef DD_STATS		/* print stats */
    if (!tempTree && method == CUDD_REORDER_GROUP_SIFT &&
	(table->groupcheck == CUDD_GROUP_CHECK7 ||
	 table->groupcheck == CUDD_GROUP_CHECK5)) {
	(void) fprintf(table->out,"\nextsymmcalls = %d\n",extsymmcalls);
	(void) fprintf(table->out,"extsymm = %d",extsymm);
    }
    if (!tempTree && method == CUDD_REORDER_GROUP_SIFT &&
	table->groupcheck == CUDD_GROUP_CHECK7) {
	(void) fprintf(table->out,"\nsecdiffcalls = %d\n",secdiffcalls);
	(void) fprintf(table->out,"secdiff = %d\n",secdiff);
	(void) fprintf(table->out,"secdiffmisfire = %d",secdiffmisfire);
    }
#endif

    if (tempTree)
	Cudd_FreeZddTree(table);
    return(result);

} /* end of cuddZddTreeSifting */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Visits the group tree and reorders each group.]

  Description [Recursively visits the group tree and reorders each
  group in postorder fashion.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddTreeSiftingAux(
  DdManager * table,
  MtrNode * treenode,
  Cudd_ReorderingType method)
{
    MtrNode  *auxnode;
    int res;

#ifdef DD_DEBUG
    Mtr_PrintGroups(treenode,1);
#endif

    auxnode = treenode;
    while (auxnode != NULL) {
	if (auxnode->child != NULL) {
	    if (!zddTreeSiftingAux(table, auxnode->child, method))
		return(0);
	    res = zddReorderChildren(table, auxnode, CUDD_REORDER_GROUP_SIFT);
	    if (res == 0)
		return(0);
	} else if (auxnode->size > 1) {
	    if (!zddReorderChildren(table, auxnode, method))
		return(0);
	}
	auxnode = auxnode->younger;
    }

    return(1);

} /* end of zddTreeSiftingAux */


#ifdef DD_STATS
/**Function********************************************************************

  Synopsis    [Counts the number of internal nodes of the group tree.]

  Description [Counts the number of internal nodes of the group tree.
  Returns the count.]

  SideEffects [None]

******************************************************************************/
static int
zddCountInternalMtrNodes(
  DdManager * table,
  MtrNode * treenode)
{
    MtrNode *auxnode;
    int     count,nodeCount;


    nodeCount = 0;
    auxnode = treenode;
    while (auxnode != NULL) {
	if (!(MTR_TEST(auxnode,MTR_TERMINAL))) {
	    nodeCount++;
	    count = zddCountInternalMtrNodes(table,auxnode->child);
	    nodeCount += count;
	}
	auxnode = auxnode->younger;
    }

    return(nodeCount);

} /* end of zddCountInternalMtrNodes */
#endif


/**Function********************************************************************

  Synopsis    [Reorders the children of a group tree node according to
  the options.]

  Description [Reorders the children of a group tree node according to
  the options. After reordering puts all the variables in the group
  and/or its descendents in a single group. This allows hierarchical
  reordering.  If the variables in the group do not exist yet, simply
  does nothing. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddReorderChildren(
  DdManager * table,
  MtrNode * treenode,
  Cudd_ReorderingType method)
{
    int lower;
    int upper;
    int result;
    unsigned int initialSize;

    zddFindNodeHiLo(table,treenode,&lower,&upper);
    /* If upper == -1 these variables do not exist yet. */
    if (upper == -1)
	return(1);

    if (treenode->flags == MTR_FIXED) {
	result = 1;
    } else {
#ifdef DD_STATS
	(void) fprintf(table->out," ");
#endif
	switch (method) {
	case CUDD_REORDER_RANDOM:
	case CUDD_REORDER_RANDOM_PIVOT:
	    result = cuddZddSwapping(table,lower,upper,method);
	    break;
	case CUDD_REORDER_SIFT:
	    result = cuddZddSifting(table,lower,upper);
	    break;
	case CUDD_REORDER_SIFT_CONVERGE:
	    do {
		initialSize = table->keysZ;
		result = cuddZddSifting(table,lower,upper);
		if (initialSize <= table->keysZ)
		    break;
#ifdef DD_STATS
		else
		    (void) fprintf(table->out,"\n");
#endif
	    } while (result != 0);
	    break;
	case CUDD_REORDER_SYMM_SIFT:
	    result = cuddZddSymmSifting(table,lower,upper);
	    break;
	case CUDD_REORDER_SYMM_SIFT_CONV:
	    result = cuddZddSymmSiftingConv(table,lower,upper);
	    break;
	case CUDD_REORDER_GROUP_SIFT:
	    result = zddGroupSifting(table,lower,upper);
	    break;
	case CUDD_REORDER_LINEAR:
	    result = cuddZddLinearSifting(table,lower,upper);
	    break;
	case CUDD_REORDER_LINEAR_CONVERGE:
	    do {
		initialSize = table->keysZ;
		result = cuddZddLinearSifting(table,lower,upper);
		if (initialSize <= table->keysZ)
		    break;
#ifdef DD_STATS
		else
		    (void) fprintf(table->out,"\n");
#endif
	    } while (result != 0);
	    break;
	default:
	    return(0);
	}
    }

    /* Create a single group for all the variables that were sifted,
    ** so that they will be treated as a single block by successive
    ** invocations of zddGroupSifting.
    */
    zddMergeGroups(table,treenode,lower,upper);

#ifdef DD_DEBUG
    if (pr > 0) (void) fprintf(table->out,"zddReorderChildren:");
#endif

    return(result);

} /* end of zddReorderChildren */


/**Function********************************************************************

  Synopsis    [Finds the lower and upper bounds of the group represented
  by treenode.]

  Description [Finds the lower and upper bounds of the group represented
  by treenode.  The high and low fields of treenode are indices.  From
  those we need to derive the current positions, and find maximum and
  minimum.]

  SideEffects [The bounds are returned as side effects.]

  SeeAlso     []

******************************************************************************/
static void
zddFindNodeHiLo(
  DdManager * table,
  MtrNode * treenode,
  int * lower,
  int * upper)
{
    int low;
    int high;

    /* Check whether no variables in this group already exist.
    ** If so, return immediately. The calling procedure will know from
    ** the values of upper that no reordering is needed.
    */
    if ((int) treenode->low >= table->sizeZ) {
	*lower = table->sizeZ;
	*upper = -1;
	return;
    }

    *lower = low = (unsigned int) table->permZ[treenode->index];
    high = (int) (low + treenode->size - 1);

    if (high >= table->sizeZ) {
	/* This is the case of a partially existing group. The aim is to
	** reorder as many variables as safely possible.  If the tree
	** node is terminal, we just reorder the subset of the group
	** that is currently in existence.  If the group has
	** subgroups, then we only reorder those subgroups that are
	** fully instantiated.  This way we avoid breaking up a group.
	*/
	MtrNode *auxnode = treenode->child;
	if (auxnode == NULL) {
	    *upper = (unsigned int) table->sizeZ - 1;
	} else {
	    /* Search the subgroup that strands the table->sizeZ line.
	    ** If the first group starts at 0 and goes past table->sizeZ
	    ** upper will get -1, thus correctly signaling that no reordering
	    ** should take place.
	    */
	    while (auxnode != NULL) {
		int thisLower = table->permZ[auxnode->low];
		int thisUpper = thisLower + auxnode->size - 1;
		if (thisUpper >= table->sizeZ && thisLower < table->sizeZ)
		    *upper = (unsigned int) thisLower - 1;
		auxnode = auxnode->younger;
	    }
	}
    } else {
	/* Normal case: All the variables of the group exist. */
	*upper = (unsigned int) high;
    }

#ifdef DD_DEBUG
    /* Make sure that all variables in group are contiguous. */
    assert(treenode->size >= *upper - *lower + 1);
#endif

    return;

} /* end of zddFindNodeHiLo */


/**Function********************************************************************

  Synopsis    [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the variables
  according to the number of keys in the subtables.  Returns the
  difference in number of keys between the two variables being
  compared.]

  SideEffects [None]

******************************************************************************/
static int
zddUniqueCompareGroup(
  int * ptrX,
  int * ptrY)
{
#if 0
    if (entry[*ptrY] == entry[*ptrX]) {
	return((*ptrX) - (*ptrY));
    }
#endif
    return(entry[*ptrY] - entry[*ptrX]);

} /* end of zddUniqueCompareGroup */


/**Function********************************************************************

  Synopsis    [Sifts from treenode->low to treenode->high.]

  Description [Sifts from treenode->low to treenode->high. If
  croupcheck == CUDD_GROUP_CHECK7, it checks for group creation at the
  end of the initial sifting. If a group is created, it is then sifted
  again. After sifting one variable, the group that contains it is
  dissolved.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupSifting(
  DdManager * table,
  int  lower,
  int  upper)
{
    int		*var;
    int		i,j,x,xInit;
    int		nvars;
    int		classes;
    int		result;
    int		*sifted;
#ifdef DD_STATS
    unsigned	previousSize;
#endif
    int		xindex;

    nvars = table->sizeZ;

    /* Order variables to sift. */
    entry = NULL;
    sifted = NULL;
    var = ALLOC(int,nvars);
    if (var == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto zddGroupSiftingOutOfMem;
    }
    entry = ALLOC(int,nvars);
    if (entry == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto zddGroupSiftingOutOfMem;
    }
    sifted = ALLOC(int,nvars);
    if (sifted == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto zddGroupSiftingOutOfMem;
    }

    /* Here we consider only one representative for each group. */
    for (i = 0, classes = 0; i < nvars; i++) {
	sifted[i] = 0;
	x = table->permZ[i];
	if ((unsigned) x >= table->subtableZ[x].next) {
	    entry[i] = table->subtableZ[x].keys;
	    var[classes] = i;
	    classes++;
	}
    }

    qsort((void *)var,classes,sizeof(int),(DD_QSFP)zddUniqueCompareGroup);

    /* Now sift. */
    for (i = 0; i < ddMin(table->siftMaxVar,classes); i++) {
	if (zddTotalNumberSwapping >= table->siftMaxSwap)
	    break;
        if (util_cpu_time() - table->startTime > table->timeLimit) {
            table->autoDynZ = 0; /* prevent further reordering */
            break;
        }
	xindex = var[i];
	if (sifted[xindex] == 1) /* variable already sifted as part of group */
	    continue;
	x = table->permZ[xindex]; /* find current level of this variable */
	if (x < lower || x > upper)
	    continue;
#ifdef DD_STATS
	previousSize = table->keysZ;
#endif
#ifdef DD_DEBUG
	/* x is bottom of group */
	assert((unsigned) x >= table->subtableZ[x].next);
#endif
	result = zddGroupSiftingAux(table,x,lower,upper);
	if (!result) goto zddGroupSiftingOutOfMem;

#ifdef DD_STATS
	if (table->keysZ < previousSize) {
	    (void) fprintf(table->out,"-");
	} else if (table->keysZ > previousSize) {
	    (void) fprintf(table->out,"+");
	} else {
	    (void) fprintf(table->out,"=");
	}
	fflush(table->out);
#endif

	/* Mark variables in the group just sifted. */
	x = table->permZ[xindex];
	if ((unsigned) x != table->subtableZ[x].next) {
	    xInit = x;
	    do {
		j = table->invpermZ[x];
		sifted[j] = 1;
		x = table->subtableZ[x].next;
	    } while (x != xInit);
	}

#ifdef DD_DEBUG
	if (pr > 0) (void) fprintf(table->out,"zddGroupSifting:");
#endif
    } /* for */

    FREE(sifted);
    FREE(var);
    FREE(entry);

    return(1);

zddGroupSiftingOutOfMem:
    if (entry != NULL)	FREE(entry);
    if (var != NULL)	FREE(var);
    if (sifted != NULL)	FREE(sifted);

    return(0);

} /* end of zddGroupSifting */


/**Function********************************************************************

  Synopsis    [Sifts one variable up and down until it has taken all
  positions. Checks for aggregation.]

  Description [Sifts one variable up and down until it has taken all
  positions. Checks for aggregation. There may be at most two sweeps,
  even if the group grows.  Assumes that x is either an isolated
  variable, or it is the bottom of a group. All groups may not have
  been found. The variable being moved is returned to the best position
  seen during sifting.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupSiftingAux(
  DdManager * table,
  int  x,
  int  xLow,
  int  xHigh)
{
    Move *move;
    Move *moves;	/* list of moves */
    int  initialSize;
    int  result;


#ifdef DD_DEBUG
    if (pr > 0) (void) fprintf(table->out,"zddGroupSiftingAux from %d to %d\n",xLow,xHigh);
    assert((unsigned) x >= table->subtableZ[x].next); /* x is bottom of group */
#endif

    initialSize = table->keysZ;
    moves = NULL;

    if (x == xLow) { /* Sift down */
#ifdef DD_DEBUG
	/* x must be a singleton */
	assert((unsigned) x == table->subtableZ[x].next);
#endif
	if (x == xHigh) return(1);	/* just one variable */

	if (!zddGroupSiftingDown(table,x,xHigh,&moves))
	    goto zddGroupSiftingAuxOutOfMem;
	/* at this point x == xHigh, unless early term */

	/* move backward and stop at best position */
	result = zddGroupSiftingBackward(table,moves,initialSize);
#ifdef DD_DEBUG
	assert(table->keysZ <= (unsigned) initialSize);
#endif
	if (!result) goto zddGroupSiftingAuxOutOfMem;

    } else if (cuddZddNextHigh(table,x) > xHigh) { /* Sift up */
#ifdef DD_DEBUG
	/* x is bottom of group */
	assert((unsigned) x >= table->subtableZ[x].next);
#endif
	/* Find top of x's group */
	x = table->subtableZ[x].next;

	if (!zddGroupSiftingUp(table,x,xLow,&moves))
	    goto zddGroupSiftingAuxOutOfMem;
	/* at this point x == xLow, unless early term */

	/* move backward and stop at best position */
	result = zddGroupSiftingBackward(table,moves,initialSize);
#ifdef DD_DEBUG
	assert(table->keysZ <= (unsigned) initialSize);
#endif
	if (!result) goto zddGroupSiftingAuxOutOfMem;

    } else if (x - xLow > xHigh - x) { /* must go down first: shorter */
	if (!zddGroupSiftingDown(table,x,xHigh,&moves))
	    goto zddGroupSiftingAuxOutOfMem;
	/* at this point x == xHigh, unless early term */

	/* Find top of group */
	if (moves) {
	    x = moves->y;
	}
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;
	x = table->subtableZ[x].next;
#ifdef DD_DEBUG
	/* x should be the top of a group */
	assert((unsigned) x <= table->subtableZ[x].next);
#endif

	if (!zddGroupSiftingUp(table,x,xLow,&moves))
	    goto zddGroupSiftingAuxOutOfMem;

	/* move backward and stop at best position */
	result = zddGroupSiftingBackward(table,moves,initialSize);
#ifdef DD_DEBUG
	assert(table->keysZ <= (unsigned) initialSize);
#endif
	if (!result) goto zddGroupSiftingAuxOutOfMem;

    } else { /* moving up first: shorter */
	/* Find top of x's group */
	x = table->subtableZ[x].next;

	if (!zddGroupSiftingUp(table,x,xLow,&moves))
	    goto zddGroupSiftingAuxOutOfMem;
	/* at this point x == xHigh, unless early term */

	if (moves) {
	    x = moves->x;
	}
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;
#ifdef DD_DEBUG
	/* x is bottom of a group */
	assert((unsigned) x >= table->subtableZ[x].next);
#endif

	if (!zddGroupSiftingDown(table,x,xHigh,&moves))
	    goto zddGroupSiftingAuxOutOfMem;

	/* move backward and stop at best position */
	result = zddGroupSiftingBackward(table,moves,initialSize);
#ifdef DD_DEBUG
	assert(table->keysZ <= (unsigned) initialSize);
#endif
	if (!result) goto zddGroupSiftingAuxOutOfMem;
    }

    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }

    return(1);

zddGroupSiftingAuxOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }

    return(0);

} /* end of zddGroupSiftingAux */


/**Function********************************************************************

  Synopsis    [Sifts up a variable until either it reaches position xLow
  or the size of the DD heap increases too much.]

  Description [Sifts up a variable until either it reaches position
  xLow or the size of the DD heap increases too much. Assumes that y is
  the top of a group (or a singleton).  Checks y for aggregation to the
  adjacent variables. Records all the moves that are appended to the
  list of moves received as input and returned as a side effect.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupSiftingUp(
  DdManager * table,
  int  y,
  int  xLow,
  Move ** moves)
{
    Move *move;
    int  x;
    int  size;
    int  gxtop;
    int  limitSize;

    limitSize = table->keysZ;

    x = cuddZddNextLow(table,y);
    while (x >= xLow) {
	gxtop = table->subtableZ[x].next;
	if (table->subtableZ[x].next == (unsigned) x &&
	    table->subtableZ[y].next == (unsigned) y) {
	    /* x and y are self groups */
	    size = cuddZddSwapInPlace(table,x,y);
#ifdef DD_DEBUG
	    assert(table->subtableZ[x].next == (unsigned) x);
	    assert(table->subtableZ[y].next == (unsigned) y);
#endif
	    if (size == 0) goto zddGroupSiftingUpOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto zddGroupSiftingUpOutOfMem;
	    move->x = x;
	    move->y = y;
	    move->flags = MTR_DEFAULT;
	    move->size = size;
	    move->next = *moves;
	    *moves = move;

#ifdef DD_DEBUG
	    if (pr > 0) (void) fprintf(table->out,"zddGroupSiftingUp (2 single groups):\n");
#endif
	    if ((double) size > (double) limitSize * table->maxGrowth)
		return(1);
	    if (size < limitSize) limitSize = size;
	} else { /* group move */
	    size = zddGroupMove(table,x,y,moves);
	    if (size == 0) goto zddGroupSiftingUpOutOfMem;
	    if ((double) size > (double) limitSize * table->maxGrowth)
		return(1);
	    if (size < limitSize) limitSize = size;
	}
	y = gxtop;
	x = cuddZddNextLow(table,y);
    }

    return(1);

zddGroupSiftingUpOutOfMem:
    while (*moves != NULL) {
	move = (*moves)->next;
	cuddDeallocMove(table, *moves);
	*moves = move;
    }
    return(0);

} /* end of zddGroupSiftingUp */


/**Function********************************************************************

  Synopsis    [Sifts down a variable until it reaches position xHigh.]

  Description [Sifts down a variable until it reaches position xHigh.
  Assumes that x is the bottom of a group (or a singleton).  Records
  all the moves.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupSiftingDown(
  DdManager * table,
  int  x,
  int  xHigh,
  Move ** moves)
{
    Move *move;
    int  y;
    int  size;
    int  limitSize;
    int  gybot;


    /* Initialize R */
    limitSize = size = table->keysZ;
    y = cuddZddNextHigh(table,x);
    while (y <= xHigh) {
	/* Find bottom of y group. */
	gybot = table->subtableZ[y].next;
	while (table->subtableZ[gybot].next != (unsigned) y)
	    gybot = table->subtableZ[gybot].next;

	if (table->subtableZ[x].next == (unsigned) x &&
	    table->subtableZ[y].next == (unsigned) y) {
	    /* x and y are self groups */
	    size = cuddZddSwapInPlace(table,x,y);
#ifdef DD_DEBUG
	    assert(table->subtableZ[x].next == (unsigned) x);
	    assert(table->subtableZ[y].next == (unsigned) y);
#endif
	    if (size == 0) goto zddGroupSiftingDownOutOfMem;

	    /* Record move. */
	    move = (Move *) cuddDynamicAllocNode(table);
	    if (move == NULL) goto zddGroupSiftingDownOutOfMem;
	    move->x = x;
	    move->y = y;
	    move->flags = MTR_DEFAULT;
	    move->size = size;
	    move->next = *moves;
	    *moves = move;

#ifdef DD_DEBUG
	    if (pr > 0) (void) fprintf(table->out,"zddGroupSiftingDown (2 single groups):\n");
#endif
	    if ((double) size > (double) limitSize * table->maxGrowth)
		return(1);
	    if (size < limitSize) limitSize = size;
	    x = y;
	    y = cuddZddNextHigh(table,x);
	} else { /* Group move */
	    size = zddGroupMove(table,x,y,moves);
	    if (size == 0) goto zddGroupSiftingDownOutOfMem;
	    if ((double) size > (double) limitSize * table->maxGrowth)
		return(1);
	    if (size < limitSize) limitSize = size;
	}
	x = gybot;
	y = cuddZddNextHigh(table,x);
    }

    return(1);

zddGroupSiftingDownOutOfMem:
    while (*moves != NULL) {
	move = (*moves)->next;
	cuddDeallocMove(table, *moves);
	*moves = move;
    }

    return(0);

} /* end of zddGroupSiftingDown */


/**Function********************************************************************

  Synopsis    [Swaps two groups and records the move.]

  Description [Swaps two groups and records the move. Returns the
  number of keys in the DD table in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupMove(
  DdManager * table,
  int  x,
  int  y,
  Move ** moves)
{
    Move *move;
    int  size;
    int  i,j,xtop,xbot,xsize,ytop,ybot,ysize,newxtop;
    int  swapx,swapy;
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
    int  initialSize,bestSize;
#endif

#ifdef DD_DEBUG
    /* We assume that x < y */
    assert(x < y);
#endif
    /* Find top, bottom, and size for the two groups. */
    xbot = x;
    xtop = table->subtableZ[x].next;
    xsize = xbot - xtop + 1;
    ybot = y;
    while ((unsigned) ybot < table->subtableZ[ybot].next)
	ybot = table->subtableZ[ybot].next;
    ytop = y;
    ysize = ybot - ytop + 1;

#if defined(DD_DEBUG) && defined(DD_VERBOSE)
    initialSize = bestSize = table->keysZ;
#endif
    /* Sift the variables of the second group up through the first group */
    for (i = 1; i <= ysize; i++) {
	for (j = 1; j <= xsize; j++) {
	    size = cuddZddSwapInPlace(table,x,y);
	    if (size == 0) goto zddGroupMoveOutOfMem;
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
	    if (size < bestSize)
		bestSize = size;
#endif
	    swapx = x; swapy = y;
	    y = x;
	    x = cuddZddNextLow(table,y);
	}
	y = ytop + i;
	x = cuddZddNextLow(table,y);
    }
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
    if ((bestSize < initialSize) && (bestSize < size))
	(void) fprintf(table->out,"Missed local minimum: initialSize:%d  bestSize:%d  finalSize:%d\n",initialSize,bestSize,size);
#endif

    /* fix groups */
    y = xtop; /* ytop is now where xtop used to be */
    for (i = 0; i < ysize - 1; i++) {
	table->subtableZ[y].next = cuddZddNextHigh(table,y);
	y = cuddZddNextHigh(table,y);
    }
    table->subtableZ[y].next = xtop; /* y is bottom of its group, join */
				    /* it to top of its group */
    x = cuddZddNextHigh(table,y);
    newxtop = x;
    for (i = 0; i < xsize - 1; i++) {
	table->subtableZ[x].next = cuddZddNextHigh(table,x);
	x = cuddZddNextHigh(table,x);
    }
    table->subtableZ[x].next = newxtop; /* x is bottom of its group, join */
				    /* it to top of its group */
#ifdef DD_DEBUG
    if (pr > 0) (void) fprintf(table->out,"zddGroupMove:\n");
#endif

    /* Store group move */
    move = (Move *) cuddDynamicAllocNode(table);
    if (move == NULL) goto zddGroupMoveOutOfMem;
    move->x = swapx;
    move->y = swapy;
    move->flags = MTR_DEFAULT;
    move->size = table->keysZ;
    move->next = *moves;
    *moves = move;

    return(table->keysZ);

zddGroupMoveOutOfMem:
    while (*moves != NULL) {
	move = (*moves)->next;
	cuddDeallocMove(table, *moves);
	*moves = move;
    }
    return(0);

} /* end of zddGroupMove */


/**Function********************************************************************

  Synopsis    [Undoes the swap two groups.]

  Description [Undoes the swap two groups.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupMoveBackward(
  DdManager * table,
  int  x,
  int  y)
{
    int size;
    int i,j,xtop,xbot,xsize,ytop,ybot,ysize,newxtop;


#ifdef DD_DEBUG
    /* We assume that x < y */
    assert(x < y);
#endif

    /* Find top, bottom, and size for the two groups. */
    xbot = x;
    xtop = table->subtableZ[x].next;
    xsize = xbot - xtop + 1;
    ybot = y;
    while ((unsigned) ybot < table->subtableZ[ybot].next)
	ybot = table->subtableZ[ybot].next;
    ytop = y;
    ysize = ybot - ytop + 1;

    /* Sift the variables of the second group up through the first group */
    for (i = 1; i <= ysize; i++) {
	for (j = 1; j <= xsize; j++) {
	    size = cuddZddSwapInPlace(table,x,y);
	    if (size == 0)
		return(0);
	    y = x;
	    x = cuddZddNextLow(table,y);
	}
	y = ytop + i;
	x = cuddZddNextLow(table,y);
    }

    /* fix groups */
    y = xtop;
    for (i = 0; i < ysize - 1; i++) {
	table->subtableZ[y].next = cuddZddNextHigh(table,y);
	y = cuddZddNextHigh(table,y);
    }
    table->subtableZ[y].next = xtop; /* y is bottom of its group, join */
				    /* to its top */
    x = cuddZddNextHigh(table,y);
    newxtop = x;
    for (i = 0; i < xsize - 1; i++) {
	table->subtableZ[x].next = cuddZddNextHigh(table,x);
	x = cuddZddNextHigh(table,x);
    }
    table->subtableZ[x].next = newxtop; /* x is bottom of its group, join */
				    /* to its top */
#ifdef DD_DEBUG
    if (pr > 0) (void) fprintf(table->out,"zddGroupMoveBackward:\n");
#endif

    return(1);

} /* end of zddGroupMoveBackward */


/**Function********************************************************************

  Synopsis    [Determines the best position for a variables and returns
  it there.]

  Description [Determines the best position for a variables and returns
  it there.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int
zddGroupSiftingBackward(
  DdManager * table,
  Move * moves,
  int  size)
{
    Move *move;
    int  res;


    for (move = moves; move != NULL; move = move->next) {
	if (move->size < size) {
	    size = move->size;
	}
    }

    for (move = moves; move != NULL; move = move->next) {
	if (move->size == size) return(1);
	if ((table->subtableZ[move->x].next == move->x) &&
	(table->subtableZ[move->y].next == move->y)) {
	    res = cuddZddSwapInPlace(table,(int)move->x,(int)move->y);
	    if (!res) return(0);
#ifdef DD_DEBUG
	    if (pr > 0) (void) fprintf(table->out,"zddGroupSiftingBackward:\n");
	    assert(table->subtableZ[move->x].next == move->x);
	    assert(table->subtableZ[move->y].next == move->y);
#endif
	} else { /* Group move necessary */
	    res = zddGroupMoveBackward(table,(int)move->x,(int)move->y);
	    if (!res) return(0);
	}
    }

    return(1);

} /* end of zddGroupSiftingBackward */


/**Function********************************************************************

  Synopsis    [Merges groups in the DD table.]

  Description [Creates a single group from low to high and adjusts the
  idex field of the tree node.]

  SideEffects [None]

******************************************************************************/
static void
zddMergeGroups(
  DdManager * table,
  MtrNode * treenode,
  int  low,
  int  high)
{
    int i;
    MtrNode *auxnode;
    int saveindex;
    int newindex;

    /* Merge all variables from low to high in one group, unless
    ** this is the topmost group. In such a case we do not merge lest
    ** we lose the symmetry information. */
    if (treenode != table->treeZ) {
	for (i = low; i < high; i++)
	    table->subtableZ[i].next = i+1;
	table->subtableZ[high].next = low;
    }

    /* Adjust the index fields of the tree nodes. If a node is the
    ** first child of its parent, then the parent may also need adjustment. */
    saveindex = treenode->index;
    newindex = table->invpermZ[low];
    auxnode = treenode;
    do {
	auxnode->index = newindex;
	if (auxnode->parent == NULL ||
		(int) auxnode->parent->index != saveindex)
	    break;
	auxnode = auxnode->parent;
    } while (1);
    return;

} /* end of zddMergeGroups */
