/**CFile***********************************************************************

  FileName    [cuddGenetic.c]

  PackageName [cudd]

  Synopsis    [Genetic algorithm for variable reordering.]

  Description [Internal procedures included in this file:
		<ul>
		<li> cuddGa()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> make_random()
		<li> sift_up()
		<li> build_dd()
		<li> largest()
		<li> rand_int()
		<li> array_hash()
		<li> array_compare()
		<li> find_best()
		<li> find_average_fitness()
		<li> PMX()
		<li> roulette()
		</ul>

  The genetic algorithm implemented here is as follows.  We start with
  the current DD order.  We sift this order and use this as the
  reference DD.  We only keep 1 DD around for the entire process and
  simply rearrange the order of this DD, storing the various orders
  and their corresponding DD sizes.  We generate more random orders to
  build an initial population. This initial population is 3 times the
  number of variables, with a maximum of 120. Each random order is
  built (from the reference DD) and its size stored.  Each random
  order is also sifted to keep the DD sizes fairly small.  Then a
  crossover is performed between two orders (picked randomly) and the
  two resulting DDs are built and sifted.  For each new order, if its
  size is smaller than any DD in the population, it is inserted into
  the population and the DD with the largest number of nodes is thrown
  out. The crossover process happens up to 50 times, and at this point
  the DD in the population with the smallest size is chosen as the
  result.  This DD must then be built from the reference DD.]

  SeeAlso     []

  Author      [Curt Musfeldt, Alan Shuler, Fabio Somenzi]

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
static char rcsid[] DD_UNUSED = "$Id: cuddGenetic.c,v 1.30 2012/02/05 01:07:18 fabio Exp $";
#endif

static int popsize;		/* the size of the population */
static int numvars;		/* the number of input variables in the ckt. */
/* storedd stores the population orders and sizes. This table has two
** extra rows and one extras column. The two extra rows are used for the
** offspring produced by a crossover. Each row stores one order and its
** size. The order is stored by storing the indices of variables in the
** order in which they appear in the order. The table is in reality a
** one-dimensional array which is accessed via a macro to give the illusion
** it is a two-dimensional structure.
*/
static int *storedd;
static st_table *computed;	/* hash table to identify existing orders */
static int *repeat;		/* how many times an order is present */
static int large;		/* stores the index of the population with
				** the largest number of nodes in the DD */
static int result;
static int cross;		/* the number of crossovers to perform */

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/* macro used to access the population table as if it were a
** two-dimensional structure.
*/
#define STOREDD(i,j)	storedd[(i)*(numvars+1)+(j)]

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int make_random (DdManager *table, int lower);
static int sift_up (DdManager *table, int x, int x_low);
static int build_dd (DdManager *table, int num, int lower, int upper);
static int largest (void);
static int rand_int (int a);
static int array_hash (char *array, int modulus);
static int array_compare (const char *array1, const char *array2);
static int find_best (void);
#ifdef DD_STATS
static double find_average_fitness (void);
#endif
static int PMX (int maxvar);
static int roulette (int *p1, int *p2);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Genetic algorithm for DD reordering.]

  Description [Genetic algorithm for DD reordering.
  The two children of a crossover will be stored in
  storedd[popsize] and storedd[popsize+1] --- the last two slots in the
  storedd array.  (This will make comparisons and replacement easy.)
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddGa(
  DdManager * table /* manager */,
  int  lower /* lowest level to be reordered */,
  int  upper /* highest level to be reorderded */)
{
    int 	i,n,m;		/* dummy/loop vars */
    int		index;
#ifdef DD_STATS
    double	average_fitness;
#endif
    int		small;		/* index of smallest DD in population */

    /* Do an initial sifting to produce at least one reasonable individual. */
    if (!cuddSifting(table,lower,upper)) return(0);

    /* Get the initial values. */
    numvars = upper - lower + 1; /* number of variables to be reordered */
    if (table->populationSize == 0) {
	popsize = 3 * numvars;  /* population size is 3 times # of vars */
	if (popsize > 120) {
	    popsize = 120;	/* Maximum population size is 120 */
	}
    } else {
	popsize = table->populationSize;  /* user specified value */
    }
    if (popsize < 4) popsize = 4;	/* enforce minimum population size */

    /* Allocate population table. */
    storedd = ALLOC(int,(popsize+2)*(numvars+1));
    if (storedd == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }

    /* Initialize the computed table. This table is made up of two data
    ** structures: A hash table with the key given by the order, which says
    ** if a given order is present in the population; and the repeat
    ** vector, which says how many copies of a given order are stored in
    ** the population table. If there are multiple copies of an order, only
    ** one has a repeat count greater than 1. This copy is the one pointed
    ** by the computed table.
    */
    repeat = ALLOC(int,popsize);
    if (repeat == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	FREE(storedd);
	return(0);
    }
    for (i = 0; i < popsize; i++) {
	repeat[i] = 0;
    }
    computed = st_init_table(array_compare,array_hash);
    if (computed == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	FREE(storedd);
	FREE(repeat);
	return(0);
    }

    /* Copy the current DD and its size to the population table. */
    for (i = 0; i < numvars; i++) {
	STOREDD(0,i) = table->invperm[i+lower]; /* order of initial DD */
    }
    STOREDD(0,numvars) = table->keys - table->isolated; /* size of initial DD */

    /* Store the initial order in the computed table. */
    if (st_insert(computed,(char *)storedd,(char *) 0) == ST_OUT_OF_MEM) {
	FREE(storedd);
	FREE(repeat);
	st_free_table(computed);
	return(0);
    }
    repeat[0]++;

    /* Insert the reverse order as second element of the population. */
    for (i = 0; i < numvars; i++) {
	STOREDD(1,numvars-1-i) = table->invperm[i+lower]; /* reverse order */
    }

    /* Now create the random orders. make_random fills the population
    ** table with random permutations. The successive loop builds and sifts
    ** the DDs for the reverse order and each random permutation, and stores
    ** the results in the computed table.
    */
    if (!make_random(table,lower)) {
	table->errorCode = CUDD_MEMORY_OUT;
	FREE(storedd);
	FREE(repeat);
	st_free_table(computed);
	return(0);
    }
    for (i = 1; i < popsize; i++) {
	result = build_dd(table,i,lower,upper);	/* build and sift order */
	if (!result) {
	    FREE(storedd);
	    FREE(repeat);
	    st_free_table(computed);
	    return(0);
	}
	if (st_lookup_int(computed,(char *)&STOREDD(i,0),&index)) {
	    repeat[index]++;
	} else {
	    if (st_insert(computed,(char *)&STOREDD(i,0),(char *)(long)i) ==
	    ST_OUT_OF_MEM) {
		FREE(storedd);
		FREE(repeat);
		st_free_table(computed);
		return(0);
	    }
	    repeat[i]++;
	}
    }

#if 0
#ifdef DD_STATS
    /* Print the initial population. */
    (void) fprintf(table->out,"Initial population after sifting\n");
    for (m = 0; m < popsize; m++) {
	for (i = 0; i < numvars; i++) {
	    (void) fprintf(table->out," %2d",STOREDD(m,i));
	}
	(void) fprintf(table->out," : %3d (%d)\n",
		       STOREDD(m,numvars),repeat[m]);
    }
#endif
#endif

    small = find_best();
#ifdef DD_STATS
    average_fitness = find_average_fitness();
    (void) fprintf(table->out,"\nInitial population: best fitness = %d, average fitness %8.3f",STOREDD(small,numvars),average_fitness);
#endif

    /* Decide how many crossovers should be tried. */
    if (table->numberXovers == 0) {
	cross = 3*numvars;
	if (cross > 60) {	/* do a maximum of 50 crossovers */
	    cross = 60;
	}
    } else {
	cross = table->numberXovers;      /* use user specified value */
    }
    if (cross >= popsize) {
	cross = popsize;
    }

    /* Perform the crossovers to get the best order. */
    for (m = 0; m < cross; m++) {
	if (!PMX(table->size)) {	/* perform one crossover */
	    table->errorCode = CUDD_MEMORY_OUT;
	    FREE(storedd);
	    FREE(repeat);
	    st_free_table(computed);
	    return(0);
	}
	/* The offsprings are left in the last two entries of the
	** population table. These are now considered in turn.
	*/
	for (i = popsize; i <= popsize+1; i++) {
	    result = build_dd(table,i,lower,upper); /* build and sift child */
	    if (!result) {
		FREE(storedd);
		FREE(repeat);
		st_free_table(computed);
		return(0);
	    }
	    large = largest();	/* find the largest DD in population */

	    /* If the new child is smaller than the largest DD in the current
	    ** population, enter it into the population in place of the
	    ** largest DD.
	    */
	    if (STOREDD(i,numvars) < STOREDD(large,numvars)) {
		/* Look up the largest DD in the computed table.
		** Decrease its repetition count. If the repetition count
		** goes to 0, remove the largest DD from the computed table.
		*/
		result = st_lookup_int(computed,(char *)&STOREDD(large,0),
				       &index);
		if (!result) {
		    FREE(storedd);
		    FREE(repeat);
		    st_free_table(computed);
		    return(0);
		}
		repeat[index]--;
		if (repeat[index] == 0) {
		    int *pointer = &STOREDD(index,0);
		    result = st_delete(computed, &pointer, NULL);
		    if (!result) {
			FREE(storedd);
			FREE(repeat);
			st_free_table(computed);
			return(0);
		    }
		}
		/* Copy the new individual to the entry of the
		** population table just made available and update the
		** computed table.
		*/
		for (n = 0; n <= numvars; n++) {
		    STOREDD(large,n) = STOREDD(i,n);
		}
		if (st_lookup_int(computed,(char *)&STOREDD(large,0),
				  &index)) {
		    repeat[index]++;
		} else {
		    if (st_insert(computed,(char *)&STOREDD(large,0),
		    (char *)(long)large) == ST_OUT_OF_MEM) {
			FREE(storedd);
			FREE(repeat);
			st_free_table(computed);
			return(0);
		    }
		    repeat[large]++;
		}
	    }
	}
    }

    /* Find the smallest DD in the population and build it;
    ** that will be the result.
    */
    small = find_best();

    /* Print stats on the final population. */
#ifdef DD_STATS
    average_fitness = find_average_fitness();
    (void) fprintf(table->out,"\nFinal population: best fitness = %d, average fitness %8.3f",STOREDD(small,numvars),average_fitness);
#endif

    /* Clean up, build the result DD, and return. */
    st_free_table(computed);
    computed = NULL;
    result = build_dd(table,small,lower,upper);
    FREE(storedd);
    FREE(repeat);
    return(result);

} /* end of cuddGa */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Generates the random sequences for the initial population.]

  Description [Generates the random sequences for the initial population.
  The sequences are permutations of the indices between lower and
  upper in the current order.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
make_random(
  DdManager * table,
  int  lower)
{
    int i,j;		/* loop variables */
    int	*used;		/* is a number already in a permutation */
    int	next;		/* next random number without repetitions */

    used = ALLOC(int,numvars);
    if (used == NULL) {
	table->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
#if 0
#ifdef DD_STATS
    (void) fprintf(table->out,"Initial population before sifting\n");
    for (i = 0; i < 2; i++) {
	for (j = 0; j < numvars; j++) {
	    (void) fprintf(table->out," %2d",STOREDD(i,j));
	}
	(void) fprintf(table->out,"\n");
    }
#endif
#endif
    for (i = 2; i < popsize; i++) {
       	for (j = 0; j < numvars; j++) {
	    used[j] = 0;
	}
	/* Generate a permutation of {0...numvars-1} and use it to
	** permute the variables in the layesr from lower to upper.
	*/
       	for (j = 0; j < numvars; j++) {
	    do {
		next = rand_int(numvars-1);
	    } while (used[next] != 0);
	    used[next] = 1;
	    STOREDD(i,j) = table->invperm[next+lower];
       	}
#if 0
#ifdef DD_STATS
	/* Print the order just generated. */
	for (j = 0; j < numvars; j++) {
	    (void) fprintf(table->out," %2d",STOREDD(i,j));
	}
	(void) fprintf(table->out,"\n");
#endif
#endif
    }
    FREE(used);
    return(1);

} /* end of make_random */


/**Function********************************************************************

  Synopsis    [Moves one variable up.]

  Description [Takes a variable from position x and sifts it up to
  position x_low;  x_low should be less than x. Returns 1 if successful;
  0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
sift_up(
  DdManager * table,
  int  x,
  int  x_low)
{
    int        y;
    int        size;

    y = cuddNextLow(table,x);
    while (y >= x_low) {
	size = cuddSwapInPlace(table,y,x);
	if (size == 0) {
	    return(0);
	}
	x = y;
	y = cuddNextLow(table,x);
    }
    return(1);

} /* end of sift_up */


/**Function********************************************************************

  Synopsis [Builds a DD from a given order.]

  Description [Builds a DD from a given order.  This procedure also
  sifts the final order and inserts into the array the size in nodes
  of the result. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
build_dd(
  DdManager * table,
  int  num /* the index of the individual to be built */,
  int  lower,
  int  upper)
{
    int 	i,j;		/* loop vars */
    int 	position;
    int		index;
    int		limit;		/* how large the DD for this order can grow */
    int		size;

    /* Check the computed table. If the order already exists, it
    ** suffices to copy the size from the existing entry.
    */
    if (computed && st_lookup_int(computed,(char *)&STOREDD(num,0),&index)) {
	STOREDD(num,numvars) = STOREDD(index,numvars);
#ifdef DD_STATS
	(void) fprintf(table->out,"\nCache hit for index %d", index);
#endif
	return(1);
    }

    /* Stop if the DD grows 20 times larges than the reference size. */
    limit = 20 * STOREDD(0,numvars);

    /* Sift up the variables so as to build the desired permutation.
    ** First the variable that has to be on top is sifted to the top.
    ** Then the variable that has to occupy the secon position is sifted
    ** up to the second position, and so on.
    */
    for (j = 0; j < numvars; j++) {
	i = STOREDD(num,j);
	position = table->perm[i];
	result = sift_up(table,position,j+lower);
	if (!result) return(0);
	size = table->keys - table->isolated;
	if (size > limit) break;
    }

    /* Sift the DD just built. */
#ifdef DD_STATS
    (void) fprintf(table->out,"\n");
#endif
    result = cuddSifting(table,lower,upper);
    if (!result) return(0);

    /* Copy order and size to table. */
    for (j = 0; j < numvars; j++) {
	STOREDD(num,j) = table->invperm[lower+j];
    }
    STOREDD(num,numvars) = table->keys - table->isolated; /* size of new DD */
    return(1);

} /* end of build_dd */


/**Function********************************************************************

  Synopsis    [Finds the largest DD in the population.]

  Description [Finds the largest DD in the population. If an order is
  repeated, it avoids choosing the copy that is in the computed table
  (it has repeat[i] > 1).]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
largest(void)
{
    int i;	/* loop var */
    int big;	/* temporary holder to return result */

    big = 0;
    while (repeat[big] > 1) big++;
    for (i = big + 1; i < popsize; i++) {
	if (STOREDD(i,numvars) >= STOREDD(big,numvars) && repeat[i] <= 1) {
	    big = i;
	}
    }
    return(big);

} /* end of largest */


/**Function********************************************************************

  Synopsis    [Generates a random number between 0 and the integer a.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
rand_int(
  int  a)
{
    return(Cudd_Random() % (a+1));

} /* end of rand_int */


/**Function********************************************************************

  Synopsis    [Hash function for the computed table.]

  Description [Hash function for the computed table. Returns the bucket
  number.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
array_hash(
  char * array,
  int  modulus)
{
    int val = 0;
    int i;
    int *intarray;

    intarray = (int *) array;

    for (i = 0; i < numvars; i++) {
	val = val * 997 + intarray[i];
    }

    return ((val < 0) ? -val : val) % modulus;

} /* end of array_hash */


/**Function********************************************************************

  Synopsis    [Comparison function for the computed table.]

  Description [Comparison function for the computed table. Returns 0 if
  the two arrays are equal; 1 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
array_compare(
  const char * array1,
  const char * array2)
{
    int i;
    int *intarray1, *intarray2;

    intarray1 = (int *) array1;
    intarray2 = (int *) array2;

    for (i = 0; i < numvars; i++) {
	if (intarray1[i] != intarray2[i]) return(1);
    }
    return(0);

} /* end of array_compare */


/**Function********************************************************************

  Synopsis    [Returns the index of the fittest individual.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
find_best(void)
{
    int i,small;

    small = 0;
    for (i = 1; i < popsize; i++) {
	if (STOREDD(i,numvars) < STOREDD(small,numvars)) {
	    small = i;
	}
    }
    return(small);

} /* end of find_best */


/**Function********************************************************************

  Synopsis    [Returns the average fitness of the population.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
#ifdef DD_STATS
static double
find_average_fitness(void)
{
    int i;
    int total_fitness = 0;
    double average_fitness;

    for (i = 0; i < popsize; i++) {
	total_fitness += STOREDD(i,numvars);
    }
    average_fitness = (double) total_fitness / (double) popsize;
    return(average_fitness);

} /* end of find_average_fitness */
#endif


/**Function********************************************************************

  Synopsis [Performs the crossover between two parents.]

  Description [Performs the crossover between two randomly chosen
  parents, and creates two children, x1 and x2. Uses the Partially
  Matched Crossover operator.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
PMX(
  int  maxvar)
{
    int 	cut1,cut2;	/* the two cut positions (random) */
    int 	mom,dad;	/* the two randomly chosen parents */
    int		*inv1;		/* inverse permutations for repair algo */
    int		*inv2;
    int 	i;		/* loop vars */
    int		u,v;		/* aux vars */

    inv1 = ALLOC(int,maxvar);
    if (inv1 == NULL) {
	return(0);
    }
    inv2 = ALLOC(int,maxvar);
    if (inv2 == NULL) {
	FREE(inv1);
	return(0);
    }

    /* Choose two orders from the population using roulette wheel. */
    if (!roulette(&mom,&dad)) {
	FREE(inv1);
	FREE(inv2);
	return(0);
    }

    /* Choose two random cut positions. A cut in position i means that
    ** the cut immediately precedes position i.  If cut1 < cut2, we
    ** exchange the middle of the two orderings; otherwise, we
    ** exchange the beginnings and the ends.
    */
    cut1 = rand_int(numvars-1);
    do {
	cut2 = rand_int(numvars-1);
    } while (cut1 == cut2);

#if 0
    /* Print out the parents. */
    (void) fprintf(table->out,
		   "Crossover of %d (mom) and %d (dad) between %d and %d\n",
		   mom,dad,cut1,cut2);
    for (i = 0; i < numvars; i++) {
	if (i == cut1 || i == cut2) (void) fprintf(table->out,"|");
	(void) fprintf(table->out,"%2d ",STOREDD(mom,i));
    }
    (void) fprintf(table->out,"\n");
    for (i = 0; i < numvars; i++) {
	if (i == cut1 || i == cut2) (void) fprintf(table->out,"|");
	(void) fprintf(table->out,"%2d ",STOREDD(dad,i));
    }
    (void) fprintf(table->out,"\n");
#endif

    /* Initialize the inverse permutations: -1 means yet undetermined. */
    for (i = 0; i < maxvar; i++) {
	inv1[i] = -1;
	inv2[i] = -1;
    }

    /* Copy the portions whithin the cuts. */
    for (i = cut1; i != cut2; i = (i == numvars-1) ? 0 : i+1) {
	STOREDD(popsize,i) = STOREDD(dad,i);
	inv1[STOREDD(popsize,i)] = i;
	STOREDD(popsize+1,i) = STOREDD(mom,i);
	inv2[STOREDD(popsize+1,i)] = i;
    }

    /* Now apply the repair algorithm outside the cuts. */
    for (i = cut2; i != cut1; i = (i == numvars-1 ) ? 0 : i+1) {
	v = i;
	do {
	    u = STOREDD(mom,v);
	    v = inv1[u];
	} while (v != -1);
	STOREDD(popsize,i) = u;
	inv1[u] = i;
	v = i;
	do {
	    u = STOREDD(dad,v);
	    v = inv2[u];
	} while (v != -1);
	STOREDD(popsize+1,i) = u;
	inv2[u] = i;
    }

#if 0
    /* Print the results of crossover. */
    for (i = 0; i < numvars; i++) {
	if (i == cut1 || i == cut2) (void) fprintf(table->out,"|");
	(void) fprintf(table->out,"%2d ",STOREDD(popsize,i));
    }
    (void) fprintf(table->out,"\n");
    for (i = 0; i < numvars; i++) {
	if (i == cut1 || i == cut2) (void) fprintf(table->out,"|");
	(void) fprintf(table->out,"%2d ",STOREDD(popsize+1,i));
    }
    (void) fprintf(table->out,"\n");
#endif

    FREE(inv1);
    FREE(inv2);
    return(1);

} /* end of PMX */


/**Function********************************************************************

  Synopsis    [Selects two parents with the roulette wheel method.]

  Description [Selects two distinct parents with the roulette wheel method.]

  SideEffects [The indices of the selected parents are returned as side
  effects.]

  SeeAlso     []

******************************************************************************/
static int
roulette(
  int * p1,
  int * p2)
{
    double *wheel;
    double spin;
    int i;

    wheel = ALLOC(double,popsize);
    if (wheel == NULL) {
	return(0);
    }

    /* The fitness of an individual is the reciprocal of its size. */
    wheel[0] = 1.0 / (double) STOREDD(0,numvars);

    for (i = 1; i < popsize; i++) {
	wheel[i] = wheel[i-1] + 1.0 / (double) STOREDD(i,numvars);
    }

    /* Get a random number between 0 and wheel[popsize-1] (that is,
    ** the sum of all fitness values. 2147483561 is the largest number
    ** returned by Cudd_Random.
    */
    spin = wheel[numvars-1] * (double) Cudd_Random() / 2147483561.0;

    /* Find the lucky element by scanning the wheel. */
    for (i = 0; i < popsize; i++) {
	if (spin <= wheel[i]) break;
    }
    *p1 = i;

    /* Repeat the process for the second parent, making sure it is
    ** distinct from the first.
    */
    do {
	spin = wheel[popsize-1] * (double) Cudd_Random() / 2147483561.0;
	for (i = 0; i < popsize; i++) {
	    if (spin <= wheel[i]) break;
	}
    } while (i == *p1);
    *p2 = i;

    FREE(wheel);
    return(1);

} /* end of roulette */

