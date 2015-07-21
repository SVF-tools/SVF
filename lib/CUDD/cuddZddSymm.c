/**CFile***********************************************************************

  FileName    [cuddZddSymm.c]

  PackageName [cudd]

  Synopsis    [Functions for symmetry-based ZDD variable reordering.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_zddSymmProfile()
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    <li> cuddZddSymmCheck()
		    <li> cuddZddSymmSifting()
		    <li> cuddZddSymmSiftingConv()
		    </ul>
	       Static procedures included in this module:
		    <ul>
		    <li> cuddZddUniqueCompare()
		    <li> cuddZddSymmSiftingAux()
		    <li> cuddZddSymmSiftingConvAux()
		    <li> cuddZddSymmSifting_up()
		    <li> cuddZddSymmSifting_down()
		    <li> zdd_group_move()
		    <li> cuddZddSymmSiftingBackward()
		    <li> zdd_group_move_backward()
		    </ul>
	      ]

  SeeAlso     [cuddSymmetry.c]

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

#define ZDD_MV_OOM (Move *)1

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddSymm.c,v 1.31 2012/02/05 01:07:19 fabio Exp $";
#endif

extern int   	*zdd_entry;

extern int	zddTotalNumberSwapping;

static DdNode	*empty;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int cuddZddSymmSiftingAux (DdManager *table, int x, int x_low, int x_high);
static int cuddZddSymmSiftingConvAux (DdManager *table, int x, int x_low, int x_high);
static Move * cuddZddSymmSifting_up (DdManager *table, int x, int x_low, int initial_size);
static Move * cuddZddSymmSifting_down (DdManager *table, int x, int x_high, int initial_size);
static int cuddZddSymmSiftingBackward (DdManager *table, Move *moves, int size);
static int zdd_group_move (DdManager *table, int x, int y, Move **moves);
static int zdd_group_move_backward (DdManager *table, int x, int y);
static void cuddZddSymmSummary (DdManager *table, int lower, int upper, int *symvars, int *symgroups);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Prints statistics on symmetric ZDD variables.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
Cudd_zddSymmProfile(
  DdManager * table,
  int  lower,
  int  upper)
{
    int		i, x, gbot;
    int		TotalSymm = 0;
    int 	TotalSymmGroups = 0;

    for (i = lower; i < upper; i++) {
	if (table->subtableZ[i].next != (unsigned) i) {
	    x = i;
	    (void) fprintf(table->out,"Group:");
	    do {
		(void) fprintf(table->out,"  %d", table->invpermZ[x]);
		TotalSymm++;
		gbot = x;
		x = table->subtableZ[x].next;
	    } while (x != i);
	    TotalSymmGroups++;
#ifdef DD_DEBUG
	    assert(table->subtableZ[gbot].next == (unsigned) i);
#endif
	    i = gbot;
	    (void) fprintf(table->out,"\n");
	}
    }
    (void) fprintf(table->out,"Total Symmetric = %d\n", TotalSymm);
    (void) fprintf(table->out,"Total Groups = %d\n", TotalSymmGroups);

} /* end of Cudd_zddSymmProfile */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Checks for symmetry of x and y.]

  Description [Checks for symmetry of x and y. Ignores projection
  functions, unless they are isolated. Returns 1 in case of
  symmetry; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddZddSymmCheck(
  DdManager * table,
  int  x,
  int  y)
{
    int		i;
    DdNode	*f, *f0, *f1, *f01, *f00, *f11, *f10;
    int		yindex;
    int 	xsymmy = 1;
    int		xsymmyp = 1;
    int 	arccount = 0;
    int 	TotalRefCount = 0;
    int 	symm_found;

    empty = table->zero;

    yindex = table->invpermZ[y];
    for (i = table->subtableZ[x].slots - 1; i >= 0; i--) {
	f = table->subtableZ[x].nodelist[i];
	while (f != NULL) {
	    /* Find f1, f0, f11, f10, f01, f00 */
	    f1 = cuddT(f);
	    f0 = cuddE(f);
	    if ((int) f1->index == yindex) {
		f11 = cuddT(f1);
		f10 = cuddE(f1);
		if (f10 != empty)
		    arccount++;
	    } else {
		if ((int) f0->index != yindex) {
		    return(0); /* f bypasses layer y */
		}
		f11 = empty;
		f10 = f1;
	    }
	    if ((int) f0->index == yindex) {
		f01 = cuddT(f0);
		f00 = cuddE(f0);
		if (f00 != empty)
		    arccount++;
	    } else {
		f01 = empty;
		f00 = f0;
	    }
	    if (f01 != f10)
		xsymmy = 0;
	    if (f11 != f00)
		xsymmyp = 0;
	    if ((xsymmy == 0) && (xsymmyp == 0))
		return(0);

	    f = f->next;
	} /* for each element of the collision list */
    } /* for each slot of the subtable */

    /* Calculate the total reference counts of y
    ** whose else arc is not empty.
    */
    for (i = table->subtableZ[y].slots - 1; i >= 0; i--) {
	f = table->subtableZ[y].nodelist[i];
	while (f != NIL(DdNode)) {
	    if (cuddE(f) != empty)
		TotalRefCount += f->ref;
	    f = f->next;
	}
    }

    symm_found = (arccount == TotalRefCount);
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
    if (symm_found) {
	int xindex = table->invpermZ[x];
	(void) fprintf(table->out,
		       "Found symmetry! x =%d\ty = %d\tPos(%d,%d)\n",
		       xindex,yindex,x,y);
    }
#endif

    return(symm_found);

} /* end cuddZddSymmCheck */


/**Function********************************************************************

  Synopsis [Symmetric sifting algorithm for ZDDs.]

  Description [Symmetric sifting algorithm.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries in
    each unique subtable.
    <li> Sift the variable up and down, remembering each time the total
    size of the ZDD heap and grouping variables that are symmetric.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 plus the number of symmetric variables if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddZddSymmSiftingConv]

******************************************************************************/
int
cuddZddSymmSifting(
  DdManager * table,
  int  lower,
  int  upper)
{
    int		i;
    int		*var;
    int		nvars;
    int		x;
    int		result;
    int		symvars;
    int		symgroups;
    int		iteration;
#ifdef DD_STATS
    int		previousSize;
#endif

    nvars = table->sizeZ;

    /* Find order in which to sift variables. */
    var = NULL;
    zdd_entry = ALLOC(int, nvars);
    if (zdd_entry == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddZddSymmSiftingOutOfMem;
    }
    var = ALLOC(int, nvars);
    if (var == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddZddSymmSiftingOutOfMem;
    }

    for (i = 0; i < nvars; i++) {
	x = table->permZ[i];
	zdd_entry[i] = table->subtableZ[x].keys;
	var[i] = i;
    }

    qsort((void *)var, nvars, sizeof(int), (DD_QSFP)cuddZddUniqueCompare);

    /* Initialize the symmetry of each subtable to itself. */
    for (i = lower; i <= upper; i++)
	table->subtableZ[i].next = i;

    iteration = ddMin(table->siftMaxVar, nvars);
    for (i = 0; i < iteration; i++) {
	if (zddTotalNumberSwapping >= table->siftMaxSwap)
	    break;
        if (util_cpu_time() - table->startTime > table->timeLimit) {
            table->autoDynZ = 0; /* prevent further reordering */
            break;
        }
	x = table->permZ[var[i]];
#ifdef DD_STATS
	previousSize = table->keysZ;
#endif
	if (x < lower || x > upper) continue;
	if (table->subtableZ[x].next == (unsigned) x) {
	    result = cuddZddSymmSiftingAux(table, x, lower, upper);
	    if (!result)
		goto cuddZddSymmSiftingOutOfMem;
#ifdef DD_STATS
	    if (table->keysZ < (unsigned) previousSize) {
		(void) fprintf(table->out,"-");
	    } else if (table->keysZ > (unsigned) previousSize) {
		(void) fprintf(table->out,"+");
#ifdef DD_VERBOSE
		(void) fprintf(table->out,"\nSize increased from %d to %d while sifting variable %d\n", previousSize, table->keysZ, var[i]);
#endif
	    } else {
		(void) fprintf(table->out,"=");
	    }
	    fflush(table->out);
#endif
	}
    }

    FREE(var);
    FREE(zdd_entry);

    cuddZddSymmSummary(table, lower, upper, &symvars, &symgroups);

#ifdef DD_STATS
    (void) fprintf(table->out,"\n#:S_SIFTING %8d: symmetric variables\n",symvars);
    (void) fprintf(table->out,"#:G_SIFTING %8d: symmetric groups\n",symgroups);
#endif

    return(1+symvars);

cuddZddSymmSiftingOutOfMem:

    if (zdd_entry != NULL)
	FREE(zdd_entry);
    if (var != NULL)
	FREE(var);

    return(0);

} /* end of cuddZddSymmSifting */


/**Function********************************************************************

  Synopsis [Symmetric sifting to convergence algorithm for ZDDs.]

  Description [Symmetric sifting to convergence algorithm for ZDDs.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries in
    each unique subtable.
    <li> Sift the variable up and down, remembering each time the total
    size of the ZDD heap and grouping variables that are symmetric.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    <li> Repeat 1-4 until no further improvement.
    </ol>
  Returns 1 plus the number of symmetric variables if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddZddSymmSifting]

******************************************************************************/
int
cuddZddSymmSiftingConv(
  DdManager * table,
  int  lower,
  int  upper)
{
    int		i;
    int		*var;
    int		nvars;
    int		initialSize;
    int		x;
    int		result;
    int		symvars;
    int		symgroups;
    int		classes;
    int		iteration;
#ifdef DD_STATS
    int         previousSize;
#endif

    initialSize = table->keysZ;

    nvars = table->sizeZ;

    /* Find order in which to sift variables. */
    var = NULL;
    zdd_entry = ALLOC(int, nvars);
    if (zdd_entry == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddZddSymmSiftingConvOutOfMem;
    }
    var = ALLOC(int, nvars);
    if (var == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	goto cuddZddSymmSiftingConvOutOfMem;
    }

    for (i = 0; i < nvars; i++) {
	x = table->permZ[i];
	zdd_entry[i] = table->subtableZ[x].keys;
	var[i] = i;
    }

    qsort((void *)var, nvars, sizeof(int), (DD_QSFP)cuddZddUniqueCompare);

    /* Initialize the symmetry of each subtable to itself
    ** for first pass of converging symmetric sifting.
    */
    for (i = lower; i <= upper; i++)
	table->subtableZ[i].next = i;

    iteration = ddMin(table->siftMaxVar, table->sizeZ);
    for (i = 0; i < iteration; i++) {
	if (zddTotalNumberSwapping >= table->siftMaxSwap)
	    break;
        if (util_cpu_time() - table->startTime > table->timeLimit) {
            table->autoDynZ = 0; /* prevent further reordering */
            break;
        }
	x = table->permZ[var[i]];
	if (x < lower || x > upper) continue;
	/* Only sift if not in symmetry group already. */
	if (table->subtableZ[x].next == (unsigned) x) {
#ifdef DD_STATS
	    previousSize = table->keysZ;
#endif
	    result = cuddZddSymmSiftingAux(table, x, lower, upper);
	    if (!result)
		goto cuddZddSymmSiftingConvOutOfMem;
#ifdef DD_STATS
	    if (table->keysZ < (unsigned) previousSize) {
		(void) fprintf(table->out,"-");
	    } else if (table->keysZ > (unsigned) previousSize) {
		(void) fprintf(table->out,"+");
#ifdef DD_VERBOSE
		(void) fprintf(table->out,"\nSize increased from %d to %d while sifting variable %d\n", previousSize, table->keysZ, var[i]);
#endif
	    } else {
		(void) fprintf(table->out,"=");
	    }
	    fflush(table->out);
#endif
	}
    }

    /* Sifting now until convergence. */
    while ((unsigned) initialSize > table->keysZ) {
	initialSize = table->keysZ;
#ifdef DD_STATS
	(void) fprintf(table->out,"\n");
#endif
	/* Here we consider only one representative for each symmetry class. */
	for (x = lower, classes = 0; x <= upper; x++, classes++) {
	    while ((unsigned) x < table->subtableZ[x].next)
		x = table->subtableZ[x].next;
	    /* Here x is the largest index in a group.
	    ** Groups consists of adjacent variables.
	    ** Hence, the next increment of x will move it to a new group.
	    */
	    i = table->invpermZ[x];
	    zdd_entry[i] = table->subtableZ[x].keys;
	    var[classes] = i;
	}

	qsort((void *)var,classes,sizeof(int),(DD_QSFP)cuddZddUniqueCompare);

	/* Now sift. */
	iteration = ddMin(table->siftMaxVar, nvars);
	for (i = 0; i < iteration; i++) {
	    if (zddTotalNumberSwapping >= table->siftMaxSwap)
		break;
            if (util_cpu_time() - table->startTime > table->timeLimit) {
              table->autoDynZ = 0; /* prevent further reordering */
              break;
            }
	    x = table->permZ[var[i]];
	    if ((unsigned) x >= table->subtableZ[x].next) {
#ifdef DD_STATS
		previousSize = table->keysZ;
#endif
		result = cuddZddSymmSiftingConvAux(table, x, lower, upper);
		if (!result)
		    goto cuddZddSymmSiftingConvOutOfMem;
#ifdef DD_STATS
		if (table->keysZ < (unsigned) previousSize) {
		    (void) fprintf(table->out,"-");
		} else if (table->keysZ > (unsigned) previousSize) {
		    (void) fprintf(table->out,"+");
#ifdef DD_VERBOSE
		(void) fprintf(table->out,"\nSize increased from %d to %d while sifting variable %d\n", previousSize, table->keysZ, var[i]);
#endif
		} else {
		    (void) fprintf(table->out,"=");
		}
		fflush(table->out);
#endif
	    }
	} /* for */
    }

    cuddZddSymmSummary(table, lower, upper, &symvars, &symgroups);

#ifdef DD_STATS
    (void) fprintf(table->out,"\n#:S_SIFTING %8d: symmetric variables\n",
		   symvars);
    (void) fprintf(table->out,"#:G_SIFTING %8d: symmetric groups\n",
		   symgroups);
#endif

    FREE(var);
    FREE(zdd_entry);

    return(1+symvars);

cuddZddSymmSiftingConvOutOfMem:

    if (zdd_entry != NULL)
	FREE(zdd_entry);
    if (var != NULL)
	FREE(var);

    return(0);

} /* end of cuddZddSymmSiftingConv */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Given x_low <= x <= x_high moves x up and down between the
  boundaries.]

  Description [Given x_low <= x <= x_high moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Assumes that x is not part of a symmetry group. Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddZddSymmSiftingAux(
  DdManager * table,
  int  x,
  int  x_low,
  int  x_high)
{
    Move *move;
    Move *move_up;	/* list of up move */
    Move *move_down;	/* list of down move */
    int	 initial_size;
    int	 result;
    int	 i;
    int  topbot;	/* index to either top or bottom of symmetry group */
    int	 init_group_size, final_group_size;

    initial_size = table->keysZ;

    move_down = NULL;
    move_up = NULL;

    /* Look for consecutive symmetries above x. */
    for (i = x; i > x_low; i--) {
	if (!cuddZddSymmCheck(table, i - 1, i))
            break;
	/* find top of i-1's symmetry */
	topbot = table->subtableZ[i - 1].next;
	table->subtableZ[i - 1].next = i;
	table->subtableZ[x].next = topbot;
	/* x is bottom of group so its symmetry is top of i-1's
	   group */
	i = topbot + 1; /* add 1 for i--, new i is top of symm group */
    }
    /* Look for consecutive symmetries below x. */
    for (i = x; i < x_high; i++) {
	if (!cuddZddSymmCheck(table, i, i + 1))
            break;
	/* find bottom of i+1's symm group */
	topbot = i + 1;
	while ((unsigned) topbot < table->subtableZ[topbot].next)
	    topbot = table->subtableZ[topbot].next;

	table->subtableZ[topbot].next = table->subtableZ[i].next;
	table->subtableZ[i].next = i + 1;
	i = topbot - 1; /* add 1 for i++,
			   new i is bottom of symm group */
    }

    /* Now x maybe in the middle of a symmetry group. */
    if (x == x_low) { /* Sift down */
	/* Find bottom of x's symm group */
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;

	i = table->subtableZ[x].next;
	init_group_size = x - i + 1;

	move_down = cuddZddSymmSifting_down(table, x, x_high,
	    initial_size);
	/* after that point x --> x_high, unless early term */
	if (move_down == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingAuxOutOfMem;

	if (move_down == NULL ||
	    table->subtableZ[move_down->y].next != move_down->y) {
	    /* symmetry detected may have to make another complete
	       pass */
            if (move_down != NULL)
		x = move_down->y;
	    else
		x = table->subtableZ[x].next;
	    i = x;
	    while ((unsigned) i < table->subtableZ[i].next) {
		i = table->subtableZ[i].next;
	    }
	    final_group_size = i - x + 1;

	    if (init_group_size == final_group_size) {
		/* No new symmetry groups detected,
		   return to best position */
		result = cuddZddSymmSiftingBackward(table,
		    move_down, initial_size);
	    }
	    else {
		initial_size = table->keysZ;
		move_up = cuddZddSymmSifting_up(table, x, x_low,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_down,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingAuxOutOfMem;
    }
    else if (x == x_high) { /* Sift up */
	/* Find top of x's symm group */
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;
	x = table->subtableZ[x].next;

	i = x;
	while ((unsigned) i < table->subtableZ[i].next) {
	    i = table->subtableZ[i].next;
	}
	init_group_size = i - x + 1;

	move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
	/* after that point x --> x_low, unless early term */
	if (move_up == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingAuxOutOfMem;

	if (move_up == NULL ||
	    table->subtableZ[move_up->x].next != move_up->x) {
	    /* symmetry detected may have to make another complete
		pass */
            if (move_up != NULL)
		x = move_up->x;
	    else {
		while ((unsigned) x < table->subtableZ[x].next)
		    x = table->subtableZ[x].next;
	    }
	    i = table->subtableZ[x].next;
	    final_group_size = x - i + 1;

	    if (init_group_size == final_group_size) {
		/* No new symmetry groups detected,
		   return to best position */
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	    else {
		initial_size = table->keysZ;
		move_down = cuddZddSymmSifting_down(table, x, x_high,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_up,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingAuxOutOfMem;
    }
    else if ((x - x_low) > (x_high - x)) { /* must go down first:
						shorter */
	/* Find bottom of x's symm group */
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;

	move_down = cuddZddSymmSifting_down(table, x, x_high,
	    initial_size);
	/* after that point x --> x_high, unless early term */
	if (move_down == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingAuxOutOfMem;

	if (move_down != NULL) {
	    x = move_down->y;
	}
	else {
	    x = table->subtableZ[x].next;
	}
	i = x;
	while ((unsigned) i < table->subtableZ[i].next) {
	    i = table->subtableZ[i].next;
	}
	init_group_size = i - x + 1;

	move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
	if (move_up == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingAuxOutOfMem;

	if (move_up == NULL ||
	    table->subtableZ[move_up->x].next != move_up->x) {
	    /* symmetry detected may have to make another complete
	       pass */
	    if (move_up != NULL) {
		x = move_up->x;
	    }
	    else {
		while ((unsigned) x < table->subtableZ[x].next)
		    x = table->subtableZ[x].next;
	    }
	    i = table->subtableZ[x].next;
	    final_group_size = x - i + 1;

	    if (init_group_size == final_group_size) {
		/* No new symmetry groups detected,
		   return to best position */
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	    else {
		while (move_down != NULL) {
		    move = move_down->next;
		    cuddDeallocMove(table, move_down);
		    move_down = move;
		}
		initial_size = table->keysZ;
		move_down = cuddZddSymmSifting_down(table, x, x_high,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_up,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingAuxOutOfMem;
    }
    else { /* moving up first:shorter */
        /* Find top of x's symmetry group */
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;
	x = table->subtableZ[x].next;

	move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
	/* after that point x --> x_high, unless early term */
	if (move_up == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingAuxOutOfMem;

	if (move_up != NULL) {
	    x = move_up->x;
	}
	else {
	    while ((unsigned) x < table->subtableZ[x].next)
		x = table->subtableZ[x].next;
	}
	i = table->subtableZ[x].next;
	init_group_size = x - i + 1;

	move_down = cuddZddSymmSifting_down(table, x, x_high,
	    initial_size);
	if (move_down == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingAuxOutOfMem;

	if (move_down == NULL ||
	    table->subtableZ[move_down->y].next != move_down->y) {
	    /* symmetry detected may have to make another complete
	       pass */
            if (move_down != NULL) {
		x = move_down->y;
	    }
	    else {
		x = table->subtableZ[x].next;
	    }
	    i = x;
	    while ((unsigned) i < table->subtableZ[i].next) {
		i = table->subtableZ[i].next;
	    }
	    final_group_size = i - x + 1;

	    if (init_group_size == final_group_size) {
		/* No new symmetries detected,
		   go back to best position */
		result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
	    }
	    else {
		while (move_up != NULL) {
		    move = move_up->next;
		    cuddDeallocMove(table, move_up);
		    move_up = move;
		}
		initial_size = table->keysZ;
		move_up = cuddZddSymmSifting_up(table, x, x_low,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_down,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingAuxOutOfMem;
    }

    while (move_down != NULL) {
	move = move_down->next;
	cuddDeallocMove(table, move_down);
	move_down = move;
    }
    while (move_up != NULL) {
	move = move_up->next;
	cuddDeallocMove(table, move_up);
	move_up = move;
    }

    return(1);

cuddZddSymmSiftingAuxOutOfMem:
    if (move_down != ZDD_MV_OOM) {
	while (move_down != NULL) {
	    move = move_down->next;
	    cuddDeallocMove(table, move_down);
	    move_down = move;
	}
    }
    if (move_up != ZDD_MV_OOM) {
	while (move_up != NULL) {
	    move = move_up->next;
	    cuddDeallocMove(table, move_up);
	    move_up = move;
	}
    }

    return(0);

} /* end of cuddZddSymmSiftingAux */


/**Function********************************************************************

  Synopsis [Given x_low <= x <= x_high moves x up and down between the
  boundaries.]

  Description [Given x_low <= x <= x_high moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Assumes that x is either an isolated variable, or it is the bottom of
  a symmetry group. All symmetries may not have been found, because of
  exceeded growth limit. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddZddSymmSiftingConvAux(
  DdManager * table,
  int  x,
  int  x_low,
  int  x_high)
{
    Move	*move;
    Move	*move_up;	/* list of up move */
    Move	*move_down;	/* list of down move */
    int		initial_size;
    int		result;
    int		i;
    int		init_group_size, final_group_size;

    initial_size = table->keysZ;

    move_down = NULL;
    move_up = NULL;

    if (x == x_low) { /* Sift down */
        i = table->subtableZ[x].next;
	init_group_size = x - i + 1;

	move_down = cuddZddSymmSifting_down(table, x, x_high,
	    initial_size);
	/* after that point x --> x_high, unless early term */
	if (move_down == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;

	if (move_down == NULL ||
	    table->subtableZ[move_down->y].next != move_down->y) {
	    /* symmetry detected may have to make another complete
		pass */
            if (move_down != NULL)
		x = move_down->y;
	    else {
		while ((unsigned) x < table->subtableZ[x].next)
		    x = table->subtableZ[x].next;
		x = table->subtableZ[x].next;
	    }
	    i = x;
	    while ((unsigned) i < table->subtableZ[i].next) {
		i = table->subtableZ[i].next;
	    }
	    final_group_size = i - x + 1;

	    if (init_group_size == final_group_size) {
		/* No new symmetries detected,
		   go back to best position */
		result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
	    }
	    else {
		initial_size = table->keysZ;
		move_up = cuddZddSymmSifting_up(table, x, x_low,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_down,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;
    }
    else if (x == x_high) { /* Sift up */
	/* Find top of x's symm group */
	while ((unsigned) x < table->subtableZ[x].next)
	    x = table->subtableZ[x].next;
	x = table->subtableZ[x].next;

	i = x;
	while ((unsigned) i < table->subtableZ[i].next) {
	    i = table->subtableZ[i].next;
	}
	init_group_size = i - x + 1;

	move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
	/* after that point x --> x_low, unless early term */
	if (move_up == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;

	if (move_up == NULL ||
	    table->subtableZ[move_up->x].next != move_up->x) {
	    /* symmetry detected may have to make another complete
	       pass */
            if (move_up != NULL)
		x = move_up->x;
	    else {
		while ((unsigned) x < table->subtableZ[x].next)
		    x = table->subtableZ[x].next;
	    }
	    i = table->subtableZ[x].next;
	    final_group_size = x - i + 1;

	    if (init_group_size == final_group_size) {
		/* No new symmetry groups detected,
		   return to best position */
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	    else {
		initial_size = table->keysZ;
		move_down = cuddZddSymmSifting_down(table, x, x_high,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_up,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;
    }
    else if ((x - x_low) > (x_high - x)) { /* must go down first:
						shorter */
	move_down = cuddZddSymmSifting_down(table, x, x_high,
	    initial_size);
	/* after that point x --> x_high */
	if (move_down == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;

	if (move_down != NULL) {
	    x = move_down->y;
	}
	else {
	    while ((unsigned) x < table->subtableZ[x].next)
		x = table->subtableZ[x].next;
	    x = table->subtableZ[x].next;
	}
	i = x;
	while ((unsigned) i < table->subtableZ[i].next) {
	    i = table->subtableZ[i].next;
	}
	init_group_size = i - x + 1;

	move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
	if (move_up == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;

	if (move_up == NULL ||
	    table->subtableZ[move_up->x].next != move_up->x) {
	    /* symmetry detected may have to make another complete
	       pass */
	    if (move_up != NULL) {
		x = move_up->x;
	    }
	    else {
		while ((unsigned) x < table->subtableZ[x].next)
		    x = table->subtableZ[x].next;
	    }
            i = table->subtableZ[x].next;
            final_group_size = x - i + 1;

            if (init_group_size == final_group_size) {
		/* No new symmetry groups detected,
		   return to best position */
                result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
            }
	    else {
		while (move_down != NULL) {
		    move = move_down->next;
		    cuddDeallocMove(table, move_down);
		    move_down = move;
		}
		initial_size = table->keysZ;
		move_down = cuddZddSymmSifting_down(table, x, x_high,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_up,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;
    }
    else { /* moving up first:shorter */
	/* Find top of x's symmetry group */
	x = table->subtableZ[x].next;

	move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
	/* after that point x --> x_high, unless early term */
	if (move_up == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;

	if (move_up != NULL) {
	    x = move_up->x;
	}
	else {
	    while ((unsigned) x < table->subtableZ[x].next)
		x = table->subtableZ[x].next;
	}
        i = table->subtableZ[x].next;
        init_group_size = x - i + 1;

	move_down = cuddZddSymmSifting_down(table, x, x_high,
	    initial_size);
	if (move_down == ZDD_MV_OOM)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;

	if (move_down == NULL ||
	    table->subtableZ[move_down->y].next != move_down->y) {
	    /* symmetry detected may have to make another complete
	       pass */
            if (move_down != NULL) {
		x = move_down->y;
	    }
	    else {
		while ((unsigned) x < table->subtableZ[x].next)
		    x = table->subtableZ[x].next;
		x = table->subtableZ[x].next;
	    }
            i = x;
            while ((unsigned) i < table->subtableZ[i].next) {
                i = table->subtableZ[i].next;
            }
	    final_group_size = i - x + 1;

            if (init_group_size == final_group_size) {
		/* No new symmetries detected,
		   go back to best position */
                result = cuddZddSymmSiftingBackward(table, move_down,
		    initial_size);
            }
	    else {
		while (move_up != NULL) {
		    move = move_up->next;
		    cuddDeallocMove(table, move_up);
		    move_up = move;
		}
		initial_size = table->keysZ;
		move_up = cuddZddSymmSifting_up(table, x, x_low,
		    initial_size);
		result = cuddZddSymmSiftingBackward(table, move_up,
		    initial_size);
	    }
	}
	else {
	    result = cuddZddSymmSiftingBackward(table, move_down,
		initial_size);
	    /* move backward and stop at best position */
	}
	if (!result)
	    goto cuddZddSymmSiftingConvAuxOutOfMem;
    }

    while (move_down != NULL) {
	move = move_down->next;
	cuddDeallocMove(table, move_down);
	move_down = move;
    }
    while (move_up != NULL) {
	move = move_up->next;
	cuddDeallocMove(table, move_up);
	move_up = move;
    }

    return(1);

cuddZddSymmSiftingConvAuxOutOfMem:
    if (move_down != ZDD_MV_OOM) {
	while (move_down != NULL) {
	    move = move_down->next;
	    cuddDeallocMove(table, move_down);
	    move_down = move;
	}
    }
    if (move_up != ZDD_MV_OOM) {
	while (move_up != NULL) {
	    move = move_up->next;
	    cuddDeallocMove(table, move_up);
	    move_up = move;
	}
    }

    return(0);

} /* end of cuddZddSymmSiftingConvAux */


/**Function********************************************************************

  Synopsis [Moves x up until either it reaches the bound (x_low) or
  the size of the ZDD heap increases too much.]

  Description [Moves x up until either it reaches the bound (x_low) or
  the size of the ZDD heap increases too much. Assumes that x is the top
  of a symmetry group.  Checks x for symmetry to the adjacent
  variables. If symmetry is found, the symmetry group of x is merged
  with the symmetry group of the other variable. Returns the set of
  moves in case of success; ZDD_MV_OOM if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
cuddZddSymmSifting_up(
  DdManager * table,
  int  x,
  int  x_low,
  int  initial_size)
{
    Move	*moves;
    Move	*move;
    int		y;
    int		size;
    int		limit_size = initial_size;
    int		i, gytop;

    moves = NULL;
    y = cuddZddNextLow(table, x);
    while (y >= x_low) {
	gytop = table->subtableZ[y].next;
	if (cuddZddSymmCheck(table, y, x)) {
	    /* Symmetry found, attach symm groups */
	    table->subtableZ[y].next = x;
	    i = table->subtableZ[x].next;
	    while (table->subtableZ[i].next != (unsigned) x)
		i = table->subtableZ[i].next;
	    table->subtableZ[i].next = gytop;
	}
	else if ((table->subtableZ[x].next == (unsigned) x) &&
	    (table->subtableZ[y].next == (unsigned) y)) {
	    /* x and y have self symmetry */
	    size = cuddZddSwapInPlace(table, y, x);
	    if (size == 0)
		goto cuddZddSymmSifting_upOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto cuddZddSymmSifting_upOutOfMem;
	    move->x = y;
	    move->y = x;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    if ((double)size >
		(double)limit_size * table->maxGrowth)
		return(moves);
	    if (size < limit_size)
		limit_size = size;
	}
	else { /* Group move */
	    size = zdd_group_move(table, y, x, &moves);
	    if ((double)size >
		(double)limit_size * table->maxGrowth)
		return(moves);
	    if (size < limit_size)
		limit_size = size;
	}
	x = gytop;
	y = cuddZddNextLow(table, x);
    }

    return(moves);

cuddZddSymmSifting_upOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(ZDD_MV_OOM);

} /* end of cuddZddSymmSifting_up */


/**Function********************************************************************

  Synopsis [Moves x down until either it reaches the bound (x_high) or
  the size of the ZDD heap increases too much.]

  Description [Moves x down until either it reaches the bound (x_high)
  or the size of the ZDD heap increases too much. Assumes that x is the
  bottom of a symmetry group. Checks x for symmetry to the adjacent
  variables. If symmetry is found, the symmetry group of x is merged
  with the symmetry group of the other variable. Returns the set of
  moves in case of success; ZDD_MV_OOM if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
cuddZddSymmSifting_down(
  DdManager * table,
  int  x,
  int  x_high,
  int  initial_size)
{
    Move	*moves;
    Move	*move;
    int		y;
    int		size;
    int		limit_size = initial_size;
    int		i, gxtop, gybot;

    moves = NULL;
    y = cuddZddNextHigh(table, x);
    while (y <= x_high) {
	gybot = table->subtableZ[y].next;
	while (table->subtableZ[gybot].next != (unsigned) y)
	    gybot = table->subtableZ[gybot].next;
	if (cuddZddSymmCheck(table, x, y)) {
	    /* Symmetry found, attach symm groups */
	    gxtop = table->subtableZ[x].next;
	    table->subtableZ[x].next = y;
	    i = table->subtableZ[y].next;
	    while (table->subtableZ[i].next != (unsigned) y)
		i = table->subtableZ[i].next;
	    table->subtableZ[i].next = gxtop;
	}
	else if ((table->subtableZ[x].next == (unsigned) x) &&
	    (table->subtableZ[y].next == (unsigned) y)) {
	    /* x and y have self symmetry */
	    size = cuddZddSwapInPlace(table, x, y);
	    if (size == 0)
		goto cuddZddSymmSifting_downOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL)
		goto cuddZddSymmSifting_downOutOfMem;
	    move->x = x;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    if ((double)size >
		(double)limit_size * table->maxGrowth)
		return(moves);
	    if (size < limit_size)
		limit_size = size;
	    x = y;
	    y = cuddZddNextHigh(table, x);
	}
	else { /* Group move */
	    size = zdd_group_move(table, x, y, &moves);
	    if ((double)size >
		(double)limit_size * table->maxGrowth)
		return(moves);
	    if (size < limit_size)
		limit_size = size;
	}
	x = gybot;
	y = cuddZddNextHigh(table, x);
    }

    return(moves);

cuddZddSymmSifting_downOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(ZDD_MV_OOM);

} /* end of cuddZddSymmSifting_down */


/**Function********************************************************************

  Synopsis [Given a set of moves, returns the ZDD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the ZDD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
cuddZddSymmSiftingBackward(
  DdManager * table,
  Move * moves,
  int  size)
{
    int		i;
    int		i_best;
    Move	*move;
    int		res;

    i_best = -1;
    for (move = moves, i = 0; move != NULL; move = move->next, i++) {
	if (move->size < size) {
	    i_best = i;
	    size = move->size;
	}
    }

    for (move = moves, i = 0; move != NULL; move = move->next, i++) {
	if (i == i_best) break;
	if ((table->subtableZ[move->x].next == move->x) &&
	    (table->subtableZ[move->y].next == move->y)) {
	    res = cuddZddSwapInPlace(table, move->x, move->y);
	    if (!res) return(0);
	}
	else { /* Group move necessary */
	    res = zdd_group_move_backward(table, move->x, move->y);
	}
	if (i_best == -1 && res == size)
	    break;
    }

    return(1);

} /* end of cuddZddSymmSiftingBackward */


/**Function********************************************************************

  Synopsis [Swaps two groups.]

  Description [Swaps two groups. x is assumed to be the bottom variable
  of the first group. y is assumed to be the top variable of the second
  group.  Updates the list of moves. Returns the number of keys in the
  table if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
zdd_group_move(
  DdManager * table,
  int  x,
  int  y,
  Move ** moves)
{
    Move	*move;
    int		size;
    int		i, temp, gxtop, gxbot, gybot, yprev;
    int		swapx, swapy;

#ifdef DD_DEBUG
    assert(x < y);	/* we assume that x < y */
#endif
    /* Find top and bottom for the two groups. */
    gxtop = table->subtableZ[x].next;
    gxbot = x;
    gybot = table->subtableZ[y].next;
    while (table->subtableZ[gybot].next != (unsigned) y)
	gybot = table->subtableZ[gybot].next;
    yprev = gybot;

    while (x <= y) {
	while (y > gxtop) {
	    /* Set correct symmetries. */
	    temp = table->subtableZ[x].next;
	    if (temp == x)
		temp = y;
	    i = gxtop;
	    for (;;) {
		if (table->subtableZ[i].next == (unsigned) x) {
		    table->subtableZ[i].next = y;
		    break;
		} else {
		    i = table->subtableZ[i].next;
		}
	    }
	    if (table->subtableZ[y].next != (unsigned) y) {
		table->subtableZ[x].next = table->subtableZ[y].next;
	    } else {
		table->subtableZ[x].next = x;
	    }

	    if (yprev != y) {
		table->subtableZ[yprev].next = x;
	    } else {
		yprev = x;
	    }
	    table->subtableZ[y].next = temp;

	    size = cuddZddSwapInPlace(table, x, y);
	    if (size == 0)
		goto zdd_group_moveOutOfMem;
            swapx = x;
	    swapy = y;
	    y = x;
	    x--;
	} /* while y > gxtop */

	/* Trying to find the next y. */
	if (table->subtableZ[y].next <= (unsigned) y) {
	    gybot = y;
	} else {
	    y = table->subtableZ[y].next;
	}

	yprev = gxtop;
	gxtop++;
	gxbot++;
	x = gxbot;
    } /* while x <= y, end of group movement */
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
	goto zdd_group_moveOutOfMem;
    move->x = swapx;
    move->y = swapy;
    move->size = table->keysZ;
    move->next = *moves;
    *moves = move;

    return(table->keysZ);

zdd_group_moveOutOfMem:
    while (*moves != NULL) {
	move = (*moves)->next;
	cuddDeallocMove(table, *moves);
	*moves = move;
    }
    return(0);

} /* end of zdd_group_move */


/**Function********************************************************************

  Synopsis [Undoes the swap of two groups.]

  Description [Undoes the swap of two groups. x is assumed to be the
  bottom variable of the first group. y is assumed to be the top
  variable of the second group.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
zdd_group_move_backward(
  DdManager * table,
  int  x,
  int  y)
{
    int	       size;
    int        i, temp, gxtop, gxbot, gybot, yprev;

#ifdef DD_DEBUG
    assert(x < y);	/* we assume that x < y */
#endif
    /* Find top and bottom of the two groups. */
    gxtop = table->subtableZ[x].next;
    gxbot = x;
    gybot = table->subtableZ[y].next;
    while (table->subtableZ[gybot].next != (unsigned) y)
	gybot = table->subtableZ[gybot].next;
    yprev = gybot;

    while (x <= y) {
	while (y > gxtop) {
	    /* Set correct symmetries. */
	    temp = table->subtableZ[x].next;
	    if (temp == x)
		temp = y;
	    i = gxtop;
	    for (;;) {
		if (table->subtableZ[i].next == (unsigned) x) {
		    table->subtableZ[i].next = y;
		    break;
		} else {
		    i = table->subtableZ[i].next;
		}
	    }
	    if (table->subtableZ[y].next != (unsigned) y) {
		table->subtableZ[x].next = table->subtableZ[y].next;
	    } else {
		table->subtableZ[x].next = x;
	    }

	    if (yprev != y) {
		table->subtableZ[yprev].next = x;
	    } else {
		yprev = x;
	    }
	    table->subtableZ[y].next = temp;

	    size = cuddZddSwapInPlace(table, x, y);
	    if (size == 0)
		return(0);
	    y = x;
	    x--;
	} /* while y > gxtop */

	/* Trying to find the next y. */
	if (table->subtableZ[y].next <= (unsigned) y) {
	    gybot = y;
	} else {
	    y = table->subtableZ[y].next;
	}

	yprev = gxtop;
	gxtop++;
	gxbot++;
	x = gxbot;
    } /* while x <= y, end of group movement backward */

    return(size);

} /* end of zdd_group_move_backward */


/**Function********************************************************************

  Synopsis    [Counts numbers of symmetric variables and symmetry
  groups.]

  Description []

  SideEffects [None]

******************************************************************************/
static void
cuddZddSymmSummary(
  DdManager * table,
  int  lower,
  int  upper,
  int * symvars,
  int * symgroups)
{
    int i,x,gbot;
    int TotalSymm = 0;
    int TotalSymmGroups = 0;

    for (i = lower; i <= upper; i++) {
	if (table->subtableZ[i].next != (unsigned) i) {
	    TotalSymmGroups++;
	    x = i;
	    do {
		TotalSymm++;
		gbot = x;
		x = table->subtableZ[x].next;
	    } while (x != i);
#ifdef DD_DEBUG
	    assert(table->subtableZ[gbot].next == (unsigned) i);
#endif
	    i = gbot;
	}
    }
    *symvars = TotalSymm;
    *symgroups = TotalSymmGroups;

    return;

} /* end of cuddZddSymmSummary */

