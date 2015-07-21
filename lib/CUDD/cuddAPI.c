/**CFile***********************************************************************

  FileName    [cuddAPI.c]

  PackageName [cudd]

  Synopsis    [Application interface functions.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_addNewVar()
		<li> Cudd_addNewVarAtLevel()
		<li> Cudd_bddNewVar()
		<li> Cudd_bddNewVarAtLevel()
		<li> Cudd_addIthVar()
		<li> Cudd_bddIthVar()
		<li> Cudd_zddIthVar()
		<li> Cudd_zddVarsFromBddVars()
		<li> Cudd_addConst()
		<li> Cudd_IsNonConstant()
		<li> Cudd_ReadStartTime()
                <li> Cudd_ReadElapsedTime()
		<li> Cudd_SetStartTime()
                <li> Cudd_ResetStartTime()
		<li> Cudd_ReadTimeLimit()
		<li> Cudd_SetTimeLimit()
                <li> Cudd_UpdateTimeLimit()
                <li> Cudd_IncreaseTimeLimit()
                <li> Cudd_UnsetTimeLimit()
                <li> Cudd_TimeLimited()
		<li> Cudd_AutodynEnable()
		<li> Cudd_AutodynDisable()
		<li> Cudd_ReorderingStatus()
		<li> Cudd_AutodynEnableZdd()
		<li> Cudd_AutodynDisableZdd()
		<li> Cudd_ReorderingStatusZdd()
		<li> Cudd_zddRealignmentEnabled()
		<li> Cudd_zddRealignEnable()
		<li> Cudd_zddRealignDisable()
		<li> Cudd_bddRealignmentEnabled()
		<li> Cudd_bddRealignEnable()
		<li> Cudd_bddRealignDisable()
		<li> Cudd_ReadOne()
		<li> Cudd_ReadZddOne()
		<li> Cudd_ReadZero()
		<li> Cudd_ReadLogicZero()
		<li> Cudd_ReadPlusInfinity()
		<li> Cudd_ReadMinusInfinity()
		<li> Cudd_ReadBackground()
		<li> Cudd_SetBackground()
		<li> Cudd_ReadCacheSlots()
		<li> Cudd_ReadCacheUsedSlots()
		<li> Cudd_ReadCacheLookUps()
		<li> Cudd_ReadCacheHits()
		<li> Cudd_ReadMinHit()
		<li> Cudd_SetMinHit()
		<li> Cudd_ReadLooseUpTo()
		<li> Cudd_SetLooseUpTo()
		<li> Cudd_ReadMaxCache()
		<li> Cudd_ReadMaxCacheHard()
		<li> Cudd_SetMaxCacheHard()
		<li> Cudd_ReadSize()
		<li> Cudd_ReadSlots()
		<li> Cudd_ReadUsedSlots()
		<li> Cudd_ExpectedUsedSlots()
		<li> Cudd_ReadKeys()
		<li> Cudd_ReadDead()
		<li> Cudd_ReadMinDead()
		<li> Cudd_ReadReorderings()
		<li> Cudd_ReadMaxReorderings()
		<li> Cudd_SetMaxReorderings()
		<li> Cudd_ReadReorderingTime()
		<li> Cudd_ReadGarbageCollections()
		<li> Cudd_ReadGarbageCollectionTime()
		<li> Cudd_ReadNodesFreed()
		<li> Cudd_ReadNodesDropped()
		<li> Cudd_ReadUniqueLookUps()
		<li> Cudd_ReadUniqueLinks()
		<li> Cudd_ReadSiftMaxVar()
		<li> Cudd_SetSiftMaxVar()
		<li> Cudd_ReadMaxGrowth()
		<li> Cudd_SetMaxGrowth()
		<li> Cudd_ReadMaxGrowthAlternate()
		<li> Cudd_SetMaxGrowthAlternate()
		<li> Cudd_ReadReorderingCycle()
		<li> Cudd_SetReorderingCycle()
		<li> Cudd_ReadTree()
		<li> Cudd_SetTree()
		<li> Cudd_FreeTree()
		<li> Cudd_ReadZddTree()
		<li> Cudd_SetZddTree()
		<li> Cudd_FreeZddTree()
		<li> Cudd_NodeReadIndex()
		<li> Cudd_ReadPerm()
		<li> Cudd_ReadInvPerm()
		<li> Cudd_ReadVars()
		<li> Cudd_ReadEpsilon()
		<li> Cudd_SetEpsilon()
		<li> Cudd_ReadGroupCheck()
		<li> Cudd_SetGroupcheck()
		<li> Cudd_GarbageCollectionEnabled()
		<li> Cudd_EnableGarbageCollection()
		<li> Cudd_DisableGarbageCollection()
		<li> Cudd_DeadAreCounted()
		<li> Cudd_TurnOnCountDead()
		<li> Cudd_TurnOffCountDead()
		<li> Cudd_ReadRecomb()
		<li> Cudd_SetRecomb()
		<li> Cudd_ReadSymmviolation()
		<li> Cudd_SetSymmviolation()
		<li> Cudd_ReadArcviolation()
		<li> Cudd_SetArcviolation()
		<li> Cudd_ReadPopulationSize()
		<li> Cudd_SetPopulationSize()
		<li> Cudd_ReadNumberXovers()
		<li> Cudd_SetNumberXovers()
                <li> Cudd_ReadOrderRandomization()
                <li> Cudd_SetOrderRandomization()
		<li> Cudd_ReadMemoryInUse()
		<li> Cudd_PrintInfo()
		<li> Cudd_ReadPeakNodeCount()
		<li> Cudd_ReadPeakLiveNodeCount()
		<li> Cudd_ReadNodeCount()
		<li> Cudd_zddReadNodeCount()
		<li> Cudd_AddHook()
		<li> Cudd_RemoveHook()
		<li> Cudd_IsInHook()
		<li> Cudd_StdPreReordHook()
		<li> Cudd_StdPostReordHook()
		<li> Cudd_EnableReorderingReporting()
		<li> Cudd_DisableReorderingReporting()
		<li> Cudd_ReorderingReporting()
                <li> Cudd_PrintGroupedOrder()
                <li> Cudd_EnableOrderingMonitoring()
                <li> Cudd_DisableOrderingMonitoring()
                <li> Cudd_OrderingMonitoring()
		<li> Cudd_ReadErrorCode()
		<li> Cudd_ClearErrorCode()
		<li> Cudd_ReadStdout()
		<li> Cudd_SetStdout()
		<li> Cudd_ReadStderr()
		<li> Cudd_SetStderr()
		<li> Cudd_ReadNextReordering()
		<li> Cudd_SetNextReordering()
		<li> Cudd_ReadSwapSteps()
		<li> Cudd_ReadMaxLive()
		<li> Cudd_SetMaxLive()
		<li> Cudd_ReadMaxMemory()
		<li> Cudd_SetMaxMemory()
		<li> Cudd_bddBindVar()
		<li> Cudd_bddUnbindVar()
		<li> Cudd_bddVarIsBound()
		<li> Cudd_bddSetPiVar()
		<li> Cudd_bddSetPsVar()
		<li> Cudd_bddSetNsVar()
		<li> Cudd_bddIsPiVar()
		<li> Cudd_bddIsPsVar()
		<li> Cudd_bddIsNsVar()
		<li> Cudd_bddSetPairIndex()
		<li> Cudd_bddReadPairIndex()
		<li> Cudd_bddSetVarToBeGrouped()
		<li> Cudd_bddSetVarHardGroup()
		<li> Cudd_bddResetVarToBeGrouped()
		<li> Cudd_bddIsVarToBeGrouped()
		<li> Cudd_bddSetVarToBeUngrouped()
		<li> Cudd_bddIsVarToBeUngrouped()
		<li> Cudd_bddIsVarHardGroup()
		</ul>
	      Static procedures included in this module:
		<ul>
		<li> fixVarTree()
		</ul>]

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
static char rcsid[] DD_UNUSED = "$Id: cuddAPI.c,v 1.64 2012/02/05 01:07:18 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void fixVarTree (MtrNode *treenode, int *perm, int size);
static int addMultiplicityGroups (DdManager *dd, MtrNode *treenode, int multiplicity, char *vmask, char *lmask);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Returns a new ADD variable.]

  Description [Creates a new ADD variable.  The new variable has an
  index equal to the largest previous index plus 1.  Returns a
  pointer to the new variable if successful; NULL otherwise.
  An ADD variable differs from a BDD variable because it points to the
  arithmetic zero, instead of having a complement pointer to 1. ]

  SideEffects [None]

  SeeAlso     [Cudd_bddNewVar Cudd_addIthVar Cudd_addConst
  Cudd_addNewVarAtLevel]

******************************************************************************/
DdNode *
Cudd_addNewVar(
  DdManager * dd)
{
    DdNode *res;

    if ((unsigned int) dd->size >= CUDD_MAXINDEX - 1) return(NULL);
    do {
	dd->reordered = 0;
	res = cuddUniqueInter(dd,dd->size,DD_ONE(dd),DD_ZERO(dd));
    } while (dd->reordered == 1);

    return(res);

} /* end of Cudd_addNewVar */


/**Function********************************************************************

  Synopsis    [Returns a new ADD variable at a specified level.]

  Description [Creates a new ADD variable.  The new variable has an
  index equal to the largest previous index plus 1 and is positioned at
  the specified level in the order.  Returns a pointer to the new
  variable if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addNewVar Cudd_addIthVar Cudd_bddNewVarAtLevel]

******************************************************************************/
DdNode *
Cudd_addNewVarAtLevel(
  DdManager * dd,
  int  level)
{
    DdNode *res;

    if ((unsigned int) dd->size >= CUDD_MAXINDEX - 1) return(NULL);
    if (level >= dd->size) return(Cudd_addIthVar(dd,level));
    if (!cuddInsertSubtables(dd,1,level)) return(NULL);
    do {
	dd->reordered = 0;
	res = cuddUniqueInter(dd,dd->size - 1,DD_ONE(dd),DD_ZERO(dd));
    } while (dd->reordered == 1);

    return(res);

} /* end of Cudd_addNewVarAtLevel */


/**Function********************************************************************

  Synopsis    [Returns a new BDD variable.]

  Description [Creates a new BDD variable.  The new variable has an
  index equal to the largest previous index plus 1.  Returns a
  pointer to the new variable if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addNewVar Cudd_bddIthVar Cudd_bddNewVarAtLevel]

******************************************************************************/
DdNode *
Cudd_bddNewVar(
  DdManager * dd)
{
    DdNode *res;

    if ((unsigned int) dd->size >= CUDD_MAXINDEX - 1) return(NULL);
    res = cuddUniqueInter(dd,dd->size,dd->one,Cudd_Not(dd->one));

    return(res);

} /* end of Cudd_bddNewVar */


/**Function********************************************************************

  Synopsis    [Returns a new BDD variable at a specified level.]

  Description [Creates a new BDD variable.  The new variable has an
  index equal to the largest previous index plus 1 and is positioned at
  the specified level in the order.  Returns a pointer to the new
  variable if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddNewVar Cudd_bddIthVar Cudd_addNewVarAtLevel]

******************************************************************************/
DdNode *
Cudd_bddNewVarAtLevel(
  DdManager * dd,
  int  level)
{
    DdNode *res;

    if ((unsigned int) dd->size >= CUDD_MAXINDEX - 1) return(NULL);
    if (level >= dd->size) return(Cudd_bddIthVar(dd,level));
    if (!cuddInsertSubtables(dd,1,level)) return(NULL);
    res = dd->vars[dd->size - 1];

    return(res);

} /* end of Cudd_bddNewVarAtLevel */


/**Function********************************************************************

  Synopsis    [Returns the ADD variable with index i.]

  Description [Retrieves the ADD variable with index i if it already
  exists, or creates a new ADD variable.  Returns a pointer to the
  variable if successful; NULL otherwise.  An ADD variable differs from
  a BDD variable because it points to the arithmetic zero, instead of
  having a complement pointer to 1. ]

  SideEffects [None]

  SeeAlso     [Cudd_addNewVar Cudd_bddIthVar Cudd_addConst
  Cudd_addNewVarAtLevel]

******************************************************************************/
DdNode *
Cudd_addIthVar(
  DdManager * dd,
  int  i)
{
    DdNode *res;

    if ((unsigned int) i >= CUDD_MAXINDEX - 1) return(NULL);
    do {
	dd->reordered = 0;
	res = cuddUniqueInter(dd,i,DD_ONE(dd),DD_ZERO(dd));
    } while (dd->reordered == 1);

    return(res);

} /* end of Cudd_addIthVar */


/**Function********************************************************************

  Synopsis    [Returns the BDD variable with index i.]

  Description [Retrieves the BDD variable with index i if it already
  exists, or creates a new BDD variable.  Returns a pointer to the
  variable if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddNewVar Cudd_addIthVar Cudd_bddNewVarAtLevel
  Cudd_ReadVars]

******************************************************************************/
DdNode *
Cudd_bddIthVar(
  DdManager * dd,
  int  i)
{
    DdNode *res;

    if ((unsigned int) i >= CUDD_MAXINDEX - 1) return(NULL);
    if (i < dd->size) {
	res = dd->vars[i];
    } else {
	res = cuddUniqueInter(dd,i,dd->one,Cudd_Not(dd->one));
    }

    return(res);

} /* end of Cudd_bddIthVar */


/**Function********************************************************************

  Synopsis    [Returns the ZDD variable with index i.]

  Description [Retrieves the ZDD variable with index i if it already
  exists, or creates a new ZDD variable.  Returns a pointer to the
  variable if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIthVar Cudd_addIthVar]

******************************************************************************/
DdNode *
Cudd_zddIthVar(
  DdManager * dd,
  int  i)
{
    DdNode *res;
    DdNode *zvar;
    DdNode *lower;
    int j;

    if ((unsigned int) i >= CUDD_MAXINDEX - 1) return(NULL);

    /* The i-th variable function has the following structure:
    ** at the level corresponding to index i there is a node whose "then"
    ** child points to the universe, and whose "else" child points to zero.
    ** Above that level there are nodes with identical children.
    */

    /* First we build the node at the level of index i. */
    lower = (i < dd->sizeZ - 1) ? dd->univ[dd->permZ[i]+1] : DD_ONE(dd);
    do {
	dd->reordered = 0;
	zvar = cuddUniqueInterZdd(dd, i, lower, DD_ZERO(dd));
    } while (dd->reordered == 1);

    if (zvar == NULL)
	return(NULL);
    cuddRef(zvar);

    /* Now we add the "filler" nodes above the level of index i. */
    for (j = dd->permZ[i] - 1; j >= 0; j--) {
	do {
	    dd->reordered = 0;
	    res = cuddUniqueInterZdd(dd, dd->invpermZ[j], zvar, zvar);
	} while (dd->reordered == 1);
	if (res == NULL) {
	    Cudd_RecursiveDerefZdd(dd,zvar);
	    return(NULL);
	}
	cuddRef(res);
	Cudd_RecursiveDerefZdd(dd,zvar);
	zvar = res;
    }
    cuddDeref(zvar);
    return(zvar);

} /* end of Cudd_zddIthVar */


/**Function********************************************************************

  Synopsis    [Creates one or more ZDD variables for each BDD variable.]

  Description [Creates one or more ZDD variables for each BDD
  variable.  If some ZDD variables already exist, only the missing
  variables are created.  Parameter multiplicity allows the caller to
  control how many variables are created for each BDD variable in
  existence. For instance, if ZDDs are used to represent covers, two
  ZDD variables are required for each BDD variable.  The order of the
  BDD variables is transferred to the ZDD variables. If a variable
  group tree exists for the BDD variables, a corresponding ZDD
  variable group tree is created by expanding the BDD variable
  tree. In any case, the ZDD variables derived from the same BDD
  variable are merged in a ZDD variable group. If a ZDD variable group
  tree exists, it is freed. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddNewVar Cudd_bddIthVar Cudd_bddNewVarAtLevel]

******************************************************************************/
int
Cudd_zddVarsFromBddVars(
  DdManager * dd /* DD manager */,
  int multiplicity /* how many ZDD variables are created for each BDD variable */)
{
    int res;
    int i, j;
    int allnew;
    int *permutation;

    if (multiplicity < 1) return(0);
    allnew = dd->sizeZ == 0;
    if (dd->size * multiplicity > dd->sizeZ) {
	res = cuddResizeTableZdd(dd,dd->size * multiplicity - 1);
	if (res == 0) return(0);
    }
    /* Impose the order of the BDD variables to the ZDD variables. */
    if (allnew) {
	for (i = 0; i < dd->size; i++) {
	    for (j = 0; j < multiplicity; j++) {
		dd->permZ[i * multiplicity + j] =
		    dd->perm[i] * multiplicity + j;
		dd->invpermZ[dd->permZ[i * multiplicity + j]] =
		    i * multiplicity + j;
	    }
	}
	for (i = 0; i < dd->sizeZ; i++) {
	    dd->univ[i]->index = dd->invpermZ[i];
	}
    } else {
	permutation = ALLOC(int,dd->sizeZ);
	if (permutation == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	for (i = 0; i < dd->size; i++) {
	    for (j = 0; j < multiplicity; j++) {
		permutation[i * multiplicity + j] =
		    dd->invperm[i] * multiplicity + j;
	    }
	}
	for (i = dd->size * multiplicity; i < dd->sizeZ; i++) {
	    permutation[i] = i;
	}
	res = Cudd_zddShuffleHeap(dd, permutation);
	FREE(permutation);
	if (res == 0) return(0);
    }
    /* Copy and expand the variable group tree if it exists. */
    if (dd->treeZ != NULL) {
	Cudd_FreeZddTree(dd);
    }
    if (dd->tree != NULL) {
	dd->treeZ = Mtr_CopyTree(dd->tree, multiplicity);
	if (dd->treeZ == NULL) return(0);
    } else if (multiplicity > 1) {
	dd->treeZ = Mtr_InitGroupTree(0, dd->sizeZ);
	if (dd->treeZ == NULL) return(0);
	dd->treeZ->index = dd->invpermZ[0];
    }
    /* Create groups for the ZDD variables derived from the same BDD variable.
    */
    if (multiplicity > 1) {
	char *vmask, *lmask;

	vmask = ALLOC(char, dd->size);
	if (vmask == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	lmask =  ALLOC(char, dd->size);
	if (lmask == NULL) {
	    dd->errorCode = CUDD_MEMORY_OUT;
	    return(0);
	}
	for (i = 0; i < dd->size; i++) {
	    vmask[i] = lmask[i] = 0;
	}
	res = addMultiplicityGroups(dd,dd->treeZ,multiplicity,vmask,lmask);
	FREE(vmask);
	FREE(lmask);
	if (res == 0) return(0);
    }
    return(1);

} /* end of Cudd_zddVarsFromBddVars */


/**Function********************************************************************

  Synopsis    [Returns the ADD for constant c.]

  Description [Retrieves the ADD for constant c if it already
  exists, or creates a new ADD.  Returns a pointer to the
  ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addNewVar Cudd_addIthVar]

******************************************************************************/
DdNode *
Cudd_addConst(
  DdManager * dd,
  CUDD_VALUE_TYPE  c)
{
    return(cuddUniqueConst(dd,c));

} /* end of Cudd_addConst */


/**Function********************************************************************

  Synopsis    [Returns 1 if a DD node is not constant.]

  Description [Returns 1 if a DD node is not constant. This function is
  useful to test the results of Cudd_bddIteConstant, Cudd_addIteConstant,
  Cudd_addEvalConst. These results may be a special value signifying
  non-constant. In the other cases the macro Cudd_IsConstant can be used.]

  SideEffects [None]

  SeeAlso     [Cudd_IsConstant Cudd_bddIteConstant Cudd_addIteConstant
  Cudd_addEvalConst]

******************************************************************************/
int
Cudd_IsNonConstant(
  DdNode *f)
{
    return(f == DD_NON_CONSTANT || !Cudd_IsConstant(f));

} /* end of Cudd_IsNonConstant */


/**Function********************************************************************

  Synopsis    [Returns the start time of the manager.]

  Description [Returns the start time of the manager.  This is initially set
  to the number of milliseconds since the program started, but may be reset by
  the application.]

  SideEffects [None]

  SeeAlso     [Cudd_SetStartTime Cudd_ResetStartTime Cudd_ReadTimeLimit]

******************************************************************************/
unsigned long
Cudd_ReadStartTime(
  DdManager * unique)
{
    return unique->startTime;

} /* end of Cudd_ReadStartTime */


/**Function********************************************************************

  Synopsis    [Returns the time elapsed since the start time of the manager.]

  Description [Returns the time elapsed since the start time of the manager.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadStartTime Cudd_SetStartTime]

******************************************************************************/
unsigned long
Cudd_ReadElapsedTime(
  DdManager * unique)
{
  return util_cpu_time() - unique->startTime;

} /* end of Cudd_ReadElapsedTime */


/**Function********************************************************************

  Synopsis    [Sets the start time of the manager.]

  Description [Sets the start time of the manager.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadStartTime Cudd_ResetStartTime Cudd_ReadElapsedTime
  Cudd_SetTimeLimit]

******************************************************************************/
void
Cudd_SetStartTime(
  DdManager * unique,
  unsigned long st)
{
    unique->startTime = st;

} /* end of Cudd_SetStartTime */


/**Function********************************************************************

  Synopsis    [Resets the start time of the manager.]

  Description [Resets the start time of the manager.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadStartTime Cudd_SetStartTime Cudd_SetTimeLimit]

******************************************************************************/
void
Cudd_ResetStartTime(
  DdManager * unique)
{
    unique->startTime = util_cpu_time();

} /* end of Cudd_ResetStartTime */


/**Function********************************************************************

  Synopsis    [Returns the time limit for the manager.]

  Description [Returns the time limit for the manager.  This is initially set
  to a very large number, but may be reset by the application.]

  SideEffects [None]

  SeeAlso     [Cudd_SetTimeLimit Cudd_UpdateTimeLimit Cudd_UnsetTimeLimit
  Cudd_IncreaseTimeLimit Cudd_TimeLimited Cudd_ReadStartTime]

******************************************************************************/
unsigned long
Cudd_ReadTimeLimit(
  DdManager * unique)
{
    return unique->timeLimit;

} /* end of Cudd_ReadTimeLimit */


/**Function********************************************************************

  Synopsis    [Sets the time limit for the manager.]

  Description [Sets the time limit for the manager.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadTimeLimit Cudd_UnsetTimeLimit Cudd_UpdateTimeLimit
  Cudd_IncreaseTimeLimit Cudd_TimeLimited Cudd_SetStartTime]

******************************************************************************/
void
Cudd_SetTimeLimit(
  DdManager * unique,
  unsigned long tl)
{
    unique->timeLimit = tl;

} /* end of Cudd_SetTimeLimit */


/**Function********************************************************************

  Synopsis    [Updates the time limit for the manager.]

  Description [Updates the time limit for the manager by subtracting the
  elapsed time from it.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadTimeLimit Cudd_SetTimeLimit Cudd_UnsetTimeLimit
  Cudd_IncreaseTimeLimit Cudd_TimeLimited Cudd_SetStartTime]

******************************************************************************/
void
Cudd_UpdateTimeLimit(
  DdManager * unique)
{
    unsigned long elapsed;
    if (unique->timeLimit == ~0UL)
        return;
    elapsed = util_cpu_time() - unique->startTime;
    if (unique->timeLimit >= elapsed) {
        unique->timeLimit -= elapsed;
    } else {
        unique->timeLimit = 0;
    }

} /* end of Cudd_UpdateTimeLimit */


/**Function********************************************************************

  Synopsis    [Increases the time limit for the manager.]

  Description [Increases the time limit for the manager.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadTimeLimit Cudd_SetTimeLimit Cudd_UnsetTimeLimit
  Cudd_UpdateTimeLimit Cudd_TimeLimited Cudd_SetStartTime]

******************************************************************************/
void
Cudd_IncreaseTimeLimit(
  DdManager * unique,
  unsigned long increase)
{
    if (unique->timeLimit == ~0UL)
        unique->timeLimit = increase;
    else
        unique->timeLimit += increase;

} /* end of Cudd_IncreaseTimeLimit */


/**Function********************************************************************

  Synopsis    [Unsets the time limit for the manager.]

  Description [Unsets the time limit for the manager.  Actually, sets it to
  a very large value.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadTimeLimit Cudd_SetTimeLimit Cudd_UpdateTimeLimit
  Cudd_IncreaseTimeLimit Cudd_TimeLimited Cudd_SetStartTime]

******************************************************************************/
void
Cudd_UnsetTimeLimit(
  DdManager * unique)
{
    unique->timeLimit = ~0UL;

} /* end of Cudd_UnsetTimeLimit */


/**Function********************************************************************

  Synopsis    [Returns true if the time limit for the manager is set.]

  Description [Returns true if the time limit for the manager is set.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadTimeLimit Cudd_SetTimeLimit Cudd_UpdateTimeLimit
  Cudd_UnsetTimeLimit Cudd_IncreaseTimeLimit]

******************************************************************************/
int
Cudd_TimeLimited(
  DdManager * unique)
{
    return unique->timeLimit != ~0UL;

} /* end of Cudd_TimeLimited */


/**Function********************************************************************

  Synopsis    [Enables automatic dynamic reordering of BDDs and ADDs.]

  Description [Enables automatic dynamic reordering of BDDs and
  ADDs. Parameter method is used to determine the method used for
  reordering. If CUDD_REORDER_SAME is passed, the method is
  unchanged.]

  SideEffects [None]

  SeeAlso     [Cudd_AutodynDisable Cudd_ReorderingStatus
  Cudd_AutodynEnableZdd]

******************************************************************************/
void
Cudd_AutodynEnable(
  DdManager * unique,
  Cudd_ReorderingType  method)
{
    unique->autoDyn = 1;
    if (method != CUDD_REORDER_SAME) {
	unique->autoMethod = method;
    }
#ifndef DD_NO_DEATH_ROW
    /* If reordering is enabled, using the death row causes too many
    ** invocations. Hence, we shrink the death row to just one entry.
    */
    cuddClearDeathRow(unique);
    unique->deathRowDepth = 1;
    unique->deadMask = unique->deathRowDepth - 1;
    if ((unsigned) unique->nextDead > unique->deadMask) {
	unique->nextDead = 0;
    }
    unique->deathRow = REALLOC(DdNodePtr, unique->deathRow,
	unique->deathRowDepth);
#endif
    return;

} /* end of Cudd_AutodynEnable */


/**Function********************************************************************

  Synopsis    [Disables automatic dynamic reordering.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_AutodynEnable Cudd_ReorderingStatus
  Cudd_AutodynDisableZdd]

******************************************************************************/
void
Cudd_AutodynDisable(
  DdManager * unique)
{
    unique->autoDyn = 0;
    return;

} /* end of Cudd_AutodynDisable */


/**Function********************************************************************

  Synopsis    [Reports the status of automatic dynamic reordering of BDDs
  and ADDs.]

  Description [Reports the status of automatic dynamic reordering of
  BDDs and ADDs. Parameter method is set to the reordering method
  currently selected. Returns 1 if automatic reordering is enabled; 0
  otherwise.]

  SideEffects [Parameter method is set to the reordering method currently
  selected.]

  SeeAlso     [Cudd_AutodynEnable Cudd_AutodynDisable
  Cudd_ReorderingStatusZdd]

******************************************************************************/
int
Cudd_ReorderingStatus(
  DdManager * unique,
  Cudd_ReorderingType * method)
{
    *method = unique->autoMethod;
    return(unique->autoDyn);

} /* end of Cudd_ReorderingStatus */


/**Function********************************************************************

  Synopsis    [Enables automatic dynamic reordering of ZDDs.]

  Description [Enables automatic dynamic reordering of ZDDs. Parameter
  method is used to determine the method used for reordering ZDDs. If
  CUDD_REORDER_SAME is passed, the method is unchanged.]

  SideEffects [None]

  SeeAlso     [Cudd_AutodynDisableZdd Cudd_ReorderingStatusZdd
  Cudd_AutodynEnable]

******************************************************************************/
void
Cudd_AutodynEnableZdd(
  DdManager * unique,
  Cudd_ReorderingType method)
{
    unique->autoDynZ = 1;
    if (method != CUDD_REORDER_SAME) {
	unique->autoMethodZ = method;
    }
    return;

} /* end of Cudd_AutodynEnableZdd */


/**Function********************************************************************

  Synopsis    [Disables automatic dynamic reordering of ZDDs.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_AutodynEnableZdd Cudd_ReorderingStatusZdd
  Cudd_AutodynDisable]

******************************************************************************/
void
Cudd_AutodynDisableZdd(
  DdManager * unique)
{
    unique->autoDynZ = 0;
    return;

} /* end of Cudd_AutodynDisableZdd */


/**Function********************************************************************

  Synopsis    [Reports the status of automatic dynamic reordering of ZDDs.]

  Description [Reports the status of automatic dynamic reordering of
  ZDDs. Parameter method is set to the ZDD reordering method currently
  selected. Returns 1 if automatic reordering is enabled; 0
  otherwise.]

  SideEffects [Parameter method is set to the ZDD reordering method currently
  selected.]

  SeeAlso     [Cudd_AutodynEnableZdd Cudd_AutodynDisableZdd
  Cudd_ReorderingStatus]

******************************************************************************/
int
Cudd_ReorderingStatusZdd(
  DdManager * unique,
  Cudd_ReorderingType * method)
{
    *method = unique->autoMethodZ;
    return(unique->autoDynZ);

} /* end of Cudd_ReorderingStatusZdd */


/**Function********************************************************************

  Synopsis    [Tells whether the realignment of ZDD order to BDD order is
  enabled.]

  Description [Returns 1 if the realignment of ZDD order to BDD order is
  enabled; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddRealignEnable Cudd_zddRealignDisable
  Cudd_bddRealignEnable Cudd_bddRealignDisable]

******************************************************************************/
int
Cudd_zddRealignmentEnabled(
  DdManager * unique)
{
    return(unique->realign);

} /* end of Cudd_zddRealignmentEnabled */


/**Function********************************************************************

  Synopsis    [Enables realignment of ZDD order to BDD order.]

  Description [Enables realignment of the ZDD variable order to the
  BDD variable order after the BDDs and ADDs have been reordered.  The
  number of ZDD variables must be a multiple of the number of BDD
  variables for realignment to make sense. If this condition is not met,
  Cudd_ReduceHeap will return 0. Let <code>M</code> be the
  ratio of the two numbers. For the purpose of realignment, the ZDD
  variables from <code>M*i</code> to <code>(M+1)*i-1</code> are
  reagarded as corresponding to BDD variable <code>i</code>. Realignment
  is initially disabled.]

  SideEffects [None]

  SeeAlso     [Cudd_ReduceHeap Cudd_zddRealignDisable
  Cudd_zddRealignmentEnabled Cudd_bddRealignDisable
  Cudd_bddRealignmentEnabled]

******************************************************************************/
void
Cudd_zddRealignEnable(
  DdManager * unique)
{
    unique->realign = 1;
    return;

} /* end of Cudd_zddRealignEnable */


/**Function********************************************************************

  Synopsis    [Disables realignment of ZDD order to BDD order.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddRealignEnable Cudd_zddRealignmentEnabled
  Cudd_bddRealignEnable Cudd_bddRealignmentEnabled]

******************************************************************************/
void
Cudd_zddRealignDisable(
  DdManager * unique)
{
    unique->realign = 0;
    return;

} /* end of Cudd_zddRealignDisable */


/**Function********************************************************************

  Synopsis    [Tells whether the realignment of BDD order to ZDD order is
  enabled.]

  Description [Returns 1 if the realignment of BDD order to ZDD order is
  enabled; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddRealignEnable Cudd_bddRealignDisable
  Cudd_zddRealignEnable Cudd_zddRealignDisable]

******************************************************************************/
int
Cudd_bddRealignmentEnabled(
  DdManager * unique)
{
    return(unique->realignZ);

} /* end of Cudd_bddRealignmentEnabled */


/**Function********************************************************************

  Synopsis    [Enables realignment of BDD order to ZDD order.]

  Description [Enables realignment of the BDD variable order to the
  ZDD variable order after the ZDDs have been reordered.  The
  number of ZDD variables must be a multiple of the number of BDD
  variables for realignment to make sense. If this condition is not met,
  Cudd_zddReduceHeap will return 0. Let <code>M</code> be the
  ratio of the two numbers. For the purpose of realignment, the ZDD
  variables from <code>M*i</code> to <code>(M+1)*i-1</code> are
  reagarded as corresponding to BDD variable <code>i</code>. Realignment
  is initially disabled.]

  SideEffects [None]

  SeeAlso     [Cudd_zddReduceHeap Cudd_bddRealignDisable
  Cudd_bddRealignmentEnabled Cudd_zddRealignDisable
  Cudd_zddRealignmentEnabled]

******************************************************************************/
void
Cudd_bddRealignEnable(
  DdManager * unique)
{
    unique->realignZ = 1;
    return;

} /* end of Cudd_bddRealignEnable */


/**Function********************************************************************

  Synopsis    [Disables realignment of ZDD order to BDD order.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_bddRealignEnable Cudd_bddRealignmentEnabled
  Cudd_zddRealignEnable Cudd_zddRealignmentEnabled]

******************************************************************************/
void
Cudd_bddRealignDisable(
  DdManager * unique)
{
    unique->realignZ = 0;
    return;

} /* end of Cudd_bddRealignDisable */


/**Function********************************************************************

  Synopsis    [Returns the one constant of the manager.]

  Description [Returns the one constant of the manager. The one
  constant is common to ADDs and BDDs.]

  SideEffects [None]

  SeeAlso [Cudd_ReadZero Cudd_ReadLogicZero Cudd_ReadZddOne]

******************************************************************************/
DdNode *
Cudd_ReadOne(
  DdManager * dd)
{
    return(dd->one);

} /* end of Cudd_ReadOne */


/**Function********************************************************************

  Synopsis    [Returns the ZDD for the constant 1 function.]

  Description [Returns the ZDD for the constant 1 function.
  The representation of the constant 1 function as a ZDD depends on
  how many variables it (nominally) depends on. The index of the
  topmost variable in the support is given as argument <code>i</code>.]

  SideEffects [None]

  SeeAlso [Cudd_ReadOne]

******************************************************************************/
DdNode *
Cudd_ReadZddOne(
  DdManager * dd,
  int  i)
{
    if (i < 0)
	return(NULL);
    return(i < dd->sizeZ ? dd->univ[i] : DD_ONE(dd));

} /* end of Cudd_ReadZddOne */



/**Function********************************************************************

  Synopsis    [Returns the zero constant of the manager.]

  Description [Returns the zero constant of the manager. The zero
  constant is the arithmetic zero, rather than the logic zero. The
  latter is the complement of the one constant.]

  SideEffects [None]

  SeeAlso [Cudd_ReadOne Cudd_ReadLogicZero]

******************************************************************************/
DdNode *
Cudd_ReadZero(
  DdManager * dd)
{
    return(DD_ZERO(dd));

} /* end of Cudd_ReadZero */


/**Function********************************************************************

  Synopsis    [Returns the logic zero constant of the manager.]

  Description [Returns the zero constant of the manager. The logic zero
  constant is the complement of the one constant, and is distinct from
  the arithmetic zero.]

  SideEffects [None]

  SeeAlso [Cudd_ReadOne Cudd_ReadZero]

******************************************************************************/
DdNode *
Cudd_ReadLogicZero(
  DdManager * dd)
{
    return(Cudd_Not(DD_ONE(dd)));

} /* end of Cudd_ReadLogicZero */


/**Function********************************************************************

  Synopsis    [Reads the plus-infinity constant from the manager.]

  Description []

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_ReadPlusInfinity(
  DdManager * dd)
{
    return(dd->plusinfinity);

} /* end of Cudd_ReadPlusInfinity */


/**Function********************************************************************

  Synopsis    [Reads the minus-infinity constant from the manager.]

  Description []

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_ReadMinusInfinity(
  DdManager * dd)
{
    return(dd->minusinfinity);

} /* end of Cudd_ReadMinusInfinity */


/**Function********************************************************************

  Synopsis    [Reads the background constant of the manager.]

  Description []

  SideEffects [None]

******************************************************************************/
DdNode *
Cudd_ReadBackground(
  DdManager * dd)
{
    return(dd->background);

} /* end of Cudd_ReadBackground */


/**Function********************************************************************

  Synopsis    [Sets the background constant of the manager.]

  Description [Sets the background constant of the manager. It assumes
  that the DdNode pointer bck is already referenced.]

  SideEffects [None]

******************************************************************************/
void
Cudd_SetBackground(
  DdManager * dd,
  DdNode * bck)
{
    dd->background = bck;

} /* end of Cudd_SetBackground */


/**Function********************************************************************

  Synopsis    [Reads the number of slots in the cache.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadCacheUsedSlots]

******************************************************************************/
unsigned int
Cudd_ReadCacheSlots(
  DdManager * dd)
{
    return(dd->cacheSlots);

} /* end of Cudd_ReadCacheSlots */


/**Function********************************************************************

  Synopsis    [Reads the fraction of used slots in the cache.]

  Description [Reads the fraction of used slots in the cache. The unused
  slots are those in which no valid data is stored. Garbage collection,
  variable reordering, and cache resizing may cause used slots to become
  unused.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadCacheSlots]

******************************************************************************/
double
Cudd_ReadCacheUsedSlots(
  DdManager * dd)
{
    unsigned long used = 0;
    int slots = dd->cacheSlots;
    DdCache *cache = dd->cache;
    int i;

    for (i = 0; i < slots; i++) {
	used += cache[i].h != 0;
    }

    return((double)used / (double) dd->cacheSlots);

} /* end of Cudd_ReadCacheUsedSlots */


/**Function********************************************************************

  Synopsis    [Returns the number of cache look-ups.]

  Description [Returns the number of cache look-ups.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadCacheHits]

******************************************************************************/
double
Cudd_ReadCacheLookUps(
  DdManager * dd)
{
    return(dd->cacheHits + dd->cacheMisses +
	   dd->totCachehits + dd->totCacheMisses);

} /* end of Cudd_ReadCacheLookUps */


/**Function********************************************************************

  Synopsis    [Returns the number of cache hits.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadCacheLookUps]

******************************************************************************/
double
Cudd_ReadCacheHits(
  DdManager * dd)
{
    return(dd->cacheHits + dd->totCachehits);

} /* end of Cudd_ReadCacheHits */


/**Function********************************************************************

  Synopsis    [Returns the number of recursive calls.]

  Description [Returns the number of recursive calls if the package is
  compiled with DD_COUNT defined.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
double
Cudd_ReadRecursiveCalls(
  DdManager * dd)
{
#ifdef DD_COUNT
    return(dd->recursiveCalls);
#else
    return(-1.0);
#endif

} /* end of Cudd_ReadRecursiveCalls */



/**Function********************************************************************

  Synopsis    [Reads the hit rate that causes resizinig of the computed
  table.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetMinHit]

******************************************************************************/
unsigned int
Cudd_ReadMinHit(
  DdManager * dd)
{
    /* Internally, the package manipulates the ratio of hits to
    ** misses instead of the ratio of hits to accesses. */
    return((unsigned int) (0.5 + 100 * dd->minHit / (1 + dd->minHit)));

} /* end of Cudd_ReadMinHit */


/**Function********************************************************************

  Synopsis    [Sets the hit rate that causes resizinig of the computed
  table.]

  Description [Sets the minHit parameter of the manager. This
  parameter controls the resizing of the computed table. If the hit
  rate is larger than the specified value, and the cache is not
  already too large, then its size is doubled.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMinHit]

******************************************************************************/
void
Cudd_SetMinHit(
  DdManager * dd,
  unsigned int hr)
{
    /* Internally, the package manipulates the ratio of hits to
    ** misses instead of the ratio of hits to accesses. */
    dd->minHit = (double) hr / (100.0 - (double) hr);

} /* end of Cudd_SetMinHit */


/**Function********************************************************************

  Synopsis    [Reads the looseUpTo parameter of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetLooseUpTo Cudd_ReadMinHit Cudd_ReadMinDead]

******************************************************************************/
unsigned int
Cudd_ReadLooseUpTo(
  DdManager * dd)
{
    return(dd->looseUpTo);

} /* end of Cudd_ReadLooseUpTo */


/**Function********************************************************************

  Synopsis    [Sets the looseUpTo parameter of the manager.]

  Description [Sets the looseUpTo parameter of the manager. This
  parameter of the manager controls the threshold beyond which no fast
  growth of the unique table is allowed. The threshold is given as a
  number of slots. If the value passed to this function is 0, the
  function determines a suitable value based on the available memory.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadLooseUpTo Cudd_SetMinHit]

******************************************************************************/
void
Cudd_SetLooseUpTo(
  DdManager * dd,
  unsigned int lut)
{
    if (lut == 0) {
	unsigned long datalimit = getSoftDataLimit();
	lut = (unsigned int) (datalimit / (sizeof(DdNode) *
					   DD_MAX_LOOSE_FRACTION));
    }
    dd->looseUpTo = lut;

} /* end of Cudd_SetLooseUpTo */


/**Function********************************************************************

  Synopsis    [Returns the soft limit for the cache size.]

  Description [Returns the soft limit for the cache size.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxCacheHard]

******************************************************************************/
unsigned int
Cudd_ReadMaxCache(
  DdManager * dd)
{
    return(2 * dd->cacheSlots + dd->cacheSlack);

} /* end of Cudd_ReadMaxCache */


/**Function********************************************************************

  Synopsis    [Reads the maxCacheHard parameter of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetMaxCacheHard Cudd_ReadMaxCache]

******************************************************************************/
unsigned int
Cudd_ReadMaxCacheHard(
  DdManager * dd)
{
    return(dd->maxCacheHard);

} /* end of Cudd_ReadMaxCache */


/**Function********************************************************************

  Synopsis    [Sets the maxCacheHard parameter of the manager.]

  Description [Sets the maxCacheHard parameter of the manager. The
  cache cannot grow larger than maxCacheHard entries. This parameter
  allows an application to control the trade-off of memory versus
  speed. If the value passed to this function is 0, the function
  determines a suitable maximum cache size based on the available memory.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxCacheHard Cudd_SetMaxCache]

******************************************************************************/
void
Cudd_SetMaxCacheHard(
  DdManager * dd,
  unsigned int mc)
{
    if (mc == 0) {
	unsigned long datalimit = getSoftDataLimit();
	mc = (unsigned int) (datalimit / (sizeof(DdCache) *
					  DD_MAX_CACHE_FRACTION));
    }
    dd->maxCacheHard = mc;

} /* end of Cudd_SetMaxCacheHard */


/**Function********************************************************************

  Synopsis    [Returns the number of BDD variables in existance.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadZddSize]

******************************************************************************/
int
Cudd_ReadSize(
  DdManager * dd)
{
    return(dd->size);

} /* end of Cudd_ReadSize */


/**Function********************************************************************

  Synopsis    [Returns the number of ZDD variables in existance.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadSize]

******************************************************************************/
int
Cudd_ReadZddSize(
  DdManager * dd)
{
    return(dd->sizeZ);

} /* end of Cudd_ReadZddSize */


/**Function********************************************************************

  Synopsis    [Returns the total number of slots of the unique table.]

  Description [Returns the total number of slots of the unique table.
  This number ismainly for diagnostic purposes.]

  SideEffects [None]

******************************************************************************/
unsigned int
Cudd_ReadSlots(
  DdManager * dd)
{
    return(dd->slots);

} /* end of Cudd_ReadSlots */


/**Function********************************************************************

  Synopsis    [Reads the fraction of used slots in the unique table.]

  Description [Reads the fraction of used slots in the unique
  table. The unused slots are those in which no valid data is
  stored. Garbage collection, variable reordering, and subtable
  resizing may cause used slots to become unused.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadSlots]

******************************************************************************/
double
Cudd_ReadUsedSlots(
  DdManager * dd)
{
    unsigned long used = 0;
    int i, j;
    int size = dd->size;
    DdNodePtr *nodelist;
    DdSubtable *subtable;
    DdNode *node;
    DdNode *sentinel = &(dd->sentinel);

    /* Scan each BDD/ADD subtable. */
    for (i = 0; i < size; i++) {
	subtable = &(dd->subtables[i]);
	nodelist = subtable->nodelist;
	for (j = 0; (unsigned) j < subtable->slots; j++) {
	    node = nodelist[j];
	    if (node != sentinel) {
		used++;
	    }
	}
    }

    /* Scan the ZDD subtables. */
    size = dd->sizeZ;

    for (i = 0; i < size; i++) {
	subtable = &(dd->subtableZ[i]);
	nodelist = subtable->nodelist;
	for (j = 0; (unsigned) j < subtable->slots; j++) {
	    node = nodelist[j];
	    if (node != NULL) {
		used++;
	    }
	}
    }

    /* Constant table. */
    subtable = &(dd->constants);
    nodelist = subtable->nodelist;
    for (j = 0; (unsigned) j < subtable->slots; j++) {
	node = nodelist[j];
	if (node != NULL) {
	    used++;
	}
    }

    return((double)used / (double) dd->slots);

} /* end of Cudd_ReadUsedSlots */


/**Function********************************************************************

  Synopsis    [Computes the expected fraction of used slots in the unique
  table.]

  Description [Computes the fraction of slots in the unique table that
  should be in use. This expected value is based on the assumption
  that the hash function distributes the keys randomly; it can be
  compared with the result of Cudd_ReadUsedSlots to monitor the
  performance of the unique table hash function.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadSlots Cudd_ReadUsedSlots]

******************************************************************************/
double
Cudd_ExpectedUsedSlots(
  DdManager * dd)
{
    int i;
    int size = dd->size;
    DdSubtable *subtable;
    double empty = 0.0;

    /* To each subtable we apply the corollary to Theorem 8.5 (occupancy
    ** distribution) from Sedgewick and Flajolet's Analysis of Algorithms.
    ** The corollary says that for a table with M buckets and a load ratio
    ** of r, the expected number of empty buckets is asymptotically given
    ** by M * exp(-r).
    */

    /* Scan each BDD/ADD subtable. */
    for (i = 0; i < size; i++) {
	subtable = &(dd->subtables[i]);
	empty += (double) subtable->slots *
	    exp(-(double) subtable->keys / (double) subtable->slots);
    }

    /* Scan the ZDD subtables. */
    size = dd->sizeZ;

    for (i = 0; i < size; i++) {
	subtable = &(dd->subtableZ[i]);
	empty += (double) subtable->slots *
	    exp(-(double) subtable->keys / (double) subtable->slots);
    }

    /* Constant table. */
    subtable = &(dd->constants);
    empty += (double) subtable->slots *
	exp(-(double) subtable->keys / (double) subtable->slots);

    return(1.0 - empty / (double) dd->slots);

} /* end of Cudd_ExpectedUsedSlots */


/**Function********************************************************************

  Synopsis    [Returns the number of nodes in the unique table.]

  Description [Returns the total number of nodes currently in the unique
  table, including the dead nodes.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadDead]

******************************************************************************/
unsigned int
Cudd_ReadKeys(
  DdManager * dd)
{
    return(dd->keys);

} /* end of Cudd_ReadKeys */


/**Function********************************************************************

  Synopsis    [Returns the number of dead nodes in the unique table.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadKeys]

******************************************************************************/
unsigned int
Cudd_ReadDead(
  DdManager * dd)
{
    return(dd->dead);

} /* end of Cudd_ReadDead */


/**Function********************************************************************

  Synopsis    [Reads the minDead parameter of the manager.]

  Description [Reads the minDead parameter of the manager. The minDead
  parameter is used by the package to decide whether to collect garbage
  or resize a subtable of the unique table when the subtable becomes
  too full. The application can indirectly control the value of minDead
  by setting the looseUpTo parameter.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadDead Cudd_ReadLooseUpTo Cudd_SetLooseUpTo]

******************************************************************************/
unsigned int
Cudd_ReadMinDead(
  DdManager * dd)
{
    return(dd->minDead);

} /* end of Cudd_ReadMinDead */


/**Function********************************************************************

  Synopsis    [Returns the number of times reordering has occurred.]

  Description [Returns the number of times reordering has occurred in the
  manager. The number includes both the calls to Cudd_ReduceHeap from
  the application program and those automatically performed by the
  package. However, calls that do not even initiate reordering are not
  counted. A call may not initiate reordering if there are fewer than
  minsize live nodes in the manager, or if CUDD_REORDER_NONE is specified
  as reordering method. The calls to Cudd_ShuffleHeap are not counted.]

  SideEffects [None]

  SeeAlso [Cudd_ReduceHeap Cudd_ReadReorderingTime]

******************************************************************************/
unsigned int
Cudd_ReadReorderings(
  DdManager * dd)
{
    return(dd->reorderings);

} /* end of Cudd_ReadReorderings */


/**Function********************************************************************

  Synopsis    [Returns the maximum number of times reordering may be invoked.]

  Description [Returns the maximum number of times reordering may be invoked in
  this manager.]

  SideEffects [None]

  SeeAlso [Cudd_ReadReorderings Cudd_SetMaxReorderings Cudd_ReduceHeap]

******************************************************************************/
unsigned int
Cudd_ReadMaxReorderings(
  DdManager * dd)
{
    return(dd->maxReorderings);

} /* end of Cudd_ReadMaxReorderings */


/**Function********************************************************************

  Synopsis    [Sets the maximum number of times reordering may be invoked.]

  Description [Sets the maximum number of times reordering may be invoked in
  this manager.  The default value is (practically) infinite.]

  SideEffects [None]

  SeeAlso [Cudd_ReadReorderings Cudd_ReadMaxReorderings Cudd_ReduceHeap]

******************************************************************************/
void
Cudd_SetMaxReorderings(
  DdManager * dd, unsigned int mr)
{
    dd->maxReorderings = mr;

} /* end of Cudd_SetMaxReorderings */


/**Function********************************************************************

  Synopsis    [Returns the time spent in reordering.]

  Description [Returns the number of milliseconds spent reordering
  variables since the manager was initialized. The time spent in collecting
  garbage before reordering is included.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadReorderings]

******************************************************************************/
long
Cudd_ReadReorderingTime(
  DdManager * dd)
{
    return(dd->reordTime);

} /* end of Cudd_ReadReorderingTime */


/**Function********************************************************************

  Synopsis    [Returns the number of times garbage collection has occurred.]

  Description [Returns the number of times garbage collection has
  occurred in the manager. The number includes both the calls from
  reordering procedures and those caused by requests to create new
  nodes.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadGarbageCollectionTime]

******************************************************************************/
int
Cudd_ReadGarbageCollections(
  DdManager * dd)
{
    return(dd->garbageCollections);

} /* end of Cudd_ReadGarbageCollections */


/**Function********************************************************************

  Synopsis    [Returns the time spent in garbage collection.]

  Description [Returns the number of milliseconds spent doing garbage
  collection since the manager was initialized.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadGarbageCollections]

******************************************************************************/
long
Cudd_ReadGarbageCollectionTime(
  DdManager * dd)
{
    return(dd->GCTime);

} /* end of Cudd_ReadGarbageCollectionTime */


/**Function********************************************************************

  Synopsis    [Returns the number of nodes freed.]

  Description [Returns the number of nodes returned to the free list if the
  keeping of this statistic is enabled; -1 otherwise. This statistic is
  enabled only if the package is compiled with DD_STATS defined.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadNodesDropped]

******************************************************************************/
double
Cudd_ReadNodesFreed(
  DdManager * dd)
{
#ifdef DD_STATS
    return(dd->nodesFreed);
#else
    return(-1.0);
#endif

} /* end of Cudd_ReadNodesFreed */


/**Function********************************************************************

  Synopsis    [Returns the number of nodes dropped.]

  Description [Returns the number of nodes killed by dereferencing if the
  keeping of this statistic is enabled; -1 otherwise. This statistic is
  enabled only if the package is compiled with DD_STATS defined.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadNodesFreed]

******************************************************************************/
double
Cudd_ReadNodesDropped(
  DdManager * dd)
{
#ifdef DD_STATS
    return(dd->nodesDropped);
#else
    return(-1.0);
#endif

} /* end of Cudd_ReadNodesDropped */


/**Function********************************************************************

  Synopsis    [Returns the number of look-ups in the unique table.]

  Description [Returns the number of look-ups in the unique table if the
  keeping of this statistic is enabled; -1 otherwise. This statistic is
  enabled only if the package is compiled with DD_UNIQUE_PROFILE defined.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadUniqueLinks]

******************************************************************************/
double
Cudd_ReadUniqueLookUps(
  DdManager * dd)
{
#ifdef DD_UNIQUE_PROFILE
    return(dd->uniqueLookUps);
#else
    return(-1.0);
#endif

} /* end of Cudd_ReadUniqueLookUps */


/**Function********************************************************************

  Synopsis    [Returns the number of links followed in the unique table.]

  Description [Returns the number of links followed during look-ups in the
  unique table if the keeping of this statistic is enabled; -1 otherwise.
  If an item is found in the first position of its collision list, the
  number of links followed is taken to be 0. If it is in second position,
  the number of links is 1, and so on. This statistic is enabled only if
  the package is compiled with DD_UNIQUE_PROFILE defined.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadUniqueLookUps]

******************************************************************************/
double
Cudd_ReadUniqueLinks(
  DdManager * dd)
{
#ifdef DD_UNIQUE_PROFILE
    return(dd->uniqueLinks);
#else
    return(-1.0);
#endif

} /* end of Cudd_ReadUniqueLinks */


/**Function********************************************************************

  Synopsis    [Reads the siftMaxVar parameter of the manager.]

  Description [Reads the siftMaxVar parameter of the manager. This
  parameter gives the maximum number of variables that will be sifted
  for each invocation of sifting.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadSiftMaxSwap Cudd_SetSiftMaxVar]

******************************************************************************/
int
Cudd_ReadSiftMaxVar(
  DdManager * dd)
{
    return(dd->siftMaxVar);

} /* end of Cudd_ReadSiftMaxVar */


/**Function********************************************************************

  Synopsis    [Sets the siftMaxVar parameter of the manager.]

  Description [Sets the siftMaxVar parameter of the manager. This
  parameter gives the maximum number of variables that will be sifted
  for each invocation of sifting.]

  SideEffects [None]

  SeeAlso     [Cudd_SetSiftMaxSwap Cudd_ReadSiftMaxVar]

******************************************************************************/
void
Cudd_SetSiftMaxVar(
  DdManager * dd,
  int  smv)
{
    dd->siftMaxVar = smv;

} /* end of Cudd_SetSiftMaxVar */


/**Function********************************************************************

  Synopsis    [Reads the siftMaxSwap parameter of the manager.]

  Description [Reads the siftMaxSwap parameter of the manager. This
  parameter gives the maximum number of swaps that will be attempted
  for each invocation of sifting. The real number of swaps may exceed
  the set limit because the package will always complete the sifting
  of the variable that causes the limit to be reached.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadSiftMaxVar Cudd_SetSiftMaxSwap]

******************************************************************************/
int
Cudd_ReadSiftMaxSwap(
  DdManager * dd)
{
    return(dd->siftMaxSwap);

} /* end of Cudd_ReadSiftMaxSwap */


/**Function********************************************************************

  Synopsis    [Sets the siftMaxSwap parameter of the manager.]

  Description [Sets the siftMaxSwap parameter of the manager. This
  parameter gives the maximum number of swaps that will be attempted
  for each invocation of sifting. The real number of swaps may exceed
  the set limit because the package will always complete the sifting
  of the variable that causes the limit to be reached.]

  SideEffects [None]

  SeeAlso     [Cudd_SetSiftMaxVar Cudd_ReadSiftMaxSwap]

******************************************************************************/
void
Cudd_SetSiftMaxSwap(
  DdManager * dd,
  int  sms)
{
    dd->siftMaxSwap = sms;

} /* end of Cudd_SetSiftMaxSwap */


/**Function********************************************************************

  Synopsis    [Reads the maxGrowth parameter of the manager.]

  Description [Reads the maxGrowth parameter of the manager.  This
  parameter determines how much the number of nodes can grow during
  sifting of a variable.  Overall, sifting never increases the size of
  the decision diagrams.  This parameter only refers to intermediate
  results.  A lower value will speed up sifting, possibly at the
  expense of quality.]

  SideEffects [None]

  SeeAlso     [Cudd_SetMaxGrowth Cudd_ReadMaxGrowthAlternate]

******************************************************************************/
double
Cudd_ReadMaxGrowth(
  DdManager * dd)
{
    return(dd->maxGrowth);

} /* end of Cudd_ReadMaxGrowth */


/**Function********************************************************************

  Synopsis    [Sets the maxGrowth parameter of the manager.]

  Description [Sets the maxGrowth parameter of the manager.  This
  parameter determines how much the number of nodes can grow during
  sifting of a variable.  Overall, sifting never increases the size of
  the decision diagrams.  This parameter only refers to intermediate
  results.  A lower value will speed up sifting, possibly at the
  expense of quality.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxGrowth Cudd_SetMaxGrowthAlternate]

******************************************************************************/
void
Cudd_SetMaxGrowth(
  DdManager * dd,
  double mg)
{
    dd->maxGrowth = mg;

} /* end of Cudd_SetMaxGrowth */


/**Function********************************************************************

  Synopsis    [Reads the maxGrowthAlt parameter of the manager.]

  Description [Reads the maxGrowthAlt parameter of the manager.  This
  parameter is analogous to the maxGrowth paramter, and is used every
  given number of reorderings instead of maxGrowth.  The number of
  reorderings is set with Cudd_SetReorderingCycle.  If the number of
  reorderings is 0 (default) maxGrowthAlt is never used.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxGrowth Cudd_SetMaxGrowthAlternate
  Cudd_SetReorderingCycle Cudd_ReadReorderingCycle]

******************************************************************************/
double
Cudd_ReadMaxGrowthAlternate(
  DdManager * dd)
{
    return(dd->maxGrowthAlt);

} /* end of Cudd_ReadMaxGrowthAlternate */


/**Function********************************************************************

  Synopsis    [Sets the maxGrowthAlt parameter of the manager.]

  Description [Sets the maxGrowthAlt parameter of the manager.  This
  parameter is analogous to the maxGrowth paramter, and is used every
  given number of reorderings instead of maxGrowth.  The number of
  reorderings is set with Cudd_SetReorderingCycle.  If the number of
  reorderings is 0 (default) maxGrowthAlt is never used.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxGrowthAlternate Cudd_SetMaxGrowth
  Cudd_SetReorderingCycle Cudd_ReadReorderingCycle]

******************************************************************************/
void
Cudd_SetMaxGrowthAlternate(
  DdManager * dd,
  double mg)
{
    dd->maxGrowthAlt = mg;

} /* end of Cudd_SetMaxGrowthAlternate */


/**Function********************************************************************

  Synopsis    [Reads the reordCycle parameter of the manager.]

  Description [Reads the reordCycle parameter of the manager.  This
  parameter determines how often the alternate threshold on maximum
  growth is used in reordering.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxGrowthAlternate Cudd_SetMaxGrowthAlternate
  Cudd_SetReorderingCycle]

******************************************************************************/
int
Cudd_ReadReorderingCycle(
  DdManager * dd)
{
    return(dd->reordCycle);

} /* end of Cudd_ReadReorderingCycle */


/**Function********************************************************************

  Synopsis    [Sets the reordCycle parameter of the manager.]

  Description [Sets the reordCycle parameter of the manager.  This
  parameter determines how often the alternate threshold on maximum
  growth is used in reordering.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadMaxGrowthAlternate Cudd_SetMaxGrowthAlternate
  Cudd_ReadReorderingCycle]

******************************************************************************/
void
Cudd_SetReorderingCycle(
  DdManager * dd,
  int cycle)
{
    dd->reordCycle = cycle;

} /* end of Cudd_SetReorderingCycle */


/**Function********************************************************************

  Synopsis    [Returns the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetTree Cudd_FreeTree Cudd_ReadZddTree]

******************************************************************************/
MtrNode *
Cudd_ReadTree(
  DdManager * dd)
{
    return(dd->tree);

} /* end of Cudd_ReadTree */


/**Function********************************************************************

  Synopsis    [Sets the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_FreeTree Cudd_ReadTree Cudd_SetZddTree]

******************************************************************************/
void
Cudd_SetTree(
  DdManager * dd,
  MtrNode * tree)
{
    if (dd->tree != NULL) {
	Mtr_FreeTree(dd->tree);
    }
    dd->tree = tree;
    if (tree == NULL) return;

    fixVarTree(tree, dd->perm, dd->size);
    return;

} /* end of Cudd_SetTree */


/**Function********************************************************************

  Synopsis    [Frees the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetTree Cudd_ReadTree Cudd_FreeZddTree]

******************************************************************************/
void
Cudd_FreeTree(
  DdManager * dd)
{
    if (dd->tree != NULL) {
	Mtr_FreeTree(dd->tree);
	dd->tree = NULL;
    }
    return;

} /* end of Cudd_FreeTree */


/**Function********************************************************************

  Synopsis    [Returns the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetZddTree Cudd_FreeZddTree Cudd_ReadTree]

******************************************************************************/
MtrNode *
Cudd_ReadZddTree(
  DdManager * dd)
{
    return(dd->treeZ);

} /* end of Cudd_ReadZddTree */


/**Function********************************************************************

  Synopsis    [Sets the ZDD variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_FreeZddTree Cudd_ReadZddTree Cudd_SetTree]

******************************************************************************/
void
Cudd_SetZddTree(
  DdManager * dd,
  MtrNode * tree)
{
    if (dd->treeZ != NULL) {
	Mtr_FreeTree(dd->treeZ);
    }
    dd->treeZ = tree;
    if (tree == NULL) return;

    fixVarTree(tree, dd->permZ, dd->sizeZ);
    return;

} /* end of Cudd_SetZddTree */


/**Function********************************************************************

  Synopsis    [Frees the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetZddTree Cudd_ReadZddTree Cudd_FreeTree]

******************************************************************************/
void
Cudd_FreeZddTree(
  DdManager * dd)
{
    if (dd->treeZ != NULL) {
	Mtr_FreeTree(dd->treeZ);
	dd->treeZ = NULL;
    }
    return;

} /* end of Cudd_FreeZddTree */


/**Function********************************************************************

  Synopsis    [Returns the index of the node.]

  Description [Returns the index of the node. The node pointer can be
  either regular or complemented.]

  SideEffects [None]

  SeeAlso [Cudd_ReadIndex]

******************************************************************************/
unsigned int
Cudd_NodeReadIndex(
  DdNode * node)
{
    return((unsigned int) Cudd_Regular(node)->index);

} /* end of Cudd_NodeReadIndex */


/**Function********************************************************************

  Synopsis    [Returns the current position of the i-th variable in the
  order.]

  Description [Returns the current position of the i-th variable in
  the order. If the index is CUDD_CONST_INDEX, returns
  CUDD_CONST_INDEX; otherwise, if the index is out of bounds returns
  -1.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadInvPerm Cudd_ReadPermZdd]

******************************************************************************/
int
Cudd_ReadPerm(
  DdManager * dd,
  int  i)
{
    if (i == CUDD_CONST_INDEX) return(CUDD_CONST_INDEX);
    if (i < 0 || i >= dd->size) return(-1);
    return(dd->perm[i]);

} /* end of Cudd_ReadPerm */


/**Function********************************************************************

  Synopsis    [Returns the current position of the i-th ZDD variable in the
  order.]

  Description [Returns the current position of the i-th ZDD variable
  in the order. If the index is CUDD_CONST_INDEX, returns
  CUDD_CONST_INDEX; otherwise, if the index is out of bounds returns
  -1.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadInvPermZdd Cudd_ReadPerm]

******************************************************************************/
int
Cudd_ReadPermZdd(
  DdManager * dd,
  int  i)
{
    if (i == CUDD_CONST_INDEX) return(CUDD_CONST_INDEX);
    if (i < 0 || i >= dd->sizeZ) return(-1);
    return(dd->permZ[i]);

} /* end of Cudd_ReadPermZdd */


/**Function********************************************************************

  Synopsis    [Returns the index of the variable currently in the i-th
  position of the order.]

  Description [Returns the index of the variable currently in the i-th
  position of the order. If the index is CUDD_CONST_INDEX, returns
  CUDD_CONST_INDEX; otherwise, if the index is out of bounds returns -1.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadPerm Cudd_ReadInvPermZdd]

******************************************************************************/
int
Cudd_ReadInvPerm(
  DdManager * dd,
  int  i)
{
    if (i == CUDD_CONST_INDEX) return(CUDD_CONST_INDEX);
    if (i < 0 || i >= dd->size) return(-1);
    return(dd->invperm[i]);

} /* end of Cudd_ReadInvPerm */


/**Function********************************************************************

  Synopsis    [Returns the index of the ZDD variable currently in the i-th
  position of the order.]

  Description [Returns the index of the ZDD variable currently in the
  i-th position of the order. If the index is CUDD_CONST_INDEX, returns
  CUDD_CONST_INDEX; otherwise, if the index is out of bounds returns -1.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadPerm Cudd_ReadInvPermZdd]

******************************************************************************/
int
Cudd_ReadInvPermZdd(
  DdManager * dd,
  int  i)
{
    if (i == CUDD_CONST_INDEX) return(CUDD_CONST_INDEX);
    if (i < 0 || i >= dd->sizeZ) return(-1);
    return(dd->invpermZ[i]);

} /* end of Cudd_ReadInvPermZdd */


/**Function********************************************************************

  Synopsis    [Returns the i-th element of the vars array.]

  Description [Returns the i-th element of the vars array if it falls
  within the array bounds; NULL otherwise. If i is the index of an
  existing variable, this function produces the same result as
  Cudd_bddIthVar. However, if the i-th var does not exist yet,
  Cudd_bddIthVar will create it, whereas Cudd_ReadVars will not.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIthVar]

******************************************************************************/
DdNode *
Cudd_ReadVars(
  DdManager * dd,
  int  i)
{
    if (i < 0 || i > dd->size) return(NULL);
    return(dd->vars[i]);

} /* end of Cudd_ReadVars */


/**Function********************************************************************

  Synopsis    [Reads the epsilon parameter of the manager.]

  Description [Reads the epsilon parameter of the manager. The epsilon
  parameter control the comparison between floating point numbers.]

  SideEffects [None]

  SeeAlso     [Cudd_SetEpsilon]

******************************************************************************/
CUDD_VALUE_TYPE
Cudd_ReadEpsilon(
  DdManager * dd)
{
    return(dd->epsilon);

} /* end of Cudd_ReadEpsilon */


/**Function********************************************************************

  Synopsis    [Sets the epsilon parameter of the manager to ep.]

  Description [Sets the epsilon parameter of the manager to ep. The epsilon
  parameter control the comparison between floating point numbers.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadEpsilon]

******************************************************************************/
void
Cudd_SetEpsilon(
  DdManager * dd,
  CUDD_VALUE_TYPE  ep)
{
    dd->epsilon = ep;

} /* end of Cudd_SetEpsilon */


/**Function********************************************************************

  Synopsis    [Reads the groupcheck parameter of the manager.]

  Description [Reads the groupcheck parameter of the manager. The
  groupcheck parameter determines the aggregation criterion in group
  sifting.]

  SideEffects [None]

  SeeAlso     [Cudd_SetGroupcheck]

******************************************************************************/
Cudd_AggregationType
Cudd_ReadGroupcheck(
  DdManager * dd)
{
    return(dd->groupcheck);

} /* end of Cudd_ReadGroupCheck */


/**Function********************************************************************

  Synopsis    [Sets the parameter groupcheck of the manager to gc.]

  Description [Sets the parameter groupcheck of the manager to gc. The
  groupcheck parameter determines the aggregation criterion in group
  sifting.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadGroupCheck]

******************************************************************************/
void
Cudd_SetGroupcheck(
  DdManager * dd,
  Cudd_AggregationType gc)
{
    dd->groupcheck = gc;

} /* end of Cudd_SetGroupcheck */


/**Function********************************************************************

  Synopsis    [Tells whether garbage collection is enabled.]

  Description [Returns 1 if garbage collection is enabled; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_EnableGarbageCollection Cudd_DisableGarbageCollection]

******************************************************************************/
int
Cudd_GarbageCollectionEnabled(
  DdManager * dd)
{
    return(dd->gcEnabled);

} /* end of Cudd_GarbageCollectionEnabled */


/**Function********************************************************************

  Synopsis    [Enables garbage collection.]

  Description [Enables garbage collection. Garbage collection is
  initially enabled. Therefore it is necessary to call this function
  only if garbage collection has been explicitly disabled.]

  SideEffects [None]

  SeeAlso     [Cudd_DisableGarbageCollection Cudd_GarbageCollectionEnabled]

******************************************************************************/
void
Cudd_EnableGarbageCollection(
  DdManager * dd)
{
    dd->gcEnabled = 1;

} /* end of Cudd_EnableGarbageCollection */


/**Function********************************************************************

  Synopsis    [Disables garbage collection.]

  Description [Disables garbage collection. Garbage collection is
  initially enabled. This function may be called to disable it.
  However, garbage collection will still occur when a new node must be
  created and no memory is left, or when garbage collection is required
  for correctness. (E.g., before reordering.)]

  SideEffects [None]

  SeeAlso     [Cudd_EnableGarbageCollection Cudd_GarbageCollectionEnabled]

******************************************************************************/
void
Cudd_DisableGarbageCollection(
  DdManager * dd)
{
    dd->gcEnabled = 0;

} /* end of Cudd_DisableGarbageCollection */


/**Function********************************************************************

  Synopsis    [Tells whether dead nodes are counted towards triggering
  reordering.]

  Description [Tells whether dead nodes are counted towards triggering
  reordering. Returns 1 if dead nodes are counted; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_TurnOnCountDead Cudd_TurnOffCountDead]

******************************************************************************/
int
Cudd_DeadAreCounted(
  DdManager * dd)
{
    return(dd->countDead == 0 ? 1 : 0);

} /* end of Cudd_DeadAreCounted */


/**Function********************************************************************

  Synopsis    [Causes the dead nodes to be counted towards triggering
  reordering.]

  Description [Causes the dead nodes to be counted towards triggering
  reordering. This causes more frequent reorderings. By default dead
  nodes are not counted.]

  SideEffects [Changes the manager.]

  SeeAlso     [Cudd_TurnOffCountDead Cudd_DeadAreCounted]

******************************************************************************/
void
Cudd_TurnOnCountDead(
  DdManager * dd)
{
    dd->countDead = 0;

} /* end of Cudd_TurnOnCountDead */


/**Function********************************************************************

  Synopsis    [Causes the dead nodes not to be counted towards triggering
  reordering.]

  Description [Causes the dead nodes not to be counted towards
  triggering reordering. This causes less frequent reorderings. By
  default dead nodes are not counted. Therefore there is no need to
  call this function unless Cudd_TurnOnCountDead has been previously
  called.]

  SideEffects [Changes the manager.]

  SeeAlso     [Cudd_TurnOnCountDead Cudd_DeadAreCounted]

******************************************************************************/
void
Cudd_TurnOffCountDead(
  DdManager * dd)
{
    dd->countDead = ~0;

} /* end of Cudd_TurnOffCountDead */


/**Function********************************************************************

  Synopsis    [Returns the current value of the recombination parameter used
  in group sifting.]

  Description [Returns the current value of the recombination
  parameter used in group sifting. A larger (positive) value makes the
  aggregation of variables due to the second difference criterion more
  likely. A smaller (negative) value makes aggregation less likely.]

  SideEffects [None]

  SeeAlso     [Cudd_SetRecomb]

******************************************************************************/
int
Cudd_ReadRecomb(
  DdManager * dd)
{
    return(dd->recomb);

} /* end of Cudd_ReadRecomb */


/**Function********************************************************************

  Synopsis    [Sets the value of the recombination parameter used in group
  sifting.]

  Description [Sets the value of the recombination parameter used in
  group sifting. A larger (positive) value makes the aggregation of
  variables due to the second difference criterion more likely. A
  smaller (negative) value makes aggregation less likely. The default
  value is 0.]

  SideEffects [Changes the manager.]

  SeeAlso     [Cudd_ReadRecomb]

******************************************************************************/
void
Cudd_SetRecomb(
  DdManager * dd,
  int  recomb)
{
    dd->recomb = recomb;

} /* end of Cudd_SetRecomb */


/**Function********************************************************************

  Synopsis    [Returns the current value of the symmviolation parameter used
  in group sifting.]

  Description [Returns the current value of the symmviolation
  parameter. This parameter is used in group sifting to decide how
  many violations to the symmetry conditions <code>f10 = f01</code> or
  <code>f11 = f00</code> are tolerable when checking for aggregation
  due to extended symmetry. The value should be between 0 and 100. A
  small value causes fewer variables to be aggregated. The default
  value is 0.]

  SideEffects [None]

  SeeAlso     [Cudd_SetSymmviolation]

******************************************************************************/
int
Cudd_ReadSymmviolation(
  DdManager * dd)
{
    return(dd->symmviolation);

} /* end of Cudd_ReadSymmviolation */


/**Function********************************************************************

  Synopsis    [Sets the value of the symmviolation parameter used
  in group sifting.]

  Description [Sets the value of the symmviolation
  parameter. This parameter is used in group sifting to decide how
  many violations to the symmetry conditions <code>f10 = f01</code> or
  <code>f11 = f00</code> are tolerable when checking for aggregation
  due to extended symmetry. The value should be between 0 and 100. A
  small value causes fewer variables to be aggregated. The default
  value is 0.]

  SideEffects [Changes the manager.]

  SeeAlso     [Cudd_ReadSymmviolation]

******************************************************************************/
void
Cudd_SetSymmviolation(
  DdManager * dd,
  int  symmviolation)
{
    dd->symmviolation = symmviolation;

} /* end of Cudd_SetSymmviolation */


/**Function********************************************************************

  Synopsis    [Returns the current value of the arcviolation parameter used
  in group sifting.]

  Description [Returns the current value of the arcviolation
  parameter. This parameter is used in group sifting to decide how
  many arcs into <code>y</code> not coming from <code>x</code> are
  tolerable when checking for aggregation due to extended
  symmetry. The value should be between 0 and 100. A small value
  causes fewer variables to be aggregated. The default value is 0.]

  SideEffects [None]

  SeeAlso     [Cudd_SetArcviolation]

******************************************************************************/
int
Cudd_ReadArcviolation(
  DdManager * dd)
{
    return(dd->arcviolation);

} /* end of Cudd_ReadArcviolation */


/**Function********************************************************************

  Synopsis    [Sets the value of the arcviolation parameter used
  in group sifting.]

  Description [Sets the value of the arcviolation
  parameter. This parameter is used in group sifting to decide how
  many arcs into <code>y</code> not coming from <code>x</code> are
  tolerable when checking for aggregation due to extended
  symmetry. The value should be between 0 and 100. A small value
  causes fewer variables to be aggregated. The default value is 0.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadArcviolation]

******************************************************************************/
void
Cudd_SetArcviolation(
  DdManager * dd,
  int  arcviolation)
{
    dd->arcviolation = arcviolation;

} /* end of Cudd_SetArcviolation */


/**Function********************************************************************

  Synopsis    [Reads the current size of the population used by the
  genetic algorithm for reordering.]

  Description [Reads the current size of the population used by the
  genetic algorithm for variable reordering. A larger population size will
  cause the genetic algorithm to take more time, but will generally
  produce better results. The default value is 0, in which case the
  package uses three times the number of variables as population size,
  with a maximum of 120.]

  SideEffects [None]

  SeeAlso     [Cudd_SetPopulationSize]

******************************************************************************/
int
Cudd_ReadPopulationSize(
  DdManager * dd)
{
    return(dd->populationSize);

} /* end of Cudd_ReadPopulationSize */


/**Function********************************************************************

  Synopsis    [Sets the size of the population used by the
  genetic algorithm for reordering.]

  Description [Sets the size of the population used by the
  genetic algorithm for variable reordering. A larger population size will
  cause the genetic algorithm to take more time, but will generally
  produce better results. The default value is 0, in which case the
  package uses three times the number of variables as population size,
  with a maximum of 120.]

  SideEffects [Changes the manager.]

  SeeAlso     [Cudd_ReadPopulationSize]

******************************************************************************/
void
Cudd_SetPopulationSize(
  DdManager * dd,
  int  populationSize)
{
    dd->populationSize = populationSize;

} /* end of Cudd_SetPopulationSize */


/**Function********************************************************************

  Synopsis    [Reads the current number of crossovers used by the
  genetic algorithm for reordering.]

  Description [Reads the current number of crossovers used by the
  genetic algorithm for variable reordering. A larger number of crossovers will
  cause the genetic algorithm to take more time, but will generally
  produce better results. The default value is 0, in which case the
  package uses three times the number of variables as number of crossovers,
  with a maximum of 60.]

  SideEffects [None]

  SeeAlso     [Cudd_SetNumberXovers]

******************************************************************************/
int
Cudd_ReadNumberXovers(
  DdManager * dd)
{
    return(dd->numberXovers);

} /* end of Cudd_ReadNumberXovers */


/**Function********************************************************************

  Synopsis    [Sets the number of crossovers used by the
  genetic algorithm for reordering.]

  Description [Sets the number of crossovers used by the genetic
  algorithm for variable reordering. A larger number of crossovers
  will cause the genetic algorithm to take more time, but will
  generally produce better results. The default value is 0, in which
  case the package uses three times the number of variables as number
  of crossovers, with a maximum of 60.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadNumberXovers]

******************************************************************************/
void
Cudd_SetNumberXovers(
  DdManager * dd,
  int  numberXovers)
{
    dd->numberXovers = numberXovers;

} /* end of Cudd_SetNumberXovers */


/**Function********************************************************************

  Synopsis    [Returns the order randomization factor.]

  Description [Returns the order randomization factor.  If non-zero this
  factor is used to determine a perturbation of the next reordering threshold.
  Larger factors cause larger perturbations.]

  SideEffects [None]

  SeeAlso     [Cudd_SetOrderRandomization]

******************************************************************************/
unsigned int
Cudd_ReadOrderRandomization(
  DdManager * dd)
{
    return(dd->randomizeOrder);

} /* end of Cudd_ReadOrderRandomization */


/**Function********************************************************************

  Synopsis    [Sets the order randomization factor.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadOrderRandomization]

******************************************************************************/
void
Cudd_SetOrderRandomization(
  DdManager * dd,
  unsigned int factor)
{
    dd->randomizeOrder = factor;

} /* end of Cudd_SetOrderRandomization */


/**Function********************************************************************

  Synopsis    [Returns the memory in use by the manager measured in bytes.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
unsigned long
Cudd_ReadMemoryInUse(
  DdManager * dd)
{
    return(dd->memused);

} /* end of Cudd_ReadMemoryInUse */


/**Function********************************************************************

  Synopsis    [Prints out statistics and settings for a CUDD manager.]

  Description [Prints out statistics and settings for a CUDD manager.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_PrintInfo(
  DdManager * dd,
  FILE * fp)
{
    int retval;
    Cudd_ReorderingType autoMethod, autoMethodZ;

    /* Modifiable parameters. */
    retval = fprintf(fp,"**** CUDD modifiable parameters ****\n");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Hard limit for cache size: %u\n",
		     Cudd_ReadMaxCacheHard(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Cache hit threshold for resizing: %u%%\n",
		     Cudd_ReadMinHit(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Garbage collection enabled: %s\n",
		     Cudd_GarbageCollectionEnabled(dd) ? "yes" : "no");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Limit for fast unique table growth: %u\n",
		     Cudd_ReadLooseUpTo(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,
		     "Maximum number of variables sifted per reordering: %d\n",
		     Cudd_ReadSiftMaxVar(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,
		     "Maximum number of variable swaps per reordering: %d\n",
		     Cudd_ReadSiftMaxSwap(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Maximum growth while sifting a variable: %g\n",
		     Cudd_ReadMaxGrowth(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Dynamic reordering of BDDs enabled: %s\n",
		     Cudd_ReorderingStatus(dd,&autoMethod) ? "yes" : "no");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Default BDD reordering method: %d\n",
		     (int) autoMethod);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Dynamic reordering of ZDDs enabled: %s\n",
		     Cudd_ReorderingStatusZdd(dd,&autoMethodZ) ? "yes" : "no");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Default ZDD reordering method: %d\n",
		     (int) autoMethodZ);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Realignment of ZDDs to BDDs enabled: %s\n",
		     Cudd_zddRealignmentEnabled(dd) ? "yes" : "no");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Realignment of BDDs to ZDDs enabled: %s\n",
		     Cudd_bddRealignmentEnabled(dd) ? "yes" : "no");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Dead nodes counted in triggering reordering: %s\n",
		     Cudd_DeadAreCounted(dd) ? "yes" : "no");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Group checking criterion: %d\n",
		     (int) Cudd_ReadGroupcheck(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Recombination threshold: %d\n", Cudd_ReadRecomb(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Symmetry violation threshold: %d\n",
		     Cudd_ReadSymmviolation(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Arc violation threshold: %d\n",
		     Cudd_ReadArcviolation(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"GA population size: %d\n",
		     Cudd_ReadPopulationSize(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of crossovers for GA: %d\n",
		     Cudd_ReadNumberXovers(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Next reordering threshold: %u\n",
		     Cudd_ReadNextReordering(dd));
    if (retval == EOF) return(0);

    /* Non-modifiable parameters. */
    retval = fprintf(fp,"**** CUDD non-modifiable parameters ****\n");
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Memory in use: %lu\n", Cudd_ReadMemoryInUse(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Peak number of nodes: %ld\n",
		     Cudd_ReadPeakNodeCount(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Peak number of live nodes: %d\n",
		     Cudd_ReadPeakLiveNodeCount(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of BDD variables: %d\n", dd->size);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of ZDD variables: %d\n", dd->sizeZ);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of cache entries: %u\n", dd->cacheSlots);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of cache look-ups: %.0f\n",
		     Cudd_ReadCacheLookUps(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of cache hits: %.0f\n",
		     Cudd_ReadCacheHits(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of cache insertions: %.0f\n",
		     dd->cacheinserts);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of cache collisions: %.0f\n",
		     dd->cachecollisions);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of cache deletions: %.0f\n",
		     dd->cachedeletions);
    if (retval == EOF) return(0);
    retval = cuddCacheProfile(dd,fp);
    if (retval == 0) return(0);
    retval = fprintf(fp,"Soft limit for cache size: %u\n",
		     Cudd_ReadMaxCache(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of buckets in unique table: %u\n", dd->slots);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Used buckets in unique table: %.2f%% (expected %.2f%%)\n",
		     100.0 * Cudd_ReadUsedSlots(dd),
		     100.0 * Cudd_ExpectedUsedSlots(dd));
    if (retval == EOF) return(0);
#ifdef DD_UNIQUE_PROFILE
    retval = fprintf(fp,"Unique lookups: %.0f\n", dd->uniqueLookUps);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Unique links: %.0f (%g per lookup)\n",
	    dd->uniqueLinks, dd->uniqueLinks / dd->uniqueLookUps);
    if (retval == EOF) return(0);
#endif
    retval = fprintf(fp,"Number of BDD and ADD nodes: %u\n", dd->keys);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of ZDD nodes: %u\n", dd->keysZ);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of dead BDD and ADD nodes: %u\n", dd->dead);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Number of dead ZDD nodes: %u\n", dd->deadZ);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Total number of nodes allocated: %.0f\n",
		     dd->allocated);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Total number of nodes reclaimed: %.0f\n",
		     dd->reclaimed);
    if (retval == EOF) return(0);
#ifdef DD_STATS
    retval = fprintf(fp,"Nodes freed: %.0f\n", dd->nodesFreed);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Nodes dropped: %.0f\n", dd->nodesDropped);
    if (retval == EOF) return(0);
#endif
#ifdef DD_COUNT
    retval = fprintf(fp,"Number of recursive calls: %.0f\n",
		     Cudd_ReadRecursiveCalls(dd));
    if (retval == EOF) return(0);
#endif
    retval = fprintf(fp,"Garbage collections so far: %d\n",
		     Cudd_ReadGarbageCollections(dd));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Time for garbage collection: %.2f sec\n",
		     ((double)Cudd_ReadGarbageCollectionTime(dd)/1000.0));
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Reorderings so far: %d\n", dd->reorderings);
    if (retval == EOF) return(0);
    retval = fprintf(fp,"Time for reordering: %.2f sec\n",
		     ((double)Cudd_ReadReorderingTime(dd)/1000.0));
    if (retval == EOF) return(0);
#ifdef DD_COUNT
    retval = fprintf(fp,"Node swaps in reordering: %.0f\n",
	Cudd_ReadSwapSteps(dd));
    if (retval == EOF) return(0);
#endif

    return(1);

} /* end of Cudd_PrintInfo */


/**Function********************************************************************

  Synopsis    [Reports the peak number of nodes.]

  Description [Reports the peak number of nodes. This number includes
  node on the free list. At the peak, the number of nodes on the free
  list is guaranteed to be less than DD_MEM_CHUNK.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadNodeCount Cudd_PrintInfo]

******************************************************************************/
long
Cudd_ReadPeakNodeCount(
  DdManager * dd)
{
    long count = 0;
    DdNodePtr *scan = dd->memoryList;

    while (scan != NULL) {
	count += DD_MEM_CHUNK;
	scan = (DdNodePtr *) *scan;
    }
    return(count);

} /* end of Cudd_ReadPeakNodeCount */


/**Function********************************************************************

  Synopsis    [Reports the peak number of live nodes.]

  Description [Reports the peak number of live nodes.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadNodeCount Cudd_PrintInfo Cudd_ReadPeakNodeCount]

******************************************************************************/
int
Cudd_ReadPeakLiveNodeCount(
  DdManager * dd)
{
    unsigned int live = dd->keys - dd->dead;

    if (live > dd->peakLiveNodes) {
	dd->peakLiveNodes = live;
    }
    return((int)dd->peakLiveNodes);

} /* end of Cudd_ReadPeakLiveNodeCount */


/**Function********************************************************************

  Synopsis    [Reports the number of nodes in BDDs and ADDs.]

  Description [Reports the number of live nodes in BDDs and ADDs. This
  number does not include the isolated projection functions and the
  unused constants. These nodes that are not counted are not part of
  the DDs manipulated by the application.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadPeakNodeCount Cudd_zddReadNodeCount]

******************************************************************************/
long
Cudd_ReadNodeCount(
  DdManager * dd)
{
    long count;
    int i;

#ifndef DD_NO_DEATH_ROW
    cuddClearDeathRow(dd);
#endif

    count = (long) (dd->keys - dd->dead);

    /* Count isolated projection functions. Their number is subtracted
    ** from the node count because they are not part of the BDDs.
    */
    for (i=0; i < dd->size; i++) {
	if (dd->vars[i]->ref == 1) count--;
    }
    /* Subtract from the count the unused constants. */
    if (DD_ZERO(dd)->ref == 1) count--;
    if (DD_PLUS_INFINITY(dd)->ref == 1) count--;
    if (DD_MINUS_INFINITY(dd)->ref == 1) count--;

    return(count);

} /* end of Cudd_ReadNodeCount */



/**Function********************************************************************

  Synopsis    [Reports the number of nodes in ZDDs.]

  Description [Reports the number of nodes in ZDDs. This
  number always includes the two constants 1 and 0.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadPeakNodeCount Cudd_ReadNodeCount]

******************************************************************************/
long
Cudd_zddReadNodeCount(
  DdManager * dd)
{
    return((long)(dd->keysZ - dd->deadZ + 2));

} /* end of Cudd_zddReadNodeCount */


/**Function********************************************************************

  Synopsis    [Adds a function to a hook.]

  Description [Adds a function to a hook. A hook is a list of
  application-provided functions called on certain occasions by the
  package. Returns 1 if the function is successfully added; 2 if the
  function was already in the list; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_RemoveHook]

******************************************************************************/
int
Cudd_AddHook(
  DdManager * dd,
  DD_HFP f,
  Cudd_HookType where)
{
    DdHook **hook, *nextHook, *newHook;

    switch (where) {
    case CUDD_PRE_GC_HOOK:
	hook = &(dd->preGCHook);
	break;
    case CUDD_POST_GC_HOOK:
	hook = &(dd->postGCHook);
	break;
    case CUDD_PRE_REORDERING_HOOK:
	hook = &(dd->preReorderingHook);
	break;
    case CUDD_POST_REORDERING_HOOK:
	hook = &(dd->postReorderingHook);
	break;
    default:
	return(0);
    }
    /* Scan the list and find whether the function is already there.
    ** If so, just return. */
    nextHook = *hook;
    while (nextHook != NULL) {
	if (nextHook->f == f) {
	    return(2);
	}
	hook = &(nextHook->next);
	nextHook = nextHook->next;
    }
    /* The function was not in the list. Create a new item and append it
    ** to the end of the list. */
    newHook = ALLOC(DdHook,1);
    if (newHook == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    newHook->next = NULL;
    newHook->f = f;
    *hook = newHook;
    return(1);

} /* end of Cudd_AddHook */


/**Function********************************************************************

  Synopsis    [Removes a function from a hook.]

  Description [Removes a function from a hook. A hook is a list of
  application-provided functions called on certain occasions by the
  package. Returns 1 if successful; 0 the function was not in the list.]

  SideEffects [None]

  SeeAlso     [Cudd_AddHook]

******************************************************************************/
int
Cudd_RemoveHook(
  DdManager * dd,
  DD_HFP f,
  Cudd_HookType where)
{
    DdHook **hook, *nextHook;

    switch (where) {
    case CUDD_PRE_GC_HOOK:
	hook = &(dd->preGCHook);
	break;
    case CUDD_POST_GC_HOOK:
	hook = &(dd->postGCHook);
	break;
    case CUDD_PRE_REORDERING_HOOK:
	hook = &(dd->preReorderingHook);
	break;
    case CUDD_POST_REORDERING_HOOK:
	hook = &(dd->postReorderingHook);
	break;
    default:
	return(0);
    }
    nextHook = *hook;
    while (nextHook != NULL) {
	if (nextHook->f == f) {
	    *hook = nextHook->next;
	    FREE(nextHook);
	    return(1);
	}
	hook = &(nextHook->next);
	nextHook = nextHook->next;
    }

    return(0);

} /* end of Cudd_RemoveHook */


/**Function********************************************************************

  Synopsis    [Checks whether a function is in a hook.]

  Description [Checks whether a function is in a hook. A hook is a list of
  application-provided functions called on certain occasions by the
  package. Returns 1 if the function is found; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_AddHook Cudd_RemoveHook]

******************************************************************************/
int
Cudd_IsInHook(
  DdManager * dd,
  DD_HFP f,
  Cudd_HookType where)
{
    DdHook *hook;

    switch (where) {
    case CUDD_PRE_GC_HOOK:
	hook = dd->preGCHook;
	break;
    case CUDD_POST_GC_HOOK:
	hook = dd->postGCHook;
	break;
    case CUDD_PRE_REORDERING_HOOK:
	hook = dd->preReorderingHook;
	break;
    case CUDD_POST_REORDERING_HOOK:
	hook = dd->postReorderingHook;
	break;
    default:
	return(0);
    }
    /* Scan the list and find whether the function is already there. */
    while (hook != NULL) {
	if (hook->f == f) {
	    return(1);
	}
	hook = hook->next;
    }
    return(0);

} /* end of Cudd_IsInHook */


/**Function********************************************************************

  Synopsis    [Sample hook function to call before reordering.]

  Description [Sample hook function to call before reordering.
  Prints on the manager's stdout reordering method and initial size.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_StdPostReordHook]

******************************************************************************/
int
Cudd_StdPreReordHook(
  DdManager *dd,
  const char *str,
  void *data)
{
    Cudd_ReorderingType method = (Cudd_ReorderingType) (ptruint) data;
    int retval;

    retval = fprintf(dd->out,"%s reordering with ", str);
    if (retval == EOF) return(0);
    switch (method) {
    case CUDD_REORDER_SIFT_CONVERGE:
    case CUDD_REORDER_SYMM_SIFT_CONV:
    case CUDD_REORDER_GROUP_SIFT_CONV:
    case CUDD_REORDER_WINDOW2_CONV:
    case CUDD_REORDER_WINDOW3_CONV:
    case CUDD_REORDER_WINDOW4_CONV:
    case CUDD_REORDER_LINEAR_CONVERGE:
	retval = fprintf(dd->out,"converging ");
	if (retval == EOF) return(0);
	break;
    default:
	break;
    }
    switch (method) {
    case CUDD_REORDER_RANDOM:
    case CUDD_REORDER_RANDOM_PIVOT:
	retval = fprintf(dd->out,"random");
	break;
    case CUDD_REORDER_SIFT:
    case CUDD_REORDER_SIFT_CONVERGE:
	retval = fprintf(dd->out,"sifting");
	break;
    case CUDD_REORDER_SYMM_SIFT:
    case CUDD_REORDER_SYMM_SIFT_CONV:
	retval = fprintf(dd->out,"symmetric sifting");
	break;
    case CUDD_REORDER_LAZY_SIFT:
	retval = fprintf(dd->out,"lazy sifting");
	break;
    case CUDD_REORDER_GROUP_SIFT:
    case CUDD_REORDER_GROUP_SIFT_CONV:
	retval = fprintf(dd->out,"group sifting");
	break;
    case CUDD_REORDER_WINDOW2:
    case CUDD_REORDER_WINDOW3:
    case CUDD_REORDER_WINDOW4:
    case CUDD_REORDER_WINDOW2_CONV:
    case CUDD_REORDER_WINDOW3_CONV:
    case CUDD_REORDER_WINDOW4_CONV:
	retval = fprintf(dd->out,"window");
	break;
    case CUDD_REORDER_ANNEALING:
	retval = fprintf(dd->out,"annealing");
	break;
    case CUDD_REORDER_GENETIC:
	retval = fprintf(dd->out,"genetic");
	break;
    case CUDD_REORDER_LINEAR:
    case CUDD_REORDER_LINEAR_CONVERGE:
	retval = fprintf(dd->out,"linear sifting");
	break;
    case CUDD_REORDER_EXACT:
	retval = fprintf(dd->out,"exact");
	break;
    default:
	return(0);
    }
    if (retval == EOF) return(0);

    retval = fprintf(dd->out,": from %ld to ... ", strcmp(str, "BDD") == 0 ?
		     Cudd_ReadNodeCount(dd) : Cudd_zddReadNodeCount(dd));
    if (retval == EOF) return(0);
    fflush(dd->out);
    return(1);

} /* end of Cudd_StdPreReordHook */


/**Function********************************************************************

  Synopsis    [Sample hook function to call after reordering.]

  Description [Sample hook function to call after reordering.
  Prints on the manager's stdout final size and reordering time.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_StdPreReordHook]

******************************************************************************/
int
Cudd_StdPostReordHook(
  DdManager *dd,
  const char *str,
  void *data)
{
    unsigned long initialTime = (long) data;
    int retval;
    unsigned long finalTime = util_cpu_time();
    double totalTimeSec = (double)(finalTime - initialTime) / 1000.0;

    retval = fprintf(dd->out,"%ld nodes in %g sec\n", strcmp(str, "BDD") == 0 ?
		     Cudd_ReadNodeCount(dd) : Cudd_zddReadNodeCount(dd),
		     totalTimeSec);
    if (retval == EOF) return(0);
    retval = fflush(dd->out);
    if (retval == EOF) return(0);
    return(1);

} /* end of Cudd_StdPostReordHook */


/**Function********************************************************************

  Synopsis    [Enables reporting of reordering stats.]

  Description [Enables reporting of reordering stats.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Installs functions in the pre-reordering and post-reordering
  hooks.]

  SeeAlso     [Cudd_DisableReorderingReporting Cudd_ReorderingReporting]

******************************************************************************/
int
Cudd_EnableReorderingReporting(
  DdManager *dd)
{
    if (!Cudd_AddHook(dd, Cudd_StdPreReordHook, CUDD_PRE_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_AddHook(dd, Cudd_StdPostReordHook, CUDD_POST_REORDERING_HOOK)) {
	return(0);
    }
    return(1);

} /* end of Cudd_EnableReorderingReporting */


/**Function********************************************************************

  Synopsis    [Disables reporting of reordering stats.]

  Description [Disables reporting of reordering stats.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Removes functions from the pre-reordering and post-reordering
  hooks.]

  SeeAlso     [Cudd_EnableReorderingReporting Cudd_ReorderingReporting]

******************************************************************************/
int
Cudd_DisableReorderingReporting(
  DdManager *dd)
{
    if (!Cudd_RemoveHook(dd, Cudd_StdPreReordHook, CUDD_PRE_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_RemoveHook(dd, Cudd_StdPostReordHook, CUDD_POST_REORDERING_HOOK)) {
	return(0);
    }
    return(1);

} /* end of Cudd_DisableReorderingReporting */


/**Function********************************************************************

  Synopsis    [Returns 1 if reporting of reordering stats is enabled.]

  Description [Returns 1 if reporting of reordering stats is enabled;
  0 otherwise.]

  SideEffects [none]

  SeeAlso     [Cudd_EnableReorderingReporting Cudd_DisableReorderingReporting]

******************************************************************************/
int
Cudd_ReorderingReporting(
  DdManager *dd)
{
    return(Cudd_IsInHook(dd, Cudd_StdPreReordHook, CUDD_PRE_REORDERING_HOOK));

} /* end of Cudd_ReorderingReporting */


/**Function********************************************************************

  Synopsis    [Hook function to print the current variable order.]

  Description [Hook function to print the current variable order.  It may be
  called before or after reordering. Prints on the manager's stdout a
  parenthesized list that describes the variable groups.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_StdPreReordHook]

******************************************************************************/
int
Cudd_PrintGroupedOrder(
  DdManager * dd,
  const char *str,
  void *data)
{
    int isBdd = strcmp(str, "ZDD");
    MtrNode *tree = isBdd ? dd->tree : dd->treeZ;
    int *invperm = isBdd ? dd->invperm : dd->invpermZ;
    int size = isBdd ? dd->size : dd->sizeZ;
    if (tree == NULL) {
        int i, retval;
        for (i=0; i < size; i++) {
            retval = fprintf(dd->out, "%c%d", i==0 ? '(' : ',', invperm[i]);
            if (retval == EOF) return(0);
        }
        retval = fprintf(dd->out,")\n");
        return (retval != EOF);
    } else {
        return Mtr_PrintGroupedOrder(tree,invperm,dd->out);
    }
        
} /* end of Cudd_PrintGroupedOrder */


/**Function********************************************************************

  Synopsis    [Enables monitoring of ordering.]

  Description [Enables monitoring of ordering.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Installs functions in the pre-reordering and post-reordering
  hooks.]

  SeeAlso     [Cudd_EnableReorderingReporting]

******************************************************************************/
int
Cudd_EnableOrderingMonitoring(
  DdManager *dd)
{
    if (!Cudd_AddHook(dd, Cudd_PrintGroupedOrder, CUDD_PRE_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_AddHook(dd, Cudd_StdPreReordHook, CUDD_PRE_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_AddHook(dd, Cudd_StdPostReordHook, CUDD_POST_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_AddHook(dd, Cudd_PrintGroupedOrder, CUDD_POST_REORDERING_HOOK)) {
	return(0);
    }
    return(1);

} /* end of Cudd_EnableOrderingMonitoring */


/**Function********************************************************************

  Synopsis    [Disables monitoring of ordering.]

  Description [Disables monitoring of ordering.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Removes functions from the pre-reordering and post-reordering
  hooks.]

  SeeAlso     [Cudd_EnableOrderingMonitoring]

******************************************************************************/
int
Cudd_DisableOrderingMonitoring(
  DdManager *dd)
{
    if (!Cudd_RemoveHook(dd, Cudd_StdPreReordHook, CUDD_PRE_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_RemoveHook(dd, Cudd_PrintGroupedOrder, CUDD_PRE_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_RemoveHook(dd, Cudd_PrintGroupedOrder, CUDD_POST_REORDERING_HOOK)) {
	return(0);
    }
    if (!Cudd_RemoveHook(dd, Cudd_StdPostReordHook, CUDD_POST_REORDERING_HOOK)) {
	return(0);
    }
    return(1);

} /* end of Cudd_DisableOrderingMonitoring */


/**Function********************************************************************

  Synopsis    [Returns 1 if monitoring of ordering is enabled.]

  Description [Returns 1 if monitoring of ordering is enabled;
  0 otherwise.]

  SideEffects [none]

  SeeAlso     [Cudd_EnableOrderingMonitoring Cudd_DisableOrderingMonitoring]

******************************************************************************/
int
Cudd_OrderingMonitoring(
  DdManager *dd)
{
    return(Cudd_IsInHook(dd, Cudd_PrintGroupedOrder, CUDD_PRE_REORDERING_HOOK));

} /* end of Cudd_OrderingMonitoring */


/**Function********************************************************************

  Synopsis    [Returns the code of the last error.]

  Description [Returns the code of the last error. The error codes are
  defined in cudd.h.]

  SideEffects [None]

  SeeAlso     [Cudd_ClearErrorCode]

******************************************************************************/
Cudd_ErrorType
Cudd_ReadErrorCode(
  DdManager *dd)
{
    return(dd->errorCode);

} /* end of Cudd_ReadErrorCode */


/**Function********************************************************************

  Synopsis    [Clear the error code of a manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadErrorCode]

******************************************************************************/
void
Cudd_ClearErrorCode(
  DdManager *dd)
{
    dd->errorCode = CUDD_NO_ERROR;

} /* end of Cudd_ClearErrorCode */


/**Function********************************************************************

  Synopsis    [Reads the stdout of a manager.]

  Description [Reads the stdout of a manager. This is the file pointer to
  which messages normally going to stdout are written. It is initialized
  to stdout. Cudd_SetStdout allows the application to redirect it.]

  SideEffects [None]

  SeeAlso     [Cudd_SetStdout Cudd_ReadStderr]

******************************************************************************/
FILE *
Cudd_ReadStdout(
  DdManager *dd)
{
    return(dd->out);

} /* end of Cudd_ReadStdout */


/**Function********************************************************************

  Synopsis    [Sets the stdout of a manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadStdout Cudd_SetStderr]

******************************************************************************/
void
Cudd_SetStdout(
  DdManager *dd,
  FILE *fp)
{
    dd->out = fp;

} /* end of Cudd_SetStdout */


/**Function********************************************************************

  Synopsis    [Reads the stderr of a manager.]

  Description [Reads the stderr of a manager. This is the file pointer to
  which messages normally going to stderr are written. It is initialized
  to stderr. Cudd_SetStderr allows the application to redirect it.]

  SideEffects [None]

  SeeAlso     [Cudd_SetStderr Cudd_ReadStdout]

******************************************************************************/
FILE *
Cudd_ReadStderr(
  DdManager *dd)
{
    return(dd->err);

} /* end of Cudd_ReadStderr */


/**Function********************************************************************

  Synopsis    [Sets the stderr of a manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadStderr Cudd_SetStdout]

******************************************************************************/
void
Cudd_SetStderr(
  DdManager *dd,
  FILE *fp)
{
    dd->err = fp;

} /* end of Cudd_SetStderr */


/**Function********************************************************************

  Synopsis    [Returns the threshold for the next dynamic reordering.]

  Description [Returns the threshold for the next dynamic reordering.
  The threshold is in terms of number of nodes and is in effect only
  if reordering is enabled. The count does not include the dead nodes,
  unless the countDead parameter of the manager has been changed from
  its default setting.]

  SideEffects [None]

  SeeAlso     [Cudd_SetNextReordering]

******************************************************************************/
unsigned int
Cudd_ReadNextReordering(
  DdManager *dd)
{
    return(dd->nextDyn);

} /* end of Cudd_ReadNextReordering */


/**Function********************************************************************

  Synopsis    [Sets the threshold for the next dynamic reordering.]

  Description [Sets the threshold for the next dynamic reordering.
  The threshold is in terms of number of nodes and is in effect only
  if reordering is enabled. The count does not include the dead nodes,
  unless the countDead parameter of the manager has been changed from
  its default setting.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadNextReordering]

******************************************************************************/
void
Cudd_SetNextReordering(
  DdManager *dd,
  unsigned int next)
{
    dd->nextDyn = next;

} /* end of Cudd_SetNextReordering */


/**Function********************************************************************

  Synopsis    [Reads the number of elementary reordering steps.]

  Description []

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
double
Cudd_ReadSwapSteps(
  DdManager *dd)
{
#ifdef DD_COUNT
    return(dd->swapSteps);
#else
    return(-1);
#endif

} /* end of Cudd_ReadSwapSteps */


/**Function********************************************************************

  Synopsis    [Reads the maximum allowed number of live nodes.]

  Description [Reads the maximum allowed number of live nodes. When this
  number is exceeded, the package returns NULL.]

  SideEffects [none]

  SeeAlso     [Cudd_SetMaxLive]

******************************************************************************/
unsigned int
Cudd_ReadMaxLive(
  DdManager *dd)
{
    return(dd->maxLive);

} /* end of Cudd_ReadMaxLive */


/**Function********************************************************************

  Synopsis    [Sets the maximum allowed number of live nodes.]

  Description [Sets the maximum allowed number of live nodes. When this
  number is exceeded, the package returns NULL.]

  SideEffects [none]

  SeeAlso     [Cudd_ReadMaxLive]

******************************************************************************/
void
Cudd_SetMaxLive(
  DdManager *dd,
  unsigned int maxLive)
{
    dd->maxLive = maxLive;

} /* end of Cudd_SetMaxLive */


/**Function********************************************************************

  Synopsis    [Reads the maximum allowed memory.]

  Description [Reads the maximum allowed memory. When this
  number is exceeded, the package returns NULL.]

  SideEffects [none]

  SeeAlso     [Cudd_SetMaxMemory]

******************************************************************************/
unsigned long
Cudd_ReadMaxMemory(
  DdManager *dd)
{
    return(dd->maxmemhard);

} /* end of Cudd_ReadMaxMemory */


/**Function********************************************************************

  Synopsis    [Sets the maximum allowed memory.]

  Description [Sets the maximum allowed memory. When this
  number is exceeded, the package returns NULL.]

  SideEffects [none]

  SeeAlso     [Cudd_ReadMaxMemory]

******************************************************************************/
void
Cudd_SetMaxMemory(
  DdManager *dd,
  unsigned long maxMemory)
{
    dd->maxmemhard = maxMemory;

} /* end of Cudd_SetMaxMemory */


/**Function********************************************************************

  Synopsis    [Prevents sifting of a variable.]

  Description [This function sets a flag to prevent sifting of a
  variable.  Returns 1 if successful; 0 otherwise (i.e., invalid
  variable index).]

  SideEffects [Changes the "bindVar" flag in DdSubtable.]

  SeeAlso     [Cudd_bddUnbindVar]

******************************************************************************/
int
Cudd_bddBindVar(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return(0);
    dd->subtables[dd->perm[index]].bindVar = 1;
    return(1);

} /* end of Cudd_bddBindVar */


/**Function********************************************************************

  Synopsis    [Allows the sifting of a variable.]

  Description [This function resets the flag that prevents the sifting
  of a variable. In successive variable reorderings, the variable will
  NOT be skipped, that is, sifted.  Initially all variables can be
  sifted. It is necessary to call this function only to re-enable
  sifting after a call to Cudd_bddBindVar. Returns 1 if successful; 0
  otherwise (i.e., invalid variable index).]

  SideEffects [Changes the "bindVar" flag in DdSubtable.]

  SeeAlso     [Cudd_bddBindVar]

******************************************************************************/
int
Cudd_bddUnbindVar(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return(0);
    dd->subtables[dd->perm[index]].bindVar = 0;
    return(1);

} /* end of Cudd_bddUnbindVar */


/**Function********************************************************************

  Synopsis    [Tells whether a variable can be sifted.]

  Description [This function returns 1 if a variable is enabled for
  sifting.  Initially all variables can be sifted. This function returns
  0 only if there has been a previous call to Cudd_bddBindVar for that
  variable not followed by a call to Cudd_bddUnbindVar. The function returns
  0 also in the case in which the index of the variable is out of bounds.]

  SideEffects [none]

  SeeAlso     [Cudd_bddBindVar Cudd_bddUnbindVar]

******************************************************************************/
int
Cudd_bddVarIsBound(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return(0);
    return(dd->subtables[dd->perm[index]].bindVar);

} /* end of Cudd_bddVarIsBound */


/**Function********************************************************************

  Synopsis    [Sets a variable type to primary input.]

  Description [Sets a variable type to primary input.  The variable type is
  used by lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetPsVar Cudd_bddSetNsVar Cudd_bddIsPiVar]

******************************************************************************/
int
Cudd_bddSetPiVar(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return (0);
    dd->subtables[dd->perm[index]].varType = CUDD_VAR_PRIMARY_INPUT;
    return(1);

} /* end of Cudd_bddSetPiVar */


/**Function********************************************************************

  Synopsis    [Sets a variable type to present state.]

  Description [Sets a variable type to present state.  The variable type is
  used by lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetPiVar Cudd_bddSetNsVar Cudd_bddIsPsVar]

******************************************************************************/
int
Cudd_bddSetPsVar(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return (0);
    dd->subtables[dd->perm[index]].varType = CUDD_VAR_PRESENT_STATE;
    return(1);

} /* end of Cudd_bddSetPsVar */


/**Function********************************************************************

  Synopsis    [Sets a variable type to next state.]

  Description [Sets a variable type to next state.  The variable type is
  used by lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetPiVar Cudd_bddSetPsVar Cudd_bddIsNsVar]

******************************************************************************/
int
Cudd_bddSetNsVar(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return (0);
    dd->subtables[dd->perm[index]].varType = CUDD_VAR_NEXT_STATE;
    return(1);

} /* end of Cudd_bddSetNsVar */


/**Function********************************************************************

  Synopsis    [Checks whether a variable is primary input.]

  Description [Checks whether a variable is primary input.  Returns 1 if
  the variable's type is primary input; 0 if the variable exists but is
  not a primary input; -1 if the variable does not exist.]

  SideEffects [none]

  SeeAlso     [Cudd_bddSetPiVar Cudd_bddIsPsVar Cudd_bddIsNsVar]

******************************************************************************/
int
Cudd_bddIsPiVar(
  DdManager *dd /* manager */,
  int index /* variable index */)
{
    if (index >= dd->size || index < 0) return -1;
    return (dd->subtables[dd->perm[index]].varType == CUDD_VAR_PRIMARY_INPUT);

} /* end of Cudd_bddIsPiVar */


/**Function********************************************************************

  Synopsis    [Checks whether a variable is present state.]

  Description [Checks whether a variable is present state.  Returns 1 if
  the variable's type is present state; 0 if the variable exists but is
  not a present state; -1 if the variable does not exist.]

  SideEffects [none]

  SeeAlso     [Cudd_bddSetPsVar Cudd_bddIsPiVar Cudd_bddIsNsVar]

******************************************************************************/
int
Cudd_bddIsPsVar(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return -1;
    return (dd->subtables[dd->perm[index]].varType == CUDD_VAR_PRESENT_STATE);

} /* end of Cudd_bddIsPsVar */


/**Function********************************************************************

  Synopsis    [Checks whether a variable is next state.]

  Description [Checks whether a variable is next state.  Returns 1 if
  the variable's type is present state; 0 if the variable exists but is
  not a present state; -1 if the variable does not exist.]

  SideEffects [none]

  SeeAlso     [Cudd_bddSetNsVar Cudd_bddIsPiVar Cudd_bddIsPsVar]

******************************************************************************/
int
Cudd_bddIsNsVar(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return -1;
    return (dd->subtables[dd->perm[index]].varType == CUDD_VAR_NEXT_STATE);

} /* end of Cudd_bddIsNsVar */


/**Function********************************************************************

  Synopsis    [Sets a corresponding pair index for a given index.]

  Description [Sets a corresponding pair index for a given index.
  These pair indices are present and next state variable.  Returns 1 if
  successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddReadPairIndex]

******************************************************************************/
int
Cudd_bddSetPairIndex(
  DdManager *dd /* manager */,
  int index /* variable index */,
  int pairIndex /* corresponding variable index */)
{
    if (index >= dd->size || index < 0) return(0);
    dd->subtables[dd->perm[index]].pairIndex = pairIndex;
    return(1);

} /* end of Cudd_bddSetPairIndex */


/**Function********************************************************************

  Synopsis    [Reads a corresponding pair index for a given index.]

  Description [Reads a corresponding pair index for a given index.
  These pair indices are present and next state variable.  Returns the
  corresponding variable index if the variable exists; -1 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetPairIndex]

******************************************************************************/
int
Cudd_bddReadPairIndex(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return -1;
    return dd->subtables[dd->perm[index]].pairIndex;

} /* end of Cudd_bddReadPairIndex */


/**Function********************************************************************

  Synopsis    [Sets a variable to be grouped.]

  Description [Sets a variable to be grouped. This function is used for
  lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetVarHardGroup Cudd_bddResetVarToBeGrouped]

******************************************************************************/
int
Cudd_bddSetVarToBeGrouped(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(0);
    if (dd->subtables[dd->perm[index]].varToBeGrouped <= CUDD_LAZY_SOFT_GROUP) {
	dd->subtables[dd->perm[index]].varToBeGrouped = CUDD_LAZY_SOFT_GROUP;
    }
    return(1);

} /* end of Cudd_bddSetVarToBeGrouped */


/**Function********************************************************************

  Synopsis    [Sets a variable to be a hard group.]

  Description [Sets a variable to be a hard group.  This function is used
  for lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetVarToBeGrouped Cudd_bddResetVarToBeGrouped
  Cudd_bddIsVarHardGroup]

******************************************************************************/
int
Cudd_bddSetVarHardGroup(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(0);
    dd->subtables[dd->perm[index]].varToBeGrouped = CUDD_LAZY_HARD_GROUP;
    return(1);

} /* end of Cudd_bddSetVarHardGrouped */


/**Function********************************************************************

  Synopsis    [Resets a variable not to be grouped.]

  Description [Resets a variable not to be grouped.  This function is
  used for lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetVarToBeGrouped Cudd_bddSetVarHardGroup]

******************************************************************************/
int
Cudd_bddResetVarToBeGrouped(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(0);
    if (dd->subtables[dd->perm[index]].varToBeGrouped <=
	CUDD_LAZY_SOFT_GROUP) {
	dd->subtables[dd->perm[index]].varToBeGrouped = CUDD_LAZY_NONE;
    }
    return(1);

} /* end of Cudd_bddResetVarToBeGrouped */


/**Function********************************************************************

  Synopsis    [Checks whether a variable is set to be grouped.]

  Description [Checks whether a variable is set to be grouped. This
  function is used for lazy sifting.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
int
Cudd_bddIsVarToBeGrouped(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(-1);
    if (dd->subtables[dd->perm[index]].varToBeGrouped == CUDD_LAZY_UNGROUP)
	return(0);
    else
	return(dd->subtables[dd->perm[index]].varToBeGrouped);

} /* end of Cudd_bddIsVarToBeGrouped */


/**Function********************************************************************

  Synopsis    [Sets a variable to be ungrouped.]

  Description [Sets a variable to be ungrouped. This function is used
  for lazy sifting.  Returns 1 if successful; 0 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddIsVarToBeUngrouped]

******************************************************************************/
int
Cudd_bddSetVarToBeUngrouped(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(0);
    dd->subtables[dd->perm[index]].varToBeGrouped = CUDD_LAZY_UNGROUP;
    return(1);

} /* end of Cudd_bddSetVarToBeGrouped */


/**Function********************************************************************

  Synopsis    [Checks whether a variable is set to be ungrouped.]

  Description [Checks whether a variable is set to be ungrouped. This
  function is used for lazy sifting.  Returns 1 if the variable is marked
  to be ungrouped; 0 if the variable exists, but it is not marked to be
  ungrouped; -1 if the variable does not exist.]

  SideEffects [none]

  SeeAlso     [Cudd_bddSetVarToBeUngrouped]

******************************************************************************/
int
Cudd_bddIsVarToBeUngrouped(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(-1);
    return dd->subtables[dd->perm[index]].varToBeGrouped == CUDD_LAZY_UNGROUP;

} /* end of Cudd_bddIsVarToBeGrouped */


/**Function********************************************************************

  Synopsis    [Checks whether a variable is set to be in a hard group.]

  Description [Checks whether a variable is set to be in a hard group.  This
  function is used for lazy sifting.  Returns 1 if the variable is marked
  to be in a hard group; 0 if the variable exists, but it is not marked to be
  in a hard group; -1 if the variable does not exist.]

  SideEffects [none]

  SeeAlso     [Cudd_bddSetVarHardGroup]

******************************************************************************/
int
Cudd_bddIsVarHardGroup(
  DdManager *dd,
  int index)
{
    if (index >= dd->size || index < 0) return(-1);
    if (dd->subtables[dd->perm[index]].varToBeGrouped == CUDD_LAZY_HARD_GROUP)
	return(1);
    return(0);

} /* end of Cudd_bddIsVarToBeGrouped */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Fixes a variable group tree.]

  Description []

  SideEffects [Changes the variable group tree.]

  SeeAlso     []

******************************************************************************/
static void
fixVarTree(
  MtrNode * treenode,
  int * perm,
  int  size)
{
    treenode->index = treenode->low;
    treenode->low = ((int) treenode->index < size) ?
	perm[treenode->index] : treenode->index;
    if (treenode->child != NULL)
	fixVarTree(treenode->child, perm, size);
    if (treenode->younger != NULL)
	fixVarTree(treenode->younger, perm, size);
    return;

} /* end of fixVarTree */


/**Function********************************************************************

  Synopsis    [Adds multiplicity groups to a ZDD variable group tree.]

  Description [Adds multiplicity groups to a ZDD variable group tree.
  Returns 1 if successful; 0 otherwise. This function creates the groups
  for set of ZDD variables (whose cardinality is given by parameter
  multiplicity) that are created for each BDD variable in
  Cudd_zddVarsFromBddVars. The crux of the matter is to determine the index
  each new group. (The index of the first variable in the group.)
  We first build all the groups for the children of a node, and then deal
  with the ZDD variables that are directly attached to the node. The problem
  for these is that the tree itself does not provide information on their
  position inside the group. While we deal with the children of the node,
  therefore, we keep track of all the positions they occupy. The remaining
  positions in the tree can be freely used. Also, we keep track of all the
  variables placed in the children. All the remaining variables are directly
  attached to the group. We can then place any pair of variables not yet
  grouped in any pair of available positions in the node.]

  SideEffects [Changes the variable group tree.]

  SeeAlso     [Cudd_zddVarsFromBddVars]

******************************************************************************/
static int
addMultiplicityGroups(
  DdManager *dd /* manager */,
  MtrNode *treenode /* current tree node */,
  int multiplicity /* how many ZDD vars per BDD var */,
  char *vmask /* variable pairs for which a group has been already built */,
  char *lmask /* levels for which a group has already been built*/)
{
    int startV, stopV, startL;
    int i, j;
    MtrNode *auxnode = treenode;

    while (auxnode != NULL) {
	if (auxnode->child != NULL) {
	    addMultiplicityGroups(dd,auxnode->child,multiplicity,vmask,lmask);
	}
	/* Build remaining groups. */
	startV = dd->permZ[auxnode->index] / multiplicity;
	startL = auxnode->low / multiplicity;
	stopV = startV + auxnode->size / multiplicity;
	/* Walk down vmask starting at startV and build missing groups. */
	for (i = startV, j = startL; i < stopV; i++) {
	    if (vmask[i] == 0) {
		MtrNode *node;
		while (lmask[j] == 1) j++;
		node = Mtr_MakeGroup(auxnode, j * multiplicity, multiplicity,
				     MTR_FIXED);
		if (node == NULL) {
		    return(0);
		}
		node->index = dd->invpermZ[i * multiplicity];
		vmask[i] = 1;
		lmask[j] = 1;
	    }
	}
	auxnode = auxnode->younger;
    }
    return(1);

} /* end of addMultiplicityGroups */
