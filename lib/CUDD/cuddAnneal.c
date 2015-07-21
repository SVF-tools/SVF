/**CFile***********************************************************************

  FileName    [cuddAnneal.c]

  PackageName [cudd]

  Synopsis    [Reordering of DDs based on simulated annealing]

  Description [Internal procedures included in this file:
		<ul>
		<li> cuddAnnealing()
		</ul>
	       Static procedures included in this file:
		<ul>
		<li> stopping_criterion()
		<li> random_generator()
		<li> ddExchange()
		<li> ddJumpingAux()
		<li> ddJumpingUp()
		<li> ddJumpingDown()
		<li> siftBackwardProb()
		<li> copyOrder()
		<li> restoreOrder()
		</ul>
		]

  SeeAlso     []

  Author      [Jae-Young Jang, Jorgen Sivesind]

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

/* Annealing parameters */
#define BETA 0.6 
#define ALPHA 0.90
#define EXC_PROB 0.4 
#define JUMP_UP_PROB 0.36
#define MAXGEN_RATIO 15.0
#define STOP_TEMP 1.0

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
static char rcsid[] DD_UNUSED = "$Id: cuddAnneal.c,v 1.15 2012/02/05 01:07:18 fabio Exp $";
#endif

#ifdef DD_STATS
extern	int	ddTotalNumberSwapping;
extern	int	ddTotalNISwaps;
static	int	tosses;
static	int	acceptances;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int stopping_criterion (int c1, int c2, int c3, int c4, double temp);
static double random_generator (void);
static int ddExchange (DdManager *table, int x, int y, double temp);
static int ddJumpingAux (DdManager *table, int x, int x_low, int x_high, double temp);
static Move * ddJumpingUp (DdManager *table, int x, int x_low, int initial_size);
static Move * ddJumpingDown (DdManager *table, int x, int x_high, int initial_size);
static int siftBackwardProb (DdManager *table, Move *moves, int size, double temp);
static void copyOrder (DdManager *table, int *array, int lower, int upper);
static int restoreOrder (DdManager *table, int *array, int lower, int upper);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Get new variable-order by simulated annealing algorithm.]

  Description [Get x, y by random selection. Choose either
  exchange or jump randomly. In case of jump, choose between jump_up
  and jump_down randomly. Do exchange or jump and get optimal case.
  Loop until there is no improvement or temperature reaches
  minimum. Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddAnnealing(
  DdManager * table,
  int  lower,
  int  upper)
{
    int         nvars;
    int         size;
    int         x,y;
    int         result;
    int		c1, c2, c3, c4;
    int		BestCost;
    int		*BestOrder;
    double	NewTemp, temp;
    double	rand1;
    int         innerloop, maxGen;
    int         ecount, ucount, dcount;
   
    nvars = upper - lower + 1;

    result = cuddSifting(table,lower,upper);
#ifdef DD_STATS
    (void) fprintf(table->out,"\n");
#endif
    if (result == 0) return(0);

    size = table->keys - table->isolated;

    /* Keep track of the best order. */
    BestCost = size;
    BestOrder = ALLOC(int,nvars);
    if (BestOrder == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    copyOrder(table,BestOrder,lower,upper);

    temp = BETA * size;
    maxGen = (int) (MAXGEN_RATIO * nvars);

    c1 = size + 10;
    c2 = c1 + 10;
    c3 = size;
    c4 = c2 + 10;
    ecount = ucount = dcount = 0;
 
    while (!stopping_criterion(c1, c2, c3, c4, temp)) {
#ifdef DD_STATS
	(void) fprintf(table->out,"temp=%f\tsize=%d\tgen=%d\t",
		       temp,size,maxGen);
	tosses = acceptances = 0;
#endif
	for (innerloop = 0; innerloop < maxGen; innerloop++) {
	    /* Choose x, y  randomly. */
	    x = (int) Cudd_Random() % nvars;
	    do {
		y = (int) Cudd_Random() % nvars;
	    } while (x == y);
	    x += lower;
	    y += lower;
	    if (x > y) {
		int tmp = x;
		x = y;
		y = tmp;
	    }

	    /* Choose move with roulette wheel. */
	    rand1 = random_generator();
	    if (rand1 < EXC_PROB) {
		result = ddExchange(table,x,y,temp);       /* exchange */
		ecount++;
#if 0
		(void) fprintf(table->out,
			       "Exchange of %d and %d: size = %d\n",
			       x,y,table->keys - table->isolated);
#endif
	    } else if (rand1 < EXC_PROB + JUMP_UP_PROB) {
		result = ddJumpingAux(table,y,x,y,temp); /* jumping_up */
		ucount++;
#if 0
		(void) fprintf(table->out,
			       "Jump up of %d to %d: size = %d\n",
			       y,x,table->keys - table->isolated);
#endif
	    } else {
		result = ddJumpingAux(table,x,x,y,temp); /* jumping_down */
		dcount++;
#if 0
		(void) fprintf(table->out,
			       "Jump down of %d to %d: size = %d\n",
			       x,y,table->keys - table->isolated);
#endif
	    }

	    if (!result) {
		FREE(BestOrder);
		return(0);
	    }

	    size = table->keys - table->isolated;	/* keep current size */
	    if (size < BestCost) {			/* update best order */
		BestCost = size;
		copyOrder(table,BestOrder,lower,upper);
	    }
	}
	c1 = c2;
	c2 = c3;
	c3 = c4;
	c4 = size;
	NewTemp = ALPHA * temp;
	if (NewTemp >= 1.0) {
	    maxGen = (int)(log(NewTemp) / log(temp) * maxGen);
	}
	temp = NewTemp;	                /* control variable */
#ifdef DD_STATS
	(void) fprintf(table->out,"uphill = %d\taccepted = %d\n",
		       tosses,acceptances);
	fflush(table->out);
#endif
    }

    result = restoreOrder(table,BestOrder,lower,upper);
    FREE(BestOrder);
    if (!result) return(0);
#ifdef DD_STATS
    fprintf(table->out,"#:N_EXCHANGE %8d : total exchanges\n",ecount);
    fprintf(table->out,"#:N_JUMPUP   %8d : total jumps up\n",ucount);
    fprintf(table->out,"#:N_JUMPDOWN %8d : total jumps down",dcount);
#endif
    return(1);

} /* end of cuddAnnealing */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Checks termination condition.]

  Description [If temperature is STOP_TEMP or there is no improvement
  then terminates. Returns 1 if the termination criterion is met; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
stopping_criterion(
  int  c1,
  int  c2,
  int  c3,
  int  c4,
  double  temp)
{
    if (STOP_TEMP < temp) {
	return(0);
    } else if ((c1 == c2) && (c1 == c3) && (c1 == c4)) {
	return(1);
    } else {
	return(0);
    }

} /* end of stopping_criterion */


/**Function********************************************************************

  Synopsis    [Random number generator.]

  Description [Returns a double precision value between 0.0 and 1.0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static double
random_generator(void)
{
    return((double)(Cudd_Random() / 2147483561.0));

} /* end of random_generator */


/**Function********************************************************************

  Synopsis    [This function is for exchanging two variables, x and y.]

  Description [This is the same funcion as ddSwapping except for
  comparison expression.  Use probability function, exp(-size_change/temp).]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
ddExchange(
  DdManager * table,
  int  x,
  int  y,
  double  temp)
{
    Move       *move,*moves;
    int        tmp;
    int        x_ref,y_ref;
    int        x_next,y_next;
    int        size, result;
    int        initial_size, limit_size;

    x_ref = x;
    y_ref = y;

    x_next = cuddNextHigh(table,x);
    y_next = cuddNextLow(table,y);
    moves = NULL;
    initial_size = limit_size = table->keys - table->isolated;

    for (;;) {
	if (x_next == y_next) {
	    size = cuddSwapInPlace(table,x,x_next);
	    if (size == 0) goto ddExchangeOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddExchangeOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    size = cuddSwapInPlace(table,y_next,y);
	    if (size == 0) goto ddExchangeOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddExchangeOutOfMem;
	    move->x = y_next;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    size = cuddSwapInPlace(table,x,x_next);
	    if (size == 0) goto ddExchangeOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddExchangeOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;

	    tmp = x;
	    x = y;
	    y = tmp;
	} else if (x == y_next) {
	    size = cuddSwapInPlace(table,x,x_next);
	    if (size == 0) goto ddExchangeOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddExchangeOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    tmp = x;
	    x = y;
	    y = tmp;
	} else {
	    size = cuddSwapInPlace(table,x,x_next);
	    if (size == 0) goto ddExchangeOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddExchangeOutOfMem;
	    move->x = x;
	    move->y = x_next;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    size = cuddSwapInPlace(table,y_next,y);
	    if (size == 0) goto ddExchangeOutOfMem;
	    move = (Move *)cuddDynamicAllocNode(table);
	    if (move == NULL) goto ddExchangeOutOfMem;
	    move->x = y_next;
	    move->y = y;
	    move->size = size;
	    move->next = moves;
	    moves = move;
	    x = x_next;
	    y = y_next;
	}

	x_next = cuddNextHigh(table,x);
	y_next = cuddNextLow(table,y);
	if (x_next > y_ref) break;

	if ((double) size > DD_MAX_REORDER_GROWTH * (double) limit_size) {
	    break;
	} else if (size < limit_size) {
	    limit_size = size;
	}
    }

    if (y_next>=x_ref) {
        size = cuddSwapInPlace(table,y_next,y);
        if (size == 0) goto ddExchangeOutOfMem;
        move = (Move *)cuddDynamicAllocNode(table);
        if (move == NULL) goto ddExchangeOutOfMem;
        move->x = y_next;
        move->y = y;
        move->size = size;
        move->next = moves;
        moves = move;
    }

    /* move backward and stop at best position or accept uphill move */
    result = siftBackwardProb(table,moves,initial_size,temp);
    if (!result) goto ddExchangeOutOfMem;

    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(1);

ddExchangeOutOfMem:
    while (moves != NULL) {
        move = moves->next;
        cuddDeallocMove(table, moves);
        moves = move;
    }
    return(0);

} /* end of ddExchange */


/**Function********************************************************************

  Synopsis    [Moves a variable to a specified position.]

  Description [If x==x_low, it executes jumping_down. If x==x_high, it
  executes jumping_up. This funcion is similar to ddSiftingAux. Returns
  1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
ddJumpingAux(
  DdManager * table,
  int  x,
  int  x_low,
  int  x_high,
  double  temp)
{
    Move       *move;
    Move       *moves;        /* list of moves */
    int        initial_size;
    int        result;

    initial_size = table->keys - table->isolated;

#ifdef DD_DEBUG
    assert(table->subtables[x].keys > 0);
#endif

    moves = NULL;

    if (cuddNextLow(table,x) < x_low) {
	if (cuddNextHigh(table,x) > x_high) return(1);
	moves = ddJumpingDown(table,x,x_high,initial_size);
	/* after that point x --> x_high unless early termination */
	if (moves == NULL) goto ddJumpingAuxOutOfMem;
	/* move backward and stop at best position or accept uphill move */
	result = siftBackwardProb(table,moves,initial_size,temp);
	if (!result) goto ddJumpingAuxOutOfMem;
    } else if (cuddNextHigh(table,x) > x_high) {
	moves = ddJumpingUp(table,x,x_low,initial_size);
	/* after that point x --> x_low unless early termination */
	if (moves == NULL) goto ddJumpingAuxOutOfMem;
	/* move backward and stop at best position or accept uphill move */
	result = siftBackwardProb(table,moves,initial_size,temp);
	if (!result) goto ddJumpingAuxOutOfMem;
    } else {
	(void) fprintf(table->err,"Unexpected condition in ddJumping\n");
	goto ddJumpingAuxOutOfMem;
    }
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(1);

ddJumpingAuxOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(0);

} /* end of ddJumpingAux */


/**Function********************************************************************

  Synopsis    [This function is for jumping up.]

  Description [This is a simplified version of ddSiftingUp. It does not
  use lower bounding. Returns the set of moves in case of success; NULL
  if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
ddJumpingUp(
  DdManager * table,
  int  x,
  int  x_low,
  int  initial_size)
{
    Move       *moves;
    Move       *move;
    int        y;
    int        size;
    int        limit_size = initial_size;

    moves = NULL;
    y = cuddNextLow(table,x);
    while (y >= x_low) {
	size = cuddSwapInPlace(table,y,x);
	if (size == 0) goto ddJumpingUpOutOfMem;
	move = (Move *)cuddDynamicAllocNode(table);
	if (move == NULL) goto ddJumpingUpOutOfMem;
	move->x = y;
	move->y = x;
	move->size = size;
	move->next = moves;
	moves = move;
	if ((double) size > table->maxGrowth * (double) limit_size) {
	    break;
	} else if (size < limit_size) {
	    limit_size = size;
	}
	x = y;
	y = cuddNextLow(table,x);
    }
    return(moves);

ddJumpingUpOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(NULL);

} /* end of ddJumpingUp */


/**Function********************************************************************

  Synopsis    [This function is for jumping down.]

  Description [This is a simplified version of ddSiftingDown. It does not
  use lower bounding. Returns the set of moves in case of success; NULL
  if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *
ddJumpingDown(
  DdManager * table,
  int  x,
  int  x_high,
  int  initial_size)
{
    Move       *moves;
    Move       *move;
    int        y;
    int        size;
    int        limit_size = initial_size;

    moves = NULL;
    y = cuddNextHigh(table,x);
    while (y <= x_high) {
	size = cuddSwapInPlace(table,x,y);
	if (size == 0) goto ddJumpingDownOutOfMem;
	move = (Move *)cuddDynamicAllocNode(table);
	if (move == NULL) goto ddJumpingDownOutOfMem;
	move->x = x;
	move->y = y;
	move->size = size;
	move->next = moves;
	moves = move;
	if ((double) size > table->maxGrowth * (double) limit_size) {
	    break;
	} else if (size < limit_size) {
	    limit_size = size;
	}
	x = y;
	y = cuddNextHigh(table,x);
    }
    return(moves);

ddJumpingDownOutOfMem:
    while (moves != NULL) {
	move = moves->next;
	cuddDeallocMove(table, moves);
	moves = move;
    }
    return(NULL);

} /* end of ddJumpingDown */


/**Function********************************************************************

  Synopsis [Returns the DD to the best position encountered during
  sifting if there was improvement.]

  Description [Otherwise, "tosses a coin" to decide whether to keep
  the current configuration or return the DD to the original
  one. Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
siftBackwardProb(
  DdManager * table,
  Move * moves,
  int  size,
  double  temp)
{
    Move   *move;
    int    res;
    int    best_size = size;
    double coin, threshold;

    /* Look for best size during the last sifting */
    for (move = moves; move != NULL; move = move->next) {
	if (move->size < best_size) {
	    best_size = move->size;
	}
    }
    
    /* If best_size equals size, the last sifting did not produce any
    ** improvement. We now toss a coin to decide whether to retain
    ** this change or not.
    */
    if (best_size == size) {
	coin = random_generator();
#ifdef DD_STATS
	tosses++;
#endif
	threshold = exp(-((double)(table->keys - table->isolated - size))/temp);
	if (coin < threshold) {
#ifdef DD_STATS
	    acceptances++;
#endif
	    return(1);
	}
    }

    /* Either there was improvement, or we have decided not to
    ** accept the uphill move. Go to best position.
    */
    res = table->keys - table->isolated;
    for (move = moves; move != NULL; move = move->next) {
	if (res == best_size) return(1);
	res = cuddSwapInPlace(table,(int)move->x,(int)move->y);
	if (!res) return(0);
    }

    return(1);

} /* end of sift_backward_prob */


/**Function********************************************************************

  Synopsis    [Copies the current variable order to array.]

  Description [Copies the current variable order to array.
  At the same time inverts the permutation.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
copyOrder(
  DdManager * table,
  int * array,
  int  lower,
  int  upper)
{
    int i;
    int nvars;

    nvars = upper - lower + 1;
    for (i = 0; i < nvars; i++) {
	array[i] = table->invperm[i+lower];
    }

} /* end of copyOrder */


/**Function********************************************************************

  Synopsis    [Restores the variable order in array by a series of sifts up.]

  Description [Restores the variable order in array by a series of sifts up.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
restoreOrder(
  DdManager * table,
  int * array,
  int  lower,
  int  upper)
{
    int i, x, y, size;
    int nvars = upper - lower + 1;

    for (i = 0; i < nvars; i++) {
	x = table->perm[array[i]];
#ifdef DD_DEBUG
    assert(x >= lower && x <= upper);
#endif
	y = cuddNextLow(table,x);
	while (y >= i + lower) {
	    size = cuddSwapInPlace(table,y,x);
	    if (size == 0) return(0);
	    x = y;
	    y = cuddNextLow(table,x);
	}
    }

    return(1);

} /* end of restoreOrder */

