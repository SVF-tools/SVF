/**CFile***********************************************************************

  FileName    [cuddExport.c]

  PackageName [cudd]

  Synopsis    [Export functions.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_DumpBlif()
		<li> Cudd_DumpBlifBody()
		<li> Cudd_DumpDot()
		<li> Cudd_DumpDaVinci()
		<li> Cudd_DumpDDcal()
		<li> Cudd_DumpFactoredForm()
		</ul>
	Internal procedures included in this module:
		<ul>
		</ul>
	Static procedures included in this module:
		<ul>
		<li> ddDoDumpBlif()
		<li> ddDoDumpDaVinci()
		<li> ddDoDumpDDcal()
		<li> ddDoDumpFactoredForm()
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
static char rcsid[] DD_UNUSED = "$Id: cuddExport.c,v 1.23 2012/02/05 01:07:18 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddDoDumpBlif (DdManager *dd, DdNode *f, FILE *fp, st_table *visited, char **names, int mv);
static int ddDoDumpDaVinci (DdManager *dd, DdNode *f, FILE *fp, st_table *visited, char **names, ptruint mask);
static int ddDoDumpDDcal (DdManager *dd, DdNode *f, FILE *fp, st_table *visited, char **names, ptruint mask);
static int ddDoDumpFactoredForm (DdManager *dd, DdNode *f, FILE *fp, char **names);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Writes a blif file representing the argument BDDs.]

  Description [Writes a blif file representing the argument BDDs as a
  network of multiplexers. One multiplexer is written for each BDD
  node. It returns 1 in case of success; 0 otherwise (e.g.,
  out-of-memory, file system full, or an ADD with constants different
  from 0 and 1).  Cudd_DumpBlif does not close the file: This is the
  caller responsibility. Cudd_DumpBlif uses a minimal unique subset of
  the hexadecimal address of a node as name for it.  If the argument
  inames is non-null, it is assumed to hold the pointers to the names
  of the inputs. Similarly for onames.]

  SideEffects [None]

  SeeAlso     [Cudd_DumpBlifBody Cudd_DumpDot Cudd_PrintDebug Cudd_DumpDDcal
  Cudd_DumpDaVinci Cudd_DumpFactoredForm]

******************************************************************************/
int
Cudd_DumpBlif(
  DdManager * dd /* manager */,
  int  n /* number of output nodes to be dumped */,
  DdNode ** f /* array of output nodes to be dumped */,
  char ** inames /* array of input names (or NULL) */,
  char ** onames /* array of output names (or NULL) */,
  char * mname /* model name (or NULL) */,
  FILE * fp /* pointer to the dump file */,
  int mv /* 0: blif, 1: blif-MV */)
{
    DdNode	*support = NULL;
    DdNode	*scan;
    int		*sorted = NULL;
    int		nvars = dd->size;
    int		retval;
    int		i;

    /* Build a bit array with the support of f. */
    sorted = ALLOC(int,nvars);
    if (sorted == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	goto failure;
    }
    for (i = 0; i < nvars; i++) sorted[i] = 0;

    /* Take the union of the supports of each output function. */
    support = Cudd_VectorSupport(dd,f,n);
    if (support == NULL) goto failure;
    cuddRef(support);
    scan = support;
    while (!cuddIsConstant(scan)) {
	sorted[scan->index] = 1;
	scan = cuddT(scan);
    }
    Cudd_RecursiveDeref(dd,support);
    support = NULL; /* so that we do not try to free it in case of failure */

    /* Write the header (.model .inputs .outputs). */
    if (mname == NULL) {
	retval = fprintf(fp,".model DD\n.inputs");
    } else {
	retval = fprintf(fp,".model %s\n.inputs",mname);
    }
    if (retval == EOF) {
	FREE(sorted);
	return(0);
    }

    /* Write the input list by scanning the support array. */
    for (i = 0; i < nvars; i++) {
	if (sorted[i]) {
	    if (inames == NULL) {
		retval = fprintf(fp," %d", i);
	    } else {
		retval = fprintf(fp," %s", inames[i]);
	    }
	    if (retval == EOF) goto failure;
	}
    }
    FREE(sorted);
    sorted = NULL;

    /* Write the .output line. */
    retval = fprintf(fp,"\n.outputs");
    if (retval == EOF) goto failure;
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp," f%d", i);
	} else {
	    retval = fprintf(fp," %s", onames[i]);
	}
	if (retval == EOF) goto failure;
    }
    retval = fprintf(fp,"\n");
    if (retval == EOF) goto failure;

    retval = Cudd_DumpBlifBody(dd, n, f, inames, onames, fp, mv);
    if (retval == 0) goto failure;

    /* Write trailer and return. */
    retval = fprintf(fp,".end\n");
    if (retval == EOF) goto failure;

    return(1);

failure:
    if (sorted != NULL) FREE(sorted);
    if (support != NULL) Cudd_RecursiveDeref(dd,support);
    return(0);

} /* end of Cudd_DumpBlif */


/**Function********************************************************************

  Synopsis    [Writes a blif body representing the argument BDDs.]

  Description [Writes a blif body representing the argument BDDs as a
  network of multiplexers.  No header (.model, .inputs, and .outputs) and
  footer (.end) are produced by this function.  One multiplexer is written
  for each BDD node. It returns 1 in case of success; 0 otherwise (e.g.,
  out-of-memory, file system full, or an ADD with constants different
  from 0 and 1).  Cudd_DumpBlifBody does not close the file: This is the
  caller responsibility. Cudd_DumpBlifBody uses a minimal unique subset of
  the hexadecimal address of a node as name for it.  If the argument
  inames is non-null, it is assumed to hold the pointers to the names
  of the inputs. Similarly for onames. This function prints out only
  .names part.]

  SideEffects [None]

  SeeAlso     [Cudd_DumpBlif Cudd_DumpDot Cudd_PrintDebug Cudd_DumpDDcal
  Cudd_DumpDaVinci Cudd_DumpFactoredForm]

******************************************************************************/
int
Cudd_DumpBlifBody(
  DdManager * dd /* manager */,
  int  n /* number of output nodes to be dumped */,
  DdNode ** f /* array of output nodes to be dumped */,
  char ** inames /* array of input names (or NULL) */,
  char ** onames /* array of output names (or NULL) */,
  FILE * fp /* pointer to the dump file */,
  int mv /* 0: blif, 1: blif-MV */)
{
    st_table	*visited = NULL;
    int		retval;
    int		i;

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    /* Call the function that really gets the job done. */
    for (i = 0; i < n; i++) {
	retval = ddDoDumpBlif(dd,Cudd_Regular(f[i]),fp,visited,inames,mv);
	if (retval == 0) goto failure;
    }

    /* To account for the possible complement on the root,
    ** we put either a buffer or an inverter at the output of
    ** the multiplexer representing the top node.
    */
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp,
#if SIZEOF_VOID_P == 8
		".names %lx f%d\n", (ptruint) f[i] / (ptruint) sizeof(DdNode), i);
#else
		".names %x f%d\n", (ptruint) f[i] / (ptruint) sizeof(DdNode), i);
#endif
	} else {
	    retval = fprintf(fp,
#if SIZEOF_VOID_P == 8
		".names %lx %s\n", (ptruint) f[i] / (ptruint) sizeof(DdNode), onames[i]);
#else
		".names %x %s\n", (ptruint) f[i] / (ptruint) sizeof(DdNode), onames[i]);
#endif
	}
	if (retval == EOF) goto failure;
	if (Cudd_IsComplement(f[i])) {
	    retval = fprintf(fp,"%s0 1\n", mv ? ".def 0\n" : "");
	} else {
	    retval = fprintf(fp,"%s1 1\n", mv ? ".def 0\n" : "");
	}
	if (retval == EOF) goto failure;
    }

    st_free_table(visited);
    return(1);

failure:
    if (visited != NULL) st_free_table(visited);
    return(0);

} /* end of Cudd_DumpBlifBody */


/**Function********************************************************************

  Synopsis    [Writes a dot file representing the argument DDs.]

  Description [Writes a file representing the argument DDs in a format
  suitable for the graph drawing program dot.
  It returns 1 in case of success; 0 otherwise (e.g., out-of-memory,
  file system full).
  Cudd_DumpDot does not close the file: This is the caller
  responsibility. Cudd_DumpDot uses a minimal unique subset of the
  hexadecimal address of a node as name for it.
  If the argument inames is non-null, it is assumed to hold the pointers
  to the names of the inputs. Similarly for onames.
  Cudd_DumpDot uses the following convention to draw arcs:
    <ul>
    <li> solid line: THEN arcs;
    <li> dotted line: complement arcs;
    <li> dashed line: regular ELSE arcs.
    </ul>
  The dot options are chosen so that the drawing fits on a letter-size
  sheet.
  ]

  SideEffects [None]

  SeeAlso     [Cudd_DumpBlif Cudd_PrintDebug Cudd_DumpDDcal
  Cudd_DumpDaVinci Cudd_DumpFactoredForm]

******************************************************************************/
int
Cudd_DumpDot(
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
    int		nvars = dd->size;
    st_table	*visited = NULL;
    st_generator *gen = NULL;
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
    support = Cudd_VectorSupport(dd,f,n);
    if (support == NULL) goto failure;
    cuddRef(support);
    scan = support;
    while (!cuddIsConstant(scan)) {
	sorted[scan->index] = 1;
	scan = cuddT(scan);
    }
    Cudd_RecursiveDeref(dd,support);
    support = NULL; /* so that we do not try to free it in case of failure */

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    /* Collect all the nodes of this DD in the symbol table. */
    for (i = 0; i < n; i++) {
	retval = cuddCollectNodes(Cudd_Regular(f[i]),visited);
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
    refAddr = (long) Cudd_Regular(f[0]);
    diff = 0;
    gen = st_init_gen(visited);
    if (gen == NULL) goto failure;
    while (st_gen(gen, &scan, NULL)) {
	diff |= refAddr ^ (long) scan;
    }
    st_free_gen(gen); gen = NULL;

    /* Choose the mask. */
    for (i = 0; (unsigned) i < 8 * sizeof(long); i += 4) {
	mask = (1 << i) - 1;
	if (diff <= mask) break;
    }

    /* Write the header and the global attributes. */
    retval = fprintf(fp,"digraph \"DD\" {\n");
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
	if (sorted[dd->invperm[i]]) {
	    if (inames == NULL || inames[dd->invperm[i]] == NULL) {
		retval = fprintf(fp,"\" %d \" -> ", dd->invperm[i]);
	    } else {
		retval = fprintf(fp,"\" %s \" -> ", inames[dd->invperm[i]]);
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
	if (sorted[dd->invperm[i]]) {
	    retval = fprintf(fp,"{ rank = same; ");
	    if (retval == EOF) goto failure;
	    if (inames == NULL || inames[dd->invperm[i]] == NULL) {
		retval = fprintf(fp,"\" %d \";\n", dd->invperm[i]);
	    } else {
		retval = fprintf(fp,"\" %s \";\n", inames[dd->invperm[i]]);
	    }
	    if (retval == EOF) goto failure;
	    nodelist = dd->subtables[i].nodelist;
	    slots = dd->subtables[i].slots;
	    for (j = 0; j < slots; j++) {
		scan = nodelist[j];
		while (scan != NULL) {
		    if (st_is_member(visited,(char *) scan)) {
			retval = fprintf(fp,"\"%p\";\n",
			    (void *) ((mask & (ptrint) scan) /
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
		retval = fprintf(fp,"\"%p\";\n",
		    (void *) ((mask & (ptrint) scan) / sizeof(DdNode)));
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
	/* Account for the possible complement on the root. */
	if (Cudd_IsComplement(f[i])) {
	    retval = fprintf(fp," -> \"%p\" [style = dotted];\n",
		(void *) ((mask & (ptrint) f[i]) / sizeof(DdNode)));
	} else {
	    retval = fprintf(fp," -> \"%p\" [style = solid];\n",
		(void *) ((mask & (ptrint) f[i]) / sizeof(DdNode)));
	}
	if (retval == EOF) goto failure;
    }

    /* Edges from internal nodes. */
    for (i = 0; i < nvars; i++) {
	if (sorted[dd->invperm[i]]) {
	    nodelist = dd->subtables[i].nodelist;
	    slots = dd->subtables[i].slots;
	    for (j = 0; j < slots; j++) {
		scan = nodelist[j];
		while (scan != NULL) {
		    if (st_is_member(visited,(char *) scan)) {
			retval = fprintf(fp,
			    "\"%p\" -> \"%p\";\n",
			    (void *) ((mask & (ptrint) scan) /
			    sizeof(DdNode)),
			    (void *) ((mask & (ptrint) cuddT(scan)) /
			    sizeof(DdNode)));
			if (retval == EOF) goto failure;
			if (Cudd_IsComplement(cuddE(scan))) {
			    retval = fprintf(fp,
				"\"%p\" -> \"%p\" [style = dotted];\n",
				(void *) ((mask & (ptrint) scan) /
				sizeof(DdNode)),
				(void *) ((mask & (ptrint) cuddE(scan)) /
				sizeof(DdNode)));
			} else {
			    retval = fprintf(fp,
				"\"%p\" -> \"%p\" [style = dashed];\n",
				(void *) ((mask & (ptrint) scan) /
				sizeof(DdNode)),
				(void *) ((mask & (ptrint) cuddE(scan)) /
				sizeof(DdNode)));
			}
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
		    (void *) ((mask & (ptrint) scan) / sizeof(DdNode)),
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
    if (support != NULL) Cudd_RecursiveDeref(dd,support);
    if (visited != NULL) st_free_table(visited);
    return(0);

} /* end of Cudd_DumpDot */


/**Function********************************************************************

  Synopsis    [Writes a daVinci file representing the argument BDDs.]

  Description [Writes a daVinci file representing the argument BDDs.
  It returns 1 in case of success; 0 otherwise (e.g., out-of-memory or
  file system full).  Cudd_DumpDaVinci does not close the file: This
  is the caller responsibility. Cudd_DumpDaVinci uses a minimal unique
  subset of the hexadecimal address of a node as name for it.  If the
  argument inames is non-null, it is assumed to hold the pointers to
  the names of the inputs. Similarly for onames.]

  SideEffects [None]

  SeeAlso     [Cudd_DumpDot Cudd_PrintDebug Cudd_DumpBlif Cudd_DumpDDcal
  Cudd_DumpFactoredForm]

******************************************************************************/
int
Cudd_DumpDaVinci(
  DdManager * dd /* manager */,
  int  n /* number of output nodes to be dumped */,
  DdNode ** f /* array of output nodes to be dumped */,
  char ** inames /* array of input names (or NULL) */,
  char ** onames /* array of output names (or NULL) */,
  FILE * fp /* pointer to the dump file */)
{
    DdNode	  *support = NULL;
    DdNode	  *scan;
    st_table	  *visited = NULL;
    int		  retval;
    int		  i;
    st_generator  *gen;
    ptruint       refAddr, diff, mask;

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    /* Collect all the nodes of this DD in the symbol table. */
    for (i = 0; i < n; i++) {
	retval = cuddCollectNodes(Cudd_Regular(f[i]),visited);
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
    refAddr = (ptruint) Cudd_Regular(f[0]);
    diff = 0;
    gen = st_init_gen(visited);
    while (st_gen(gen, &scan, NULL)) {
	diff |= refAddr ^ (ptruint) scan;
    }
    st_free_gen(gen);

    /* Choose the mask. */
    for (i = 0; (unsigned) i < 8 * sizeof(ptruint); i += 4) {
	mask = (1 << i) - 1;
	if (diff <= mask) break;
    }
    st_free_table(visited);

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    retval = fprintf(fp, "[");
    if (retval == EOF) goto failure;
    /* Call the function that really gets the job done. */
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp,
			     "l(\"f%d\",n(\"root\",[a(\"OBJECT\",\"f%d\")],",
			     i,i);
	} else {
	    retval = fprintf(fp,
			     "l(\"%s\",n(\"root\",[a(\"OBJECT\",\"%s\")],",
			     onames[i], onames[i]);
	}
	if (retval == EOF) goto failure;
	retval = fprintf(fp, "[e(\"edge\",[a(\"EDGECOLOR\",\"%s\"),a(\"_DIR\",\"none\")],",
			 Cudd_IsComplement(f[i]) ? "red" : "blue");
	if (retval == EOF) goto failure;
	retval = ddDoDumpDaVinci(dd,Cudd_Regular(f[i]),fp,visited,inames,mask);
	if (retval == 0) goto failure;
	retval = fprintf(fp, ")]))%s", i == n-1 ? "" : ",");
	if (retval == EOF) goto failure;
    }

    /* Write trailer and return. */
    retval = fprintf(fp, "]\n");
    if (retval == EOF) goto failure;

    st_free_table(visited);
    return(1);

failure:
    if (support != NULL) Cudd_RecursiveDeref(dd,support);
    if (visited != NULL) st_free_table(visited);
    return(0);

} /* end of Cudd_DumpDaVinci */


/**Function********************************************************************

  Synopsis    [Writes a DDcal file representing the argument BDDs.]

  Description [Writes a DDcal file representing the argument BDDs.
  It returns 1 in case of success; 0 otherwise (e.g., out-of-memory or
  file system full).  Cudd_DumpDDcal does not close the file: This
  is the caller responsibility. Cudd_DumpDDcal uses a minimal unique
  subset of the hexadecimal address of a node as name for it.  If the
  argument inames is non-null, it is assumed to hold the pointers to
  the names of the inputs. Similarly for onames.]

  SideEffects [None]

  SeeAlso     [Cudd_DumpDot Cudd_PrintDebug Cudd_DumpBlif Cudd_DumpDaVinci
  Cudd_DumpFactoredForm]

******************************************************************************/
int
Cudd_DumpDDcal(
  DdManager * dd /* manager */,
  int  n /* number of output nodes to be dumped */,
  DdNode ** f /* array of output nodes to be dumped */,
  char ** inames /* array of input names (or NULL) */,
  char ** onames /* array of output names (or NULL) */,
  FILE * fp /* pointer to the dump file */)
{
    DdNode	  *support = NULL;
    DdNode	  *scan;
    int		  *sorted = NULL;
    int		  nvars = dd->size;
    st_table	  *visited = NULL;
    int		  retval;
    int		  i;
    st_generator  *gen;
    ptruint       refAddr, diff, mask;

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    /* Collect all the nodes of this DD in the symbol table. */
    for (i = 0; i < n; i++) {
	retval = cuddCollectNodes(Cudd_Regular(f[i]),visited);
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
    refAddr = (ptruint) Cudd_Regular(f[0]);
    diff = 0;
    gen = st_init_gen(visited);
    while (st_gen(gen, &scan, NULL)) {
	diff |= refAddr ^ (ptruint) scan;
    }
    st_free_gen(gen);

    /* Choose the mask. */
    for (i = 0; (unsigned) i < 8 * sizeof(ptruint); i += 4) {
	mask = (1 << i) - 1;
	if (diff <= mask) break;
    }
    st_free_table(visited);

    /* Build a bit array with the support of f. */
    sorted = ALLOC(int,nvars);
    if (sorted == NULL) {
	dd->errorCode = CUDD_MEMORY_OUT;
	goto failure;
    }
    for (i = 0; i < nvars; i++) sorted[i] = 0;

    /* Take the union of the supports of each output function. */
    support = Cudd_VectorSupport(dd,f,n);
    if (support == NULL) goto failure;
    cuddRef(support);
    scan = support;
    while (!cuddIsConstant(scan)) {
	sorted[scan->index] = 1;
	scan = cuddT(scan);
    }
    Cudd_RecursiveDeref(dd,support);
    support = NULL; /* so that we do not try to free it in case of failure */
    for (i = 0; i < nvars; i++) {
	if (sorted[dd->invperm[i]]) {
	    if (inames == NULL || inames[dd->invperm[i]] == NULL) {
		retval = fprintf(fp,"v%d", dd->invperm[i]);
	    } else {
		retval = fprintf(fp,"%s", inames[dd->invperm[i]]);
	    }
	    if (retval == EOF) goto failure;
	}
	retval = fprintf(fp,"%s", i == nvars - 1 ? "\n" : " * ");
	if (retval == EOF) goto failure;
    }
    FREE(sorted);
    sorted = NULL;

    /* Initialize symbol table for visited nodes. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);
    if (visited == NULL) goto failure;

    /* Call the function that really gets the job done. */
    for (i = 0; i < n; i++) {
	retval = ddDoDumpDDcal(dd,Cudd_Regular(f[i]),fp,visited,inames,mask);
	if (retval == 0) goto failure;
	if (onames == NULL) {
	    retval = fprintf(fp, "f%d = ", i);
	} else {
	    retval = fprintf(fp, "%s = ", onames[i]);
	}
	if (retval == EOF) goto failure;
	retval = fprintf(fp, "n%p%s\n",
			 (void *) (((ptruint) f[i] & mask) /
			 (ptruint) sizeof(DdNode)),
			 Cudd_IsComplement(f[i]) ? "'" : "");
	if (retval == EOF) goto failure;
    }

    /* Write trailer and return. */
    retval = fprintf(fp, "[");
    if (retval == EOF) goto failure;
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp, "f%d", i);
	} else {
	    retval = fprintf(fp, "%s", onames[i]);
	}
	retval = fprintf(fp, "%s", i == n-1 ? "" : " ");
	if (retval == EOF) goto failure;
    }
    retval = fprintf(fp, "]\n");
    if (retval == EOF) goto failure;

    st_free_table(visited);
    return(1);

failure:
    if (sorted != NULL) FREE(sorted);
    if (support != NULL) Cudd_RecursiveDeref(dd,support);
    if (visited != NULL) st_free_table(visited);
    return(0);

} /* end of Cudd_DumpDDcal */


/**Function********************************************************************

  Synopsis    [Writes factored forms representing the argument BDDs.]

  Description [Writes factored forms representing the argument BDDs.
  The format of the factored form is the one used in the genlib files
  for technology mapping in sis.  It returns 1 in case of success; 0
  otherwise (e.g., file system full).  Cudd_DumpFactoredForm does not
  close the file: This is the caller responsibility. Caution must be
  exercised because a factored form may be exponentially larger than
  the argument BDD.  If the argument inames is non-null, it is assumed
  to hold the pointers to the names of the inputs. Similarly for
  onames.]

  SideEffects [None]

  SeeAlso     [Cudd_DumpDot Cudd_PrintDebug Cudd_DumpBlif Cudd_DumpDaVinci
  Cudd_DumpDDcal]

******************************************************************************/
int
Cudd_DumpFactoredForm(
  DdManager * dd /* manager */,
  int  n /* number of output nodes to be dumped */,
  DdNode ** f /* array of output nodes to be dumped */,
  char ** inames /* array of input names (or NULL) */,
  char ** onames /* array of output names (or NULL) */,
  FILE * fp /* pointer to the dump file */)
{
    int		retval;
    int		i;

    /* Call the function that really gets the job done. */
    for (i = 0; i < n; i++) {
	if (onames == NULL) {
	    retval = fprintf(fp, "f%d = ", i);
	} else {
	    retval = fprintf(fp, "%s = ", onames[i]);
	}
	if (retval == EOF) return(0);
	if (f[i] == DD_ONE(dd)) {
	    retval = fprintf(fp, "CONST1");
	    if (retval == EOF) return(0);
	} else if (f[i] == Cudd_Not(DD_ONE(dd)) || f[i] == DD_ZERO(dd)) {
	    retval = fprintf(fp, "CONST0");
	    if (retval == EOF) return(0);
	} else {
	    retval = fprintf(fp, "%s", Cudd_IsComplement(f[i]) ? "!(" : "");
	    if (retval == EOF) return(0);
	    retval = ddDoDumpFactoredForm(dd,Cudd_Regular(f[i]),fp,inames);
	    if (retval == 0) return(0);
	    retval = fprintf(fp, "%s", Cudd_IsComplement(f[i]) ? ")" : "");
	    if (retval == EOF) return(0);
	}
	retval = fprintf(fp, "%s", i == n-1 ? "" : "\n");
	if (retval == EOF) return(0);
    }

    return(1);

} /* end of Cudd_DumpFactoredForm */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_DumpBlif.]

  Description [Performs the recursive step of Cudd_DumpBlif. Traverses
  the BDD f and writes a multiplexer-network description to the file
  pointed by fp in blif format. f is assumed to be a regular pointer
  and ddDoDumpBlif guarantees this assumption in the recursive calls.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
ddDoDumpBlif(
  DdManager * dd,
  DdNode * f,
  FILE * fp,
  st_table * visited,
  char ** names,
  int mv)
{
    DdNode	*T, *E;
    int		retval;

#ifdef DD_DEBUG
    assert(!Cudd_IsComplement(f));
#endif

    /* If already visited, nothing to do. */
    if (st_is_member(visited, (char *) f) == 1)
	return(1);

    /* Check for abnormal condition that should never happen. */
    if (f == NULL)
	return(0);

    /* Mark node as visited. */
    if (st_insert(visited, (char *) f, NULL) == ST_OUT_OF_MEM)
	return(0);

    /* Check for special case: If constant node, generate constant 1. */
    if (f == DD_ONE(dd)) {
#if SIZEOF_VOID_P == 8
	retval = fprintf(fp, ".names %lx\n1\n",(ptruint) f / (ptruint) sizeof(DdNode));
#else
	retval = fprintf(fp, ".names %x\n1\n",(ptruint) f / (ptruint) sizeof(DdNode));
#endif
	if (retval == EOF) {
	    return(0);
	} else {
	    return(1);
	}
    }

    /* Check whether this is an ADD. We deal with 0-1 ADDs, but not
    ** with the general case.
    */
    if (f == DD_ZERO(dd)) {
#if SIZEOF_VOID_P == 8
	retval = fprintf(fp, ".names %lx\n%s",
			 (ptruint) f / (ptruint) sizeof(DdNode),
			 mv ? "0\n" : "");
#else
	retval = fprintf(fp, ".names %x\n%s",
			 (ptruint) f / (ptruint) sizeof(DdNode),
			 mv ? "0\n" : "");
#endif
	if (retval == EOF) {
	    return(0);
	} else {
	    return(1);
	}
    }
    if (cuddIsConstant(f))
	return(0);

    /* Recursive calls. */
    T = cuddT(f);
    retval = ddDoDumpBlif(dd,T,fp,visited,names,mv);
    if (retval != 1) return(retval);
    E = Cudd_Regular(cuddE(f));
    retval = ddDoDumpBlif(dd,E,fp,visited,names,mv);
    if (retval != 1) return(retval);

    /* Write multiplexer taking complement arc into account. */
    if (names != NULL) {
	retval = fprintf(fp,".names %s", names[f->index]);
    } else {
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
	retval = fprintf(fp,".names %u", f->index);
#else
	retval = fprintf(fp,".names %hu", f->index);
#endif
    }
    if (retval == EOF)
	return(0);

#if SIZEOF_VOID_P == 8
    if (mv) {
	if (Cudd_IsComplement(cuddE(f))) {
	    retval = fprintf(fp," %lx %lx %lx\n.def 0\n1 1 - 1\n0 - 0 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	} else {
	    retval = fprintf(fp," %lx %lx %lx\n.def 0\n1 1 - 1\n0 - 1 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	}
    } else {
	if (Cudd_IsComplement(cuddE(f))) {
	    retval = fprintf(fp," %lx %lx %lx\n11- 1\n0-0 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	} else {
	    retval = fprintf(fp," %lx %lx %lx\n11- 1\n0-1 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	}
    }
#else
    if (mv) {
	if (Cudd_IsComplement(cuddE(f))) {
	    retval = fprintf(fp," %x %x %x\n.def 0\n1 1 - 1\n0 - 0 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	} else {
	    retval = fprintf(fp," %x %x %x\n.def 0\n1 1 - 1\n0 - 1 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	}
    } else {
	if (Cudd_IsComplement(cuddE(f))) {
	    retval = fprintf(fp," %x %x %x\n11- 1\n0-0 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	} else {
	    retval = fprintf(fp," %x %x %x\n11- 1\n0-1 1\n",
		(ptruint) T / (ptruint) sizeof(DdNode),
		(ptruint) E / (ptruint) sizeof(DdNode),
		(ptruint) f / (ptruint) sizeof(DdNode));
	}
    }
#endif
    if (retval == EOF) {
	return(0);
    } else {
	return(1);
    }

} /* end of ddDoDumpBlif */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_DumpDaVinci.]

  Description [Performs the recursive step of Cudd_DumpDaVinci. Traverses
  the BDD f and writes a term expression to the file
  pointed by fp in daVinci format. f is assumed to be a regular pointer
  and ddDoDumpDaVinci guarantees this assumption in the recursive calls.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
ddDoDumpDaVinci(
  DdManager * dd,
  DdNode * f,
  FILE * fp,
  st_table * visited,
  char ** names,
  ptruint mask)
{
    DdNode  *T, *E;
    int	    retval;
    ptruint id;

#ifdef DD_DEBUG
    assert(!Cudd_IsComplement(f));
#endif

    id = ((ptruint) f & mask) / sizeof(DdNode);

    /* If already visited, insert a reference. */
    if (st_is_member(visited, (char *) f) == 1) {
	retval = fprintf(fp,"r(\"%p\")", (void *) id);
	if (retval == EOF) {
	    return(0);
	} else {
	    return(1);
	}
    }

    /* Check for abnormal condition that should never happen. */
    if (f == NULL)
	return(0);

    /* Mark node as visited. */
    if (st_insert(visited, (char *) f, NULL) == ST_OUT_OF_MEM)
	return(0);

    /* Check for special case: If constant node, generate constant 1. */
    if (Cudd_IsConstant(f)) {
	retval = fprintf(fp,
			 "l(\"%p\",n(\"constant\",[a(\"OBJECT\",\"%g\")],[]))",
			 (void *) id, cuddV(f));
	if (retval == EOF) {
	    return(0);
	} else {
	    return(1);
	}
    }

    /* Recursive calls. */
    if (names != NULL) {
	retval = fprintf(fp,
			 "l(\"%p\",n(\"internal\",[a(\"OBJECT\",\"%s\"),",
			 (void *) id, names[f->index]);
    } else {
	retval = fprintf(fp,
#if SIZEOF_VOID_P == 8
			 "l(\"%p\",n(\"internal\",[a(\"OBJECT\",\"%u\"),",
#else
			 "l(\"%p\",n(\"internal\",[a(\"OBJECT\",\"%hu\"),",
#endif
			 (void *) id, f->index);
    }
    retval = fprintf(fp, "a(\"_GO\",\"ellipse\")],[e(\"then\",[a(\"EDGECOLOR\",\"blue\"),a(\"_DIR\",\"none\")],");
    if (retval == EOF) return(0);
    T = cuddT(f);
    retval = ddDoDumpDaVinci(dd,T,fp,visited,names,mask);
    if (retval != 1) return(retval);
    retval = fprintf(fp, "),e(\"else\",[a(\"EDGECOLOR\",\"%s\"),a(\"_DIR\",\"none\")],",
		     Cudd_IsComplement(cuddE(f)) ? "red" : "green");
    if (retval == EOF) return(0);
    E = Cudd_Regular(cuddE(f));
    retval = ddDoDumpDaVinci(dd,E,fp,visited,names,mask);
    if (retval != 1) return(retval);

    retval = fprintf(fp,")]))");
    if (retval == EOF) {
	return(0);
    } else {
	return(1);
    }

} /* end of ddDoDumpDaVinci */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_DumpDDcal.]

  Description [Performs the recursive step of Cudd_DumpDDcal. Traverses
  the BDD f and writes a line for each node to the file
  pointed by fp in DDcal format. f is assumed to be a regular pointer
  and ddDoDumpDDcal guarantees this assumption in the recursive calls.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
ddDoDumpDDcal(
  DdManager * dd,
  DdNode * f,
  FILE * fp,
  st_table * visited,
  char ** names,
  ptruint mask)
{
    DdNode  *T, *E;
    int	    retval;
    ptruint id, idT, idE;

#ifdef DD_DEBUG
    assert(!Cudd_IsComplement(f));
#endif

    id = ((ptruint) f & mask) / sizeof(DdNode);

    /* If already visited, do nothing. */
    if (st_is_member(visited, (char *) f) == 1) {
	return(1);
    }

    /* Check for abnormal condition that should never happen. */
    if (f == NULL)
	return(0);

    /* Mark node as visited. */
    if (st_insert(visited, (char *) f, NULL) == ST_OUT_OF_MEM)
	return(0);

    /* Check for special case: If constant node, assign constant. */
    if (Cudd_IsConstant(f)) {
	if (f != DD_ONE(dd) && f != DD_ZERO(dd))
	    return(0);
	retval = fprintf(fp, "n%p = %g\n", (void *) id, cuddV(f));
	if (retval == EOF) {
	    return(0);
	} else {
	    return(1);
	}
    }

    /* Recursive calls. */
    T = cuddT(f);
    retval = ddDoDumpDDcal(dd,T,fp,visited,names,mask);
    if (retval != 1) return(retval);
    E = Cudd_Regular(cuddE(f));
    retval = ddDoDumpDDcal(dd,E,fp,visited,names,mask);
    if (retval != 1) return(retval);
    idT = ((ptruint) T & mask) / sizeof(DdNode);
    idE = ((ptruint) E & mask) / sizeof(DdNode);
    if (names != NULL) {
	retval = fprintf(fp, "n%p = %s * n%p + %s' * n%p%s\n",
			 (void *) id, names[f->index],
			 (void *) idT, names[f->index],
			 (void *) idE, Cudd_IsComplement(cuddE(f)) ? "'" : "");
    } else {
#if SIZEOF_VOID_P == 8
	retval = fprintf(fp, "n%p = v%u * n%p + v%u' * n%p%s\n",
#else
	retval = fprintf(fp, "n%p = v%hu * n%p + v%hu' * n%p%s\n",
#endif
			 (void *) id, f->index,
			 (void *) idT, f->index,
			 (void *) idE, Cudd_IsComplement(cuddE(f)) ? "'" : "");
    }
    if (retval == EOF) {
	return(0);
    } else {
	return(1);
    }

} /* end of ddDoDumpDDcal */


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_DumpFactoredForm.]

  Description [Performs the recursive step of
  Cudd_DumpFactoredForm. Traverses the BDD f and writes a factored
  form for each node to the file pointed by fp in terms of the
  factored forms of the children. Constants are propagated, and
  absorption is applied.  f is assumed to be a regular pointer and
  ddDoDumpFActoredForm guarantees this assumption in the recursive
  calls.]

  SideEffects [None]

  SeeAlso     [Cudd_DumpFactoredForm]

******************************************************************************/
static int
ddDoDumpFactoredForm(
  DdManager * dd,
  DdNode * f,
  FILE * fp,
  char ** names)
{
    DdNode	*T, *E;
    int		retval;

#ifdef DD_DEBUG
    assert(!Cudd_IsComplement(f));
    assert(!Cudd_IsConstant(f));
#endif

    /* Check for abnormal condition that should never happen. */
    if (f == NULL)
	return(0);

    /* Recursive calls. */
    T = cuddT(f);
    E = cuddE(f);
    if (T != DD_ZERO(dd)) {
	if (E != DD_ONE(dd)) {
	    if (names != NULL) {
		retval = fprintf(fp, "%s", names[f->index]);
	    } else {
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
		retval = fprintf(fp, "x%u", f->index);
#else
		retval = fprintf(fp, "x%hu", f->index);
#endif
	    }
	    if (retval == EOF) return(0);
	}
	if (T != DD_ONE(dd)) {
	    retval = fprintf(fp, "%s(", E != DD_ONE(dd) ? " * " : "");
	    if (retval == EOF) return(0);
	    retval = ddDoDumpFactoredForm(dd,T,fp,names);
	    if (retval != 1) return(retval);
	    retval = fprintf(fp, ")");
	    if (retval == EOF) return(0);
	}
	if (E == Cudd_Not(DD_ONE(dd)) || E == DD_ZERO(dd)) return(1);
	retval = fprintf(fp, " + ");
	if (retval == EOF) return(0);
    }
    E = Cudd_Regular(E);
    if (T != DD_ONE(dd)) {
	if (names != NULL) {
	    retval = fprintf(fp, "!%s", names[f->index]);
	} else {
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
	    retval = fprintf(fp, "!x%u", f->index);
#else
	    retval = fprintf(fp, "!x%hu", f->index);
#endif
	}
	if (retval == EOF) return(0);
    }
    if (E != DD_ONE(dd)) {
	retval = fprintf(fp, "%s%s(", T != DD_ONE(dd) ? " * " : "",
			 E != cuddE(f) ? "!" : "");
	if (retval == EOF) return(0);
	retval = ddDoDumpFactoredForm(dd,E,fp,names);
	if (retval != 1) return(retval);
	retval = fprintf(fp, ")");
	if (retval == EOF) return(0);
    }
    return(1);

} /* end of ddDoDumpFactoredForm */
