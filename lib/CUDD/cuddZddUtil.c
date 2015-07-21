/**CFile***********************************************************************

  FileName    [cuddZddUtil.c]

  PackageName [cudd]

  Synopsis    [Utility functions for ZDDs.]

  Description [External procedures included in this module:
		    <ul>
		    <li> Cudd_zddPrintMinterm()
		    <li> Cudd_zddPrintCover()
		    <li> Cudd_zddPrintDebug()
		    <li> Cudd_zddFirstPath()
		    <li> Cudd_zddNextPath()
		    <li> Cudd_zddCoverPathToString()
                    <li> Cudd_zddSupport()
		    <li> Cudd_zddDumpDot()
		    </ul>
	       Internal procedures included in this module:
		    <ul>
		    <li> cuddZddP()
		    </ul>
	       Static procedures included in this module:
		    <ul>
		    <li> zp2()
		    <li> zdd_print_minterm_aux()
		    <li> zddPrintCoverAux()
                    <li> zddSupportStep()
                    <li> zddClearFlag()
		    </ul>
	      ]

  SeeAlso     []

  Author      [Hyong-Kyoon Shin, In-Ho Moon, Fabio Somenzi]

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
static char rcsid[] DD_UNUSED = "$Id: cuddZddUtil.c,v 1.29 2012/02/05 01:07:19 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int zp2 (DdManager *zdd, DdNode *f, st_table *t);
static void zdd_print_minterm_aux (DdManager *zdd, DdNode *node, int level, int *list);
static void zddPrintCoverAux (DdManager *zdd, DdNode *node, int level, int *list);
static void zddSupportStep(DdNode * f, int * support);
static void zddClearFlag(DdNode * f);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Prints a disjoint sum of product form for a ZDD.]

  Description [Prints a disjoint sum of product form for a ZDD. Returns 1
  if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddPrintDebug Cudd_zddPrintCover]

******************************************************************************/
int
Cudd_zddPrintMinterm(
  DdManager * zdd,
  DdNode * node)
{
    int		i, size;
    int		*list;

    size = (int)zdd->sizeZ;
    list = ALLOC(int, size);
    if (list == NULL) {
	zdd->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (i = 0; i < size; i++) list[i] = 3; /* bogus value should disappear */
    zdd_print_minterm_aux(zdd, node, 0, list);
    FREE(list);
    return(1);

} /* end of Cudd_zddPrintMinterm */


/**Function********************************************************************

  Synopsis    [Prints a sum of products from a ZDD representing a cover.]

  Description [Prints a sum of products from a ZDD representing a cover.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddPrintMinterm]

******************************************************************************/
int
Cudd_zddPrintCover(
  DdManager * zdd,
  DdNode * node)
{
    int		i, size;
    int		*list;

    size = (int)zdd->sizeZ;
    if (size % 2 != 0) return(0); /* number of variables should be even */
    list = ALLOC(int, size);
    if (list == NULL) {
	zdd->errorCode = CUDD_MEMORY_OUT;
	return(0);
    }
    for (i = 0; i < size; i++) list[i] = 3; /* bogus value should disappear */
    zddPrintCoverAux(zdd, node, 0, list);
    FREE(list);
    return(1);

} /* end of Cudd_zddPrintCover */


/**Function********************************************************************

  Synopsis [Prints to the standard output a ZDD and its statistics.]

  Description [Prints to the standard output a DD and its statistics.
  The statistics include the number of nodes and the number of minterms.
  (The number of minterms is also the number of combinations in the set.)
  The statistics are printed if pr &gt; 0.  Specifically:
  <ul>
  <li> pr = 0 : prints nothing
  <li> pr = 1 : prints counts of nodes and minterms
  <li> pr = 2 : prints counts + disjoint sum of products
  <li> pr = 3 : prints counts + list of nodes
  <li> pr &gt; 3 : prints counts + disjoint sum of products + list of nodes
  </ul>
  Returns 1 if successful; 0 otherwise.
  ]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_zddPrintDebug(
  DdManager * zdd,
  DdNode * f,
  int  n,
  int  pr)
{
    DdNode	*empty = DD_ZERO(zdd);
    int		nodes;
    double	minterms;
    int		retval = 1;

    if (f == empty && pr > 0) {
	(void) fprintf(zdd->out,": is the empty ZDD\n");
	(void) fflush(zdd->out);
	return(1);
    }

    if (pr > 0) {
	nodes = Cudd_zddDagSize(f);
	if (nodes == CUDD_OUT_OF_MEM) retval = 0;
	minterms = Cudd_zddCountMinterm(zdd, f, n);
	if (minterms == (double)CUDD_OUT_OF_MEM) retval = 0;
	(void) fprintf(zdd->out,": %d nodes %g minterms\n",
		       nodes, minterms);
	if (pr > 2)
	    if (!cuddZddP(zdd, f)) retval = 0;
	if (pr == 2 || pr > 3) {
	    if (!Cudd_zddPrintMinterm(zdd, f)) retval = 0;
	    (void) fprintf(zdd->out,"\n");
	}
	(void) fflush(zdd->out);
    }
    return(retval);

} /* end of Cudd_zddPrintDebug */



/**Function********************************************************************

  Synopsis    [Finds the first path of a ZDD.]

  Description [Defines an iterator on the paths of a ZDD
  and finds its first path. Returns a generator that contains the
  information necessary to continue the enumeration if successful; NULL
  otherwise.<p>
  A path is represented as an array of literals, which are integers in
  {0, 1, 2}; 0 represents an else arc out of a node, 1 represents a then arc
  out of a node, and 2 stands for the absence of a node.
  The size of the array equals the number of variables in the manager at
  the time Cudd_zddFirstCube is called.<p>
  The paths that end in the empty terminal are not enumerated.]

  SideEffects [The first path is returned as a side effect.]

  SeeAlso     [Cudd_zddForeachPath Cudd_zddNextPath Cudd_GenFree
  Cudd_IsGenEmpty]

******************************************************************************/
DdGen *
Cudd_zddFirstPath(
  DdManager * zdd,
  DdNode * f,
  int ** path)
{
    DdGen *gen;
    DdNode *top, *next, *prev;
    int i;
    int nvars;

    /* Sanity Check. */
    if (zdd == NULL || f == NULL) return(NULL);

    /* Allocate generator an initialize it. */
    gen = ALLOC(DdGen,1);
    if (gen == NULL) {
	zdd->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    gen->manager = zdd;
    gen->type = CUDD_GEN_ZDD_PATHS;
    gen->status = CUDD_GEN_EMPTY;
    gen->gen.cubes.cube = NULL;
    gen->gen.cubes.value = DD_ZERO_VAL;
    gen->stack.sp = 0;
    gen->stack.stack = NULL;
    gen->node = NULL;

    nvars = zdd->sizeZ;
    gen->gen.cubes.cube = ALLOC(int,nvars);
    if (gen->gen.cubes.cube == NULL) {
	zdd->errorCode = CUDD_MEMORY_OUT;
	FREE(gen);
	return(NULL);
    }
    for (i = 0; i < nvars; i++) gen->gen.cubes.cube[i] = 2;

    /* The maximum stack depth is one plus the number of variables.
    ** because a path may have nodes at all levels, including the
    ** constant level.
    */
    gen->stack.stack = ALLOC(DdNodePtr, nvars+1);
    if (gen->stack.stack == NULL) {
	zdd->errorCode = CUDD_MEMORY_OUT;
	FREE(gen->gen.cubes.cube);
	FREE(gen);
	return(NULL);
    }
    for (i = 0; i <= nvars; i++) gen->stack.stack[i] = NULL;

    /* Find the first path of the ZDD. */
    gen->stack.stack[gen->stack.sp] = f; gen->stack.sp++;

    while (1) {
	top = gen->stack.stack[gen->stack.sp-1];
	if (!cuddIsConstant(Cudd_Regular(top))) {
	    /* Take the else branch first. */
	    gen->gen.cubes.cube[Cudd_Regular(top)->index] = 0;
	    next = cuddE(Cudd_Regular(top));
	    gen->stack.stack[gen->stack.sp] = Cudd_Not(next); gen->stack.sp++;
	} else if (Cudd_Regular(top) == DD_ZERO(zdd)) {
	    /* Backtrack. */
	    while (1) {
		if (gen->stack.sp == 1) {
		    /* The current node has no predecessor. */
		    gen->status = CUDD_GEN_EMPTY;
		    gen->stack.sp--;
		    goto done;
		}
		prev = Cudd_Regular(gen->stack.stack[gen->stack.sp-2]);
		next = cuddT(prev);
		if (next != top) { /* follow the then branch next */
		    gen->gen.cubes.cube[prev->index] = 1;
		    gen->stack.stack[gen->stack.sp-1] = next;
		    break;
		}
		/* Pop the stack and try again. */
		gen->gen.cubes.cube[prev->index] = 2;
		gen->stack.sp--;
		top = gen->stack.stack[gen->stack.sp-1];
	    }
	} else {
	    gen->status = CUDD_GEN_NONEMPTY;
	    gen->gen.cubes.value = cuddV(Cudd_Regular(top));
	    goto done;
	}
    }

done:
    *path = gen->gen.cubes.cube;
    return(gen);

} /* end of Cudd_zddFirstPath */


/**Function********************************************************************

  Synopsis    [Generates the next path of a ZDD.]

  Description [Generates the next path of a ZDD onset,
  using generator gen. Returns 0 if the enumeration is completed; 1
  otherwise.]

  SideEffects [The path is returned as a side effect. The
  generator is modified.]

  SeeAlso     [Cudd_zddForeachPath Cudd_zddFirstPath Cudd_GenFree
  Cudd_IsGenEmpty]

******************************************************************************/
int
Cudd_zddNextPath(
  DdGen * gen,
  int ** path)
{
    DdNode *top, *next, *prev;
    DdManager *zdd = gen->manager;

    /* Backtrack from previously reached terminal node. */
    while (1) {
	if (gen->stack.sp == 1) {
	    /* The current node has no predecessor. */
	    gen->status = CUDD_GEN_EMPTY;
	    gen->stack.sp--;
	    goto done;
	}
	top = gen->stack.stack[gen->stack.sp-1];
	prev = Cudd_Regular(gen->stack.stack[gen->stack.sp-2]);
	next = cuddT(prev);
	if (next != top) { /* follow the then branch next */
	    gen->gen.cubes.cube[prev->index] = 1;
	    gen->stack.stack[gen->stack.sp-1] = next;
	    break;
	}
	/* Pop the stack and try again. */
	gen->gen.cubes.cube[prev->index] = 2;
	gen->stack.sp--;
    }

    while (1) {
	top = gen->stack.stack[gen->stack.sp-1];
	if (!cuddIsConstant(Cudd_Regular(top))) {
	    /* Take the else branch first. */
	    gen->gen.cubes.cube[Cudd_Regular(top)->index] = 0;
	    next = cuddE(Cudd_Regular(top));
	    gen->stack.stack[gen->stack.sp] = Cudd_Not(next); gen->stack.sp++;
	} else if (Cudd_Regular(top) == DD_ZERO(zdd)) {
	    /* Backtrack. */
	    while (1) {
		if (gen->stack.sp == 1) {
		    /* The current node has no predecessor. */
		    gen->status = CUDD_GEN_EMPTY;
		    gen->stack.sp--;
		    goto done;
		}
		prev = Cudd_Regular(gen->stack.stack[gen->stack.sp-2]);
		next = cuddT(prev);
		if (next != top) { /* follow the then branch next */
		    gen->gen.cubes.cube[prev->index] = 1;
		    gen->stack.stack[gen->stack.sp-1] = next;
		    break;
		}
		/* Pop the stack and try again. */
		gen->gen.cubes.cube[prev->index] = 2;
		gen->stack.sp--;
		top = gen->stack.stack[gen->stack.sp-1];
	    }
	} else {
	    gen->status = CUDD_GEN_NONEMPTY;
	    gen->gen.cubes.value = cuddV(Cudd_Regular(top));
	    goto done;
	}
    }

done:
    if (gen->status == CUDD_GEN_EMPTY) return(0);
    *path = gen->gen.cubes.cube;
    return(1);

} /* end of Cudd_zddNextPath */


/**Function********************************************************************

  Synopsis    [Converts a path of a ZDD representing a cover to a string.]

  Description [Converts a path of a ZDD representing a cover to a
  string.  The string represents an implicant of the cover.  The path
  is typically produced by Cudd_zddForeachPath.  Returns a pointer to
  the string if successful; NULL otherwise.  If the str input is NULL,
  it allocates a new string.  The string passed to this function must
  have enough room for all variables and for the terminator.]

  SideEffects [None]

  SeeAlso     [Cudd_zddForeachPath]

******************************************************************************/
char *
Cudd_zddCoverPathToString(
  DdManager *zdd		/* DD manager */,
  int *path			/* path of ZDD representing a cover */,
  char *str			/* pointer to string to use if != NULL */
  )
{
    int nvars = zdd->sizeZ;
    int i;
    char *res;

    if (nvars & 1) return(NULL);
    nvars >>= 1;
    if (str == NULL) {
	res = ALLOC(char, nvars+1);
	if (res == NULL) return(NULL);
    } else {
	res = str;
    }
    for (i = 0; i < nvars; i++) {
	int v = (path[2*i] << 2) | path[2*i+1];
	switch (v) {
	case 0:
	case 2:
	case 8:
	case 10:
	    res[i] = '-';
	    break;
	case 1:
	case 9:
	    res[i] = '0';
	    break;
	case 4:
	case 6:
	    res[i] = '1';
	    break;
	default:
	    res[i] = '?';
	}
    }
    res[nvars] = 0;

    return(res);

} /* end of Cudd_zddCoverPathToString */


/**Function********************************************************************

  Synopsis    [Finds the variables on which a ZDD depends.]

  Description [Finds the variables on which a ZDD depends.
  Returns a BDD consisting of the product of the variables if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Support]

******************************************************************************/
DdNode *
Cudd_zddSupport(
  DdManager * dd /* manager */,
  DdNode * f /* ZDD whose support is sought */)
{
    int *support;
    DdNode *res, *tmp, *var;
    int i,j;
    int size;

    /* Allocate and initialize support array for ddSupportStep. */
    size = ddMax(dd->size, dd->sizeZ);
    support = ALLOC(int,size);
    if (support == NULL) {
        dd->errorCode = CUDD_MEMORY_OUT;
        return(NULL);
    }
    for (i = 0; i < size; i++) {
        support[i] = 0;
    }

    /* Compute support and clean up markers. */
    zddSupportStep(Cudd_Regular(f),support);
    zddClearFlag(Cudd_Regular(f));

    /* Transform support from array to cube. */
    do {
        dd->reordered = 0;
        res = DD_ONE(dd);
        cuddRef(res);
        for (j = size - 1; j >= 0; j--) { /* for each level bottom-up */
            i = (j >= dd->size) ? j : dd->invperm[j];
            if (support[i] == 1) {
                /* The following call to cuddUniqueInter is guaranteed
                ** not to trigger reordering because the node we look up
                ** already exists. */
                var = cuddUniqueInter(dd,i,dd->one,Cudd_Not(dd->one));
                cuddRef(var);
                tmp = cuddBddAndRecur(dd,res,var);
                if (tmp == NULL) {
                    Cudd_RecursiveDeref(dd,res);
                    Cudd_RecursiveDeref(dd,var);
                    res = NULL;
                    break;
                }
                cuddRef(tmp);
                Cudd_RecursiveDeref(dd,res);
                Cudd_RecursiveDeref(dd,var);
                res = tmp;
            }
        }
    } while (dd->reordered == 1);

    FREE(support);
    if (res != NULL) cuddDeref(res);
    return(res);

} /* end of Cudd_zddSupport */


/**Function********************************************************************

  Synopsis    [Writes a dot file representing the argument ZDDs.]

  Description [Writes a file representing the argument ZDDs in a format
  suitable for the graph drawing program dot.
  It returns 1 in case of success; 0 otherwise (e.g., out-of-memory,
  file system full).
  Cudd_zddDumpDot does not close the file: This is the caller
  responsibility. Cudd_zddDumpDot uses a minimal unique subset of the
  hexadecimal address of a node as name for it.
  If the argument inames is non-null, it is assumed to hold the pointers
  to the names of the inputs. Similarly for onames.
  Cudd_zddDumpDot uses the following convention to draw arcs:
    <ul>
    <li> solid line: THEN arcs;
    <li> dashed line: ELSE arcs.
    </ul>
  The dot options are chosen so that the drawing fits on a letter-size
  sheet.
  ]

  SideEffects [None]

  SeeAlso     [Cudd_DumpDot Cudd_zddPrintDebug]

******************************************************************************/
int
Cudd_zddDumpDot(
  DdManager * dd /* manager */,
  int  n /* number of output nodes to be dumped */,
  DdNode ** f /* array of output nodes to be dumped */,
  char ** inames /* array of input names (or NULL) */,
  char ** onames /* array of output names (or NULL) */,
  FILE * fp /* pointer to the dump file */)
{
    DdNode	*support = NULL;
    DdNode	*scan;
    int		*sorted = NULL;
    int		nvars = dd->sizeZ;
    st_table	*visited = NULL;
    st_generator *gen;
    int		retval;
    int		i, j;
    int		slots;
    DdNodePtr	*nodelist;
    long	refAddr, diff, mask;

    /* Build a bit array with the support of f. */
    sorted = ALLOC(int,nvars);
    if (sorted == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	goto failure;
    }
    for (i = 0; i < nvars; i++) sorted[i] = 0;

    /* Take the union of the supports of each output function. */
    for (i = 0; i < n; i++) {
	support = Cudd_zddSupport(dd,f[i]);
	if (support == NULL) goto failure;
	cuddRef(support);
	scan = support;
	while (!cuddIsConstant(scan)) {
	    sorted[scan->index] = 1;
	    scan = cuddT(scan);
	}
	Cudd_RecursiveDeref(dd,support);
    }
    support = NULL; /* so that we do not try to free it in case of failure */

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    /* Collect all the nodes of this DD in the symbol table. */
    for (i = 0; i < n; i++) {
	retval = cuddCollectNodes(f[i],visited);
	if (retval == 0) goto failure;
    }

    /* Find how many most significant hex digits are identical
    ** in the addresses of all the nodes. Build a mask based
    ** on this knowledge, so that digits that carry no information
    ** will not be printed. This is done in two steps.
    **  1. We scan the symbol table to find the bits that differ
    **     in at least 2 addresses.
    **  2. We choose one of the possible masks. There are 8 possible
    **     masks for 32-bit integer, and 16 possible masks for 64-bit
    **     integers.
    */

    /* Find the bits that are different. */
    refAddr = (long) f[0];
    diff = 0;
    gen = st_init_gen(visited);
    while (st_gen(gen, &scan, NULL)) {
	diff |= refAddr ^ (long) scan;
    }
    st_free_gen(gen);

    /* Choose the mask. */
    for (i = 0; (unsigned) i < 8 * sizeof(long); i += 4) {
	mask = (1 << i) - 1;
	if (diff <= mask) break;
    }

    /* Write the header and the global attributes. */
    retval = fprintf(fp,"digraph \"ZDD\" {\n");
    if (retval == EOF) return(0);
    retval = fprintf(fp,
	"size = \"7.5,10\"\ncenter = true;\nedge [dir = none];\n");
    if (retval == EOF) return(0);

    /* Write the input name subgraph by scanning the support array. */
    retval = fprintf(fp,"{ node [shape = plaintext];\n");
    if (retval == EOF) goto failure;
    retval = fprintf(fp,"  edge [style = invis];\n");
    if (retval == EOF) goto failure;
    /* We use a name ("CONST NODES") with an embedded blank, because
    ** it is unlikely to appear as an input name.
    */
    retval = fprintf(fp,"  \"CONST NODES\" [style = invis];\n");
    if (retval == EOF) goto failure;
    for (i = 0; i < nvars; i++) {
	if (sorted[dd->invpermZ[i]]) {
	    if (inames == NULL) {
		retval = fprintf(fp,"\" %d \" -> ", dd->invpermZ[i]);
	    } else {
		retval = fprintf(fp,"\" %s \" -> ", inames[dd->invpermZ[i]]);
	    }
	    if (retval == EOF) goto failure;
	}
    }
    retval = fprintf(fp,"\"CONST NODES\"; \n}\n");
    if (retval == EOF) goto failure;

    /* Write the output node subgraph. */
    retval = fprintf(fp,"{ rank = same; node [shape = box]; edge [style = invis];\n");
    if (retval == EOF) goto failure;
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp,"\"F%d\"", i);
	} else {
	    retval = fprintf(fp,"\"  %s  \"", onames[i]);
	}
	if (retval == EOF) goto failure;
	if (i == n - 1) {
	    retval = fprintf(fp,"; }\n");
	} else {
	    retval = fprintf(fp," -> ");
	}
	if (retval == EOF) goto failure;
    }

    /* Write rank info: All nodes with the same index have the same rank. */
    for (i = 0; i < nvars; i++) {
	if (sorted[dd->invpermZ[i]]) {
	    retval = fprintf(fp,"{ rank = same; ");
	    if (retval == EOF) goto failure;
	    if (inames == NULL) {
		retval = fprintf(fp,"\" %d \";\n", dd->invpermZ[i]);
	    } else {
		retval = fprintf(fp,"\" %s \";\n", inames[dd->invpermZ[i]]);
	    }
	    if (retval == EOF) goto failure;
	    nodelist = dd->subtableZ[i].nodelist;
	    slots = dd->subtableZ[i].slots;
	    for (j = 0; j < slots; j++) {
		scan = nodelist[j];
		while (scan != NULL) {
		    if (st_is_member(visited,(char *) scan)) {
			retval = fprintf(fp,"\"%p\";\n", (void *)
					 ((mask & (ptrint) scan) /
					  sizeof(DdNode)));
			if (retval == EOF) goto failure;
		    }
		    scan = scan->next;
		}
	    }
	    retval = fprintf(fp,"}\n");
	    if (retval == EOF) goto failure;
	}
    }

    /* All constants have the same rank. */
    retval = fprintf(fp,
	"{ rank = same; \"CONST NODES\";\n{ node [shape = box]; ");
    if (retval == EOF) goto failure;
    nodelist = dd->constants.nodelist;
    slots = dd->constants.slots;
    for (j = 0; j < slots; j++) {
	scan = nodelist[j];
	while (scan != NULL) {
	    if (st_is_member(visited,(char *) scan)) {
		retval = fprintf(fp,"\"%p\";\n", (void *)
				 ((mask & (ptrint) scan) / sizeof(DdNode)));
		if (retval == EOF) goto failure;
	    }
	    scan = scan->next;
	}
    }
    retval = fprintf(fp,"}\n}\n");
    if (retval == EOF) goto failure;

    /* Write edge info. */
    /* Edges from the output nodes. */
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp,"\"F%d\"", i);
	} else {
	    retval = fprintf(fp,"\"  %s  \"", onames[i]);
	}
	if (retval == EOF) goto failure;
	retval = fprintf(fp," -> \"%p\" [style = solid];\n",
			 (void *) ((mask & (ptrint) f[i]) /
					  sizeof(DdNode)));
	if (retval == EOF) goto failure;
    }

    /* Edges from internal nodes. */
    for (i = 0; i < nvars; i++) {
	if (sorted[dd->invpermZ[i]]) {
	    nodelist = dd->subtableZ[i].nodelist;
	    slots = dd->subtableZ[i].slots;
	    for (j = 0; j < slots; j++) {
		scan = nodelist[j];
		while (scan != NULL) {
		    if (st_is_member(visited,(char *) scan)) {
			retval = fprintf(fp,
			    "\"%p\" -> \"%p\";\n",
			    (void *) ((mask & (ptrint) scan) / sizeof(DdNode)),
			    (void *) ((mask & (ptrint) cuddT(scan)) /
				      sizeof(DdNode)));
			if (retval == EOF) goto failure;
			retval = fprintf(fp,
					 "\"%p\" -> \"%p\" [style = dashed];\n",
					 (void *) ((mask & (ptrint) scan)
						   / sizeof(DdNode)),
					 (void *) ((mask & (ptrint)
						    cuddE(scan)) /
						   sizeof(DdNode)));
			if (retval == EOF) goto failure;
		    }
		    scan = scan->next;
		}
	    }
	}
    }

    /* Write constant labels. */
    nodelist = dd->constants.nodelist;
    slots = dd->constants.slots;
    for (j = 0; j < slots; j++) {
	scan = nodelist[j];
	while (scan != NULL) {
	    if (st_is_member(visited,(char *) scan)) {
		retval = fprintf(fp,"\"%p\" [label = \"%g\"];\n",
				 (void *) ((mask & (ptrint) scan) /
					   sizeof(DdNode)),
				 cuddV(scan));
		if (retval == EOF) goto failure;
	    }
	    scan = scan->next;
	}
    }

    /* Write trailer and return. */
    retval = fprintf(fp,"}\n");
    if (retval == EOF) goto failure;

    st_free_table(visited);
    FREE(sorted);
    return(1);

failure:
    if (sorted != NULL) FREE(sorted);
    if (visited != NULL) st_free_table(visited);
    return(0);

} /* end of Cudd_zddDumpBlif */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Prints a ZDD to the standard output. One line per node is
  printed.]

  Description [Prints a ZDD to the standard output. One line per node is
  printed. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddPrintDebug]

******************************************************************************/
int
cuddZddP(
  DdManager * zdd,
  DdNode * f)
{
    int retval;
    st_table *table = st_init_table(st_ptrcmp, st_ptrhash);

    if (table == NULL) return(0);

    retval = zp2(zdd, f, table);
    st_free_table(table);
    (void) fputc('\n', zdd->out);
    return(retval);

} /* end of cuddZddP */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis [Performs the recursive step of cuddZddP.]

  Description [Performs the recursive step of cuddZddP. Returns 1 in
  case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
zp2(
  DdManager * zdd,
  DdNode * f,
  st_table * t)
{
    DdNode	*n;
    int		T, E;
    DdNode	*base = DD_ONE(zdd);

    if (f == NULL)
	return(0);

    if (Cudd_IsConstant(f)) {
	(void)fprintf(zdd->out, "ID = %d\n", (f == base));
	return(1);
    }
    if (st_is_member(t, (char *)f) == 1)
	return(1);

    if (st_insert(t, (char *) f, NULL) == ST_OUT_OF_MEM)
	return(0);

#if SIZEOF_VOID_P == 8
    (void) fprintf(zdd->out, "ID = 0x%lx\tindex = %u\tr = %u\t",
	(ptruint)f / (ptruint) sizeof(DdNode), f->index, f->ref);
#else
    (void) fprintf(zdd->out, "ID = 0x%x\tindex = %hu\tr = %hu\t",
	(ptruint)f / (ptruint) sizeof(DdNode), f->index, f->ref);
#endif

    n = cuddT(f);
    if (Cudd_IsConstant(n)) {
	(void) fprintf(zdd->out, "T = %d\t\t", (n == base));
	T = 1;
    } else {
#if SIZEOF_VOID_P == 8
	(void) fprintf(zdd->out, "T = 0x%lx\t", (ptruint) n /
		       (ptruint) sizeof(DdNode));
#else
	(void) fprintf(zdd->out, "T = 0x%x\t", (ptruint) n /
		       (ptruint) sizeof(DdNode));
#endif
	T = 0;
    }

    n = cuddE(f);
    if (Cudd_IsConstant(n)) {
	(void) fprintf(zdd->out, "E = %d\n", (n == base));
	E = 1;
    } else {
#if SIZEOF_VOID_P == 8
	(void) fprintf(zdd->out, "E = 0x%lx\n", (ptruint) n /
		      (ptruint) sizeof(DdNode));
#else
	(void) fprintf(zdd->out, "E = 0x%x\n", (ptruint) n /
		       (ptruint) sizeof(DdNode));
#endif
	E = 0;
    }

    if (E == 0)
	if (zp2(zdd, cuddE(f), t) == 0) return(0);
    if (T == 0)
	if (zp2(zdd, cuddT(f), t) == 0) return(0);
    return(1);

} /* end of zp2 */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddPrintMinterm.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
zdd_print_minterm_aux(
  DdManager * zdd /* manager */,
  DdNode * node /* current node */,
  int  level /* depth in the recursion */,
  int * list /* current recursion path */)
{
    DdNode	*Nv, *Nnv;
    int		i, v;
    DdNode	*base = DD_ONE(zdd);

    if (Cudd_IsConstant(node)) {
	if (node == base) {
	    /* Check for missing variable. */
	    if (level != zdd->sizeZ) {
		list[zdd->invpermZ[level]] = 0;
		zdd_print_minterm_aux(zdd, node, level + 1, list);
		return;
	    }
	    /* Terminal case: Print one cube based on the current recursion
	    ** path.
	    */
	    for (i = 0; i < zdd->sizeZ; i++) {
		v = list[i];
		if (v == 0)
		    (void) fprintf(zdd->out,"0");
		else if (v == 1)
		    (void) fprintf(zdd->out,"1");
		else if (v == 3)
		    (void) fprintf(zdd->out,"@");	/* should never happen */
		else
		    (void) fprintf(zdd->out,"-");
	    }
	    (void) fprintf(zdd->out," 1\n");
	}
    } else {
	/* Check for missing variable. */
	if (level != cuddIZ(zdd,node->index)) {
	    list[zdd->invpermZ[level]] = 0;
	    zdd_print_minterm_aux(zdd, node, level + 1, list);
	    return;
	}

	Nnv = cuddE(node);
	Nv = cuddT(node);
	if (Nv == Nnv) {
	    list[node->index] = 2;
	    zdd_print_minterm_aux(zdd, Nnv, level + 1, list);
	    return;
	}

	list[node->index] = 1;
	zdd_print_minterm_aux(zdd, Nv, level + 1, list);
	list[node->index] = 0;
	zdd_print_minterm_aux(zdd, Nnv, level + 1, list);
    }
    return;

} /* end of zdd_print_minterm_aux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddPrintCover.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
zddPrintCoverAux(
  DdManager * zdd /* manager */,
  DdNode * node /* current node */,
  int  level /* depth in the recursion */,
  int * list /* current recursion path */)
{
    DdNode	*Nv, *Nnv;
    int		i, v;
    DdNode	*base = DD_ONE(zdd);

    if (Cudd_IsConstant(node)) {
	if (node == base) {
	    /* Check for missing variable. */
	    if (level != zdd->sizeZ) {
		list[zdd->invpermZ[level]] = 0;
		zddPrintCoverAux(zdd, node, level + 1, list);
		return;
	    }
	    /* Terminal case: Print one cube based on the current recursion
	    ** path.
	    */
	    for (i = 0; i < zdd->sizeZ; i += 2) {
		v = list[i] * 4 + list[i+1];
		if (v == 0)
		    (void) putc('-',zdd->out);
		else if (v == 4)
		    (void) putc('1',zdd->out);
		else if (v == 1)
		    (void) putc('0',zdd->out);
		else
		    (void) putc('@',zdd->out); /* should never happen */
	    }
	    (void) fprintf(zdd->out," 1\n");
	}
    } else {
	/* Check for missing variable. */
	if (level != cuddIZ(zdd,node->index)) {
	    list[zdd->invpermZ[level]] = 0;
	    zddPrintCoverAux(zdd, node, level + 1, list);
	    return;
	}

	Nnv = cuddE(node);
	Nv = cuddT(node);
	if (Nv == Nnv) {
	    list[node->index] = 2;
	    zddPrintCoverAux(zdd, Nnv, level + 1, list);
	    return;
	}

	list[node->index] = 1;
	zddPrintCoverAux(zdd, Nv, level + 1, list);
	list[node->index] = 0;
	zddPrintCoverAux(zdd, Nnv, level + 1, list);
    }
    return;

} /* end of zddPrintCoverAux */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddSupport.]

  Description [Performs the recursive step of Cudd_zddSupport. Performs a
  DFS from f. The support is accumulated in supp as a side effect. Uses
  the LSB of the then pointer as visited flag.]

  SideEffects [None]

  SeeAlso     [zddClearFlag]

******************************************************************************/
static void
zddSupportStep(
  DdNode * f,
  int * support)
{
    if (cuddIsConstant(f) || Cudd_IsComplement(f->next)) {
        return;
    }

    support[f->index] = 1;
    zddSupportStep(cuddT(f),support);
    zddSupportStep(Cudd_Regular(cuddE(f)),support);
    /* Mark as visited. */
    f->next = Cudd_Not(f->next);
    return;

} /* end of zddSupportStep */


/**Function********************************************************************

  Synopsis    [Performs a DFS from f, clearing the LSB of the next
  pointers.]

  Description []

  SideEffects [None]

  SeeAlso     [zddSupportStep]

******************************************************************************/
static void
zddClearFlag(
  DdNode * f)
{
    if (!Cudd_IsComplement(f->next)) {
        return;
    }
    /* Clear visited flag. */
    f->next = Cudd_Regular(f->next);
    if (cuddIsConstant(f)) {
        return;
    }
    zddClearFlag(cuddT(f));
    zddClearFlag(Cudd_Regular(cuddE(f)));
    return;

} /* end of zddClearFlag */

