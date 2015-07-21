/**CFile***********************************************************************

  FileName    [cuddCheck.c]

  PackageName [cudd]

  Synopsis    [Functions to check consistency of data structures.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_DebugCheck()
		<li> Cudd_CheckKeys()
		</ul>
	       Internal procedures included in this module:
		<ul>
		<li> cuddHeapProfile()
		<li> cuddPrintNode()
		<li> cuddPrintVarGroups()
		</ul>
	       Static procedures included in this module:
		<ul>
		<li> debugFindParent()
		</ul>
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
static char rcsid[] DD_UNUSED = "$Id: cuddCheck.c,v 1.37 2012/02/05 01:07:18 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void debugFindParent (DdManager *table, DdNode *node);
#if 0
static void debugCheckParent (DdManager *table, DdNode *node);
#endif

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Checks for inconsistencies in the DD heap.]

  Description [Checks for inconsistencies in the DD heap:
  <ul>
  <li> node has illegal index
  <li> live node has dead children
  <li> node has illegal Then or Else pointers
  <li> BDD/ADD node has identical children
  <li> ZDD node has zero then child
  <li> wrong number of total nodes
  <li> wrong number of dead nodes
  <li> ref count error at node
  </ul>
  Returns 0 if no inconsistencies are found; DD_OUT_OF_MEM if there is
  not enough memory; 1 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_CheckKeys]

******************************************************************************/
int
Cudd_DebugCheck(
  DdManager * table)
{
    unsigned int i;
    int		j,count;
    int		slots;
    DdNodePtr	*nodelist;
    DdNode	*f;
    DdNode	*sentinel = &(table->sentinel);
    st_table	*edgeTable;	/* stores internal ref count for each node */
    st_generator	*gen;
    int		flag = 0;
    int		totalNode;
    int		deadNode;
    int		index;
    int         shift;

    edgeTable = st_init_table(st_ptrcmp,st_ptrhash);
    if (edgeTable == NULL) return(CUDD_OUT_OF_MEM);

    /* Check the BDD/ADD subtables. */
    for (i = 0; i < (unsigned) table->size; i++) {
	index = table->invperm[i];
	if (i != (unsigned) table->perm[index]) {
	    (void) fprintf(table->err,
			   "Permutation corrupted: invperm[%u] = %d\t perm[%d] = %d\n",
			   i, index, index, table->perm[index]);
	}
	nodelist = table->subtables[i].nodelist;
	slots = table->subtables[i].slots;
	shift = table->subtables[i].shift;

	totalNode = 0;
	deadNode = 0;
	for (j = 0; j < slots; j++) {	/* for each subtable slot */
	    f = nodelist[j];
	    while (f != sentinel) {
		totalNode++;
		if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref != 0) {
		    if ((int) f->index != index) {
			(void) fprintf(table->err,
				       "Error: node has illegal index\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if ((unsigned) cuddI(table,cuddT(f)->index) <= i ||
			(unsigned) cuddI(table,Cudd_Regular(cuddE(f))->index)
			<= i) {
			(void) fprintf(table->err,
				       "Error: node has illegal children\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if (Cudd_Regular(cuddT(f)) != cuddT(f)) {
			(void) fprintf(table->err,
				       "Error: node has illegal form\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if (cuddT(f) == cuddE(f)) {
			(void) fprintf(table->err,
				       "Error: node has identical children\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if (cuddT(f)->ref == 0 || Cudd_Regular(cuddE(f))->ref == 0) {
			(void) fprintf(table->err,
				       "Error: live node has dead children\n");
			cuddPrintNode(f,table->err);
			flag =1;
		    }
                    if (ddHash(cuddT(f),cuddE(f),shift) != j) {
                        (void) fprintf(table->err, "Error: misplaced node\n");
			cuddPrintNode(f,table->err);
			flag =1;
                    }
		    /* Increment the internal reference count for the
		    ** then child of the current node.
		    */
		    if (st_lookup_int(edgeTable,(char *)cuddT(f),&count)) {
			count++;
		    } else {
			count = 1;
		    }
		    if (st_insert(edgeTable,(char *)cuddT(f),
		    (char *)(long)count) == ST_OUT_OF_MEM) {
			st_free_table(edgeTable);
			return(CUDD_OUT_OF_MEM);
		    }

		    /* Increment the internal reference count for the
		    ** else child of the current node.
		    */
		    if (st_lookup_int(edgeTable,(char *)Cudd_Regular(cuddE(f)),
				      &count)) {
			count++;
		    } else {
			count = 1;
		    }
		    if (st_insert(edgeTable,(char *)Cudd_Regular(cuddE(f)),
		    (char *)(long)count) == ST_OUT_OF_MEM) {
			st_free_table(edgeTable);
			return(CUDD_OUT_OF_MEM);
		    }
		} else if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref == 0) {
		    deadNode++;
#if 0
		    debugCheckParent(table,f);
#endif
		} else {
		    fprintf(table->err,
			    "Error: node has illegal Then or Else pointers\n");
		    cuddPrintNode(f,table->err);
		    flag = 1;
		}

		f = f->next;
	    }	/* for each element of the collision list */
	}	/* for each subtable slot */

	if ((unsigned) totalNode != table->subtables[i].keys) {
	    fprintf(table->err,"Error: wrong number of total nodes\n");
	    flag = 1;
	}
	if ((unsigned) deadNode != table->subtables[i].dead) {
	    fprintf(table->err,"Error: wrong number of dead nodes\n");
	    flag = 1;
	}
    }	/* for each BDD/ADD subtable */

    /* Check the ZDD subtables. */
    for (i = 0; i < (unsigned) table->sizeZ; i++) {
	index = table->invpermZ[i];
	if (i != (unsigned) table->permZ[index]) {
	    (void) fprintf(table->err,
			   "Permutation corrupted: invpermZ[%u] = %d\t permZ[%d] = %d in ZDD\n",
			   i, index, index, table->permZ[index]);
	}
	nodelist = table->subtableZ[i].nodelist;
	slots = table->subtableZ[i].slots;

	totalNode = 0;
	deadNode = 0;
	for (j = 0; j < slots; j++) {	/* for each subtable slot */
	    f = nodelist[j];
	    while (f != NULL) {
		totalNode++;
		if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref != 0) {
		    if ((int) f->index != index) {
			(void) fprintf(table->err,
				       "Error: ZDD node has illegal index\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if (Cudd_IsComplement(cuddT(f)) ||
			Cudd_IsComplement(cuddE(f))) {
			(void) fprintf(table->err,
				       "Error: ZDD node has complemented children\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if ((unsigned) cuddIZ(table,cuddT(f)->index) <= i ||
		    (unsigned) cuddIZ(table,cuddE(f)->index) <= i) {
			(void) fprintf(table->err,
				       "Error: ZDD node has illegal children\n");
			cuddPrintNode(f,table->err);
			cuddPrintNode(cuddT(f),table->err);
			cuddPrintNode(cuddE(f),table->err);
			flag = 1;
		    }
		    if (cuddT(f) == DD_ZERO(table)) {
			(void) fprintf(table->err,
				       "Error: ZDD node has zero then child\n");
			cuddPrintNode(f,table->err);
			flag = 1;
		    }
		    if (cuddT(f)->ref == 0 || cuddE(f)->ref == 0) {
			(void) fprintf(table->err,
				       "Error: ZDD live node has dead children\n");
			cuddPrintNode(f,table->err);
			flag =1;
		    }
		    /* Increment the internal reference count for the
		    ** then child of the current node.
		    */
		    if (st_lookup_int(edgeTable,(char *)cuddT(f),&count)) {
			count++;
		    } else {
			count = 1;
		    }
		    if (st_insert(edgeTable,(char *)cuddT(f),
		    (char *)(long)count) == ST_OUT_OF_MEM) {
			st_free_table(edgeTable);
			return(CUDD_OUT_OF_MEM);
		    }

		    /* Increment the internal reference count for the
		    ** else child of the current node.
		    */
		    if (st_lookup_int(edgeTable,(char *)cuddE(f),&count)) {
			count++;
		    } else {
			count = 1;
		    }
		    if (st_insert(edgeTable,(char *)cuddE(f),
		    (char *)(long)count) == ST_OUT_OF_MEM) {
			st_free_table(edgeTable);
			table->errorCode = CUDD_MEMORY_OUT;
			return(CUDD_OUT_OF_MEM);
		    }
		} else if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref == 0) {
		    deadNode++;
#if 0
		    debugCheckParent(table,f);
#endif
		} else {
		    fprintf(table->err,
			    "Error: ZDD node has illegal Then or Else pointers\n");
		    cuddPrintNode(f,table->err);
		    flag = 1;
		}

		f = f->next;
	    }	/* for each element of the collision list */
	}	/* for each subtable slot */

	if ((unsigned) totalNode != table->subtableZ[i].keys) {
	    fprintf(table->err,
		    "Error: wrong number of total nodes in ZDD\n");
	    flag = 1;
	}
	if ((unsigned) deadNode != table->subtableZ[i].dead) {
	    fprintf(table->err,
		    "Error: wrong number of dead nodes in ZDD\n");
	    flag = 1;
	}
    }	/* for each ZDD subtable */

    /* Check the constant table. */
    nodelist = table->constants.nodelist;
    slots = table->constants.slots;

    totalNode = 0;
    deadNode = 0;
    for (j = 0; j < slots; j++) {
	f = nodelist[j];
	while (f != NULL) {
	    totalNode++;
	    if (f->ref != 0) {
		if (f->index != CUDD_CONST_INDEX) {
		    fprintf(table->err,"Error: node has illegal index\n");
#if SIZEOF_VOID_P == 8
		    fprintf(table->err,
			    "       node 0x%lx, id = %u, ref = %u, value = %g\n",
			    (ptruint)f,f->index,f->ref,cuddV(f));
#else
		    fprintf(table->err,
			    "       node 0x%x, id = %hu, ref = %hu, value = %g\n",
			    (ptruint)f,f->index,f->ref,cuddV(f));
#endif
		    flag = 1;
		}
	    } else {
		deadNode++;
	    }
	    f = f->next;
	}
    }
    if ((unsigned) totalNode != table->constants.keys) {
	(void) fprintf(table->err,
		       "Error: wrong number of total nodes in constants\n");
	flag = 1;
    }
    if ((unsigned) deadNode != table->constants.dead) {
	(void) fprintf(table->err,
		       "Error: wrong number of dead nodes in constants\n");
	flag = 1;
    }
    gen = st_init_gen(edgeTable);
    while (st_gen(gen, &f, &count)) {
	if (count > (int)(f->ref) && f->ref != DD_MAXREF) {
#if SIZEOF_VOID_P == 8
	    fprintf(table->err,"ref count error at node 0x%lx, count = %d, id = %u, ref = %u, then = 0x%lx, else = 0x%lx\n",(ptruint)f,count,f->index,f->ref,(ptruint)cuddT(f),(ptruint)cuddE(f));
#else
	    fprintf(table->err,"ref count error at node 0x%x, count = %d, id = %hu, ref = %hu, then = 0x%x, else = 0x%x\n",(ptruint)f,count,f->index,f->ref,(ptruint)cuddT(f),(ptruint)cuddE(f));
#endif
	    debugFindParent(table,f);
	    flag = 1;
	}
    }
    st_free_gen(gen);
    st_free_table(edgeTable);

    return (flag);

} /* end of Cudd_DebugCheck */


/**Function********************************************************************

  Synopsis    [Checks for several conditions that should not occur.]

  Description [Checks for the following conditions:
  <ul>
  <li>Wrong sizes of subtables.
  <li>Wrong number of keys found in unique subtable.
  <li>Wrong number of dead found in unique subtable.
  <li>Wrong number of keys found in the constant table
  <li>Wrong number of dead found in the constant table
  <li>Wrong number of total slots found
  <li>Wrong number of maximum keys found
  <li>Wrong number of total dead found
  </ul>
  Reports the average length of non-empty lists. Returns the number of
  subtables for which the number of keys is wrong.]

  SideEffects [None]

  SeeAlso     [Cudd_DebugCheck]

******************************************************************************/
int
Cudd_CheckKeys(
  DdManager * table)
{
    int size;
    int i,j;
    DdNodePtr *nodelist;
    DdNode *node;
    DdNode *sentinel = &(table->sentinel);
    DdSubtable *subtable;
    int keys;
    int dead;
    int count = 0;
    int totalKeys = 0;
    int totalSlots = 0;
    int totalDead = 0;
    int nonEmpty = 0;
    unsigned int slots;
    int logSlots;
    int shift;

    size = table->size;

    for (i = 0; i < size; i++) {
	subtable = &(table->subtables[i]);
	nodelist = subtable->nodelist;
	keys = subtable->keys;
	dead = subtable->dead;
	totalKeys += keys;
	slots = subtable->slots;
	shift = subtable->shift;
	logSlots = sizeof(int) * 8 - shift;
	if (((slots >> logSlots) << logSlots) != slots) {
	    (void) fprintf(table->err,
			   "Unique table %d is not the right power of 2\n", i);
	    (void) fprintf(table->err,
			   "    slots = %u shift = %d\n", slots, shift);
	}
	totalSlots += slots;
	totalDead += dead;
	for (j = 0; (unsigned) j < slots; j++) {
	    node = nodelist[j];
	    if (node != sentinel) {
		nonEmpty++;
	    }
	    while (node != sentinel) {
		keys--;
		if (node->ref == 0) {
		    dead--;
		}
		node = node->next;
	    }
	}
	if (keys != 0) {
	    (void) fprintf(table->err, "Wrong number of keys found \
in unique table %d (difference=%d)\n", i, keys);
	    count++;
	}
	if (dead != 0) {
	    (void) fprintf(table->err, "Wrong number of dead found \
in unique table no. %d (difference=%d)\n", i, dead);
	}
    }	/* for each BDD/ADD subtable */

    /* Check the ZDD subtables. */
    size = table->sizeZ;

    for (i = 0; i < size; i++) {
	subtable = &(table->subtableZ[i]);
	nodelist = subtable->nodelist;
	keys = subtable->keys;
	dead = subtable->dead;
	totalKeys += keys;
	totalSlots += subtable->slots;
	totalDead += dead;
	for (j = 0; (unsigned) j < subtable->slots; j++) {
	    node = nodelist[j];
	    if (node != NULL) {
		nonEmpty++;
	    }
	    while (node != NULL) {
		keys--;
		if (node->ref == 0) {
		    dead--;
		}
		node = node->next;
	    }
	}
	if (keys != 0) {
	    (void) fprintf(table->err, "Wrong number of keys found \
in ZDD unique table no. %d (difference=%d)\n", i, keys);
	    count++;
	}
	if (dead != 0) {
	    (void) fprintf(table->err, "Wrong number of dead found \
in ZDD unique table no. %d (difference=%d)\n", i, dead);
	}
    }	/* for each ZDD subtable */

    /* Check the constant table. */
    subtable = &(table->constants);
    nodelist = subtable->nodelist;
    keys = subtable->keys;
    dead = subtable->dead;
    totalKeys += keys;
    totalSlots += subtable->slots;
    totalDead += dead;
    for (j = 0; (unsigned) j < subtable->slots; j++) {
	node = nodelist[j];
	if (node != NULL) {
	    nonEmpty++;
	}
	while (node != NULL) {
	    keys--;
	    if (node->ref == 0) {
		dead--;
	    }
	    node = node->next;
	}
    }
    if (keys != 0) {
	(void) fprintf(table->err, "Wrong number of keys found \
in the constant table (difference=%d)\n", keys);
	count++;
    }
    if (dead != 0) {
	(void) fprintf(table->err, "Wrong number of dead found \
in the constant table (difference=%d)\n", dead);
    }
    if ((unsigned) totalKeys != table->keys + table->keysZ) {
	(void) fprintf(table->err, "Wrong number of total keys found \
(difference=%d)\n", (int) (totalKeys-table->keys));
    }
    if ((unsigned) totalSlots != table->slots) {
	(void) fprintf(table->err, "Wrong number of total slots found \
(difference=%d)\n", (int) (totalSlots-table->slots));
    }
    if (table->minDead != (unsigned) (table->gcFrac * table->slots)) {
	(void) fprintf(table->err, "Wrong number of minimum dead found \
(%u vs. %u)\n", table->minDead,
	(unsigned) (table->gcFrac * (double) table->slots));
    }
    if ((unsigned) totalDead != table->dead + table->deadZ) {
	(void) fprintf(table->err, "Wrong number of total dead found \
(difference=%d)\n", (int) (totalDead-table->dead));
    }
    (void) fprintf(table->out,"Average length of non-empty lists = %g\n",
                   (double) table->keys / (double) nonEmpty);

    return(count);

} /* end of Cudd_CheckKeys */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Prints information about the heap.]

  Description [Prints to the manager's stdout the number of live nodes for each
  level of the DD heap that contains at least one live node.  It also
  prints a summary containing:
  <ul>
  <li> total number of tables;
  <li> number of tables with live nodes;
  <li> table with the largest number of live nodes;
  <li> number of nodes in that table.
  </ul>
  If more than one table contains the maximum number of live nodes,
  only the one of lowest index is reported. Returns 1 in case of success
  and 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
cuddHeapProfile(
  DdManager * dd)
{
    int ntables = dd->size;
    DdSubtable *subtables = dd->subtables;
    int i,		/* loop index */
	nodes,		/* live nodes in i-th layer */
	retval,		/* return value of fprintf */
	largest = -1,	/* index of the table with most live nodes */
	maxnodes = -1,	/* maximum number of live nodes in a table */
	nonempty = 0;	/* number of tables with live nodes */

    /* Print header. */
#if SIZEOF_VOID_P == 8
    retval = fprintf(dd->out,"*** DD heap profile for 0x%lx ***\n",
		     (ptruint) dd);
#else
    retval = fprintf(dd->out,"*** DD heap profile for 0x%x ***\n",
		     (ptruint) dd);
#endif
    if (retval == EOF) return 0;

    /* Print number of live nodes for each nonempty table. */
    for (i=0; i<ntables; i++) {
	nodes = subtables[i].keys - subtables[i].dead;
	if (nodes) {
	    nonempty++;
	    retval = fprintf(dd->out,"%5d: %5d nodes\n", i, nodes);
	    if (retval == EOF) return 0;
	    if (nodes > maxnodes) {
		maxnodes = nodes;
		largest = i;
	    }
	}
    }

    nodes = dd->constants.keys - dd->constants.dead;
    if (nodes) {
	nonempty++;
	retval = fprintf(dd->out,"const: %5d nodes\n", nodes);
	if (retval == EOF) return 0;
	if (nodes > maxnodes) {
	    maxnodes = nodes;
	    largest = CUDD_CONST_INDEX;
	}
    }

    /* Print summary. */
    retval = fprintf(dd->out,"Summary: %d tables, %d non-empty, largest: %d ",
	  ntables+1, nonempty, largest);
    if (retval == EOF) return 0;
    retval = fprintf(dd->out,"(with %d nodes)\n", maxnodes);
    if (retval == EOF) return 0;

    return(1);

} /* end of cuddHeapProfile */


/**Function********************************************************************

  Synopsis    [Prints out information on a node.]

  Description [Prints out information on a node.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddPrintNode(
  DdNode * f,
  FILE *fp)
{
    f = Cudd_Regular(f);
#if SIZEOF_VOID_P == 8
    (void) fprintf(fp,"       node 0x%lx, id = %u, ref = %u, then = 0x%lx, else = 0x%lx\n",(ptruint)f,f->index,f->ref,(ptruint)cuddT(f),(ptruint)cuddE(f));
#else
    (void) fprintf(fp,"       node 0x%x, id = %hu, ref = %hu, then = 0x%x, else = 0x%x\n",(ptruint)f,f->index,f->ref,(ptruint)cuddT(f),(ptruint)cuddE(f));
#endif

} /* end of cuddPrintNode */



/**Function********************************************************************

  Synopsis    [Prints the variable groups as a parenthesized list.]

  Description [Prints the variable groups as a parenthesized list.
  For each group the level range that it represents is printed. After
  each group, the group's flags are printed, preceded by a `|'.  For
  each flag (except MTR_TERMINAL) a character is printed.
  <ul>
  <li>F: MTR_FIXED
  <li>N: MTR_NEWNODE
  <li>S: MTR_SOFT
  </ul>
  The second argument, silent, if different from 0, causes
  Cudd_PrintVarGroups to only check the syntax of the group tree.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
cuddPrintVarGroups(
  DdManager * dd /* manager */,
  MtrNode * root /* root of the group tree */,
  int zdd /* 0: BDD; 1: ZDD */,
  int silent /* flag to check tree syntax only */)
{
    MtrNode *node;
    int level;

    assert(root != NULL);
    assert(root->younger == NULL || root->younger->elder == root);
    assert(root->elder == NULL || root->elder->younger == root);
    if (zdd) {
	level = dd->permZ[root->index];
    } else {
	level = dd->perm[root->index];
    }
    if (!silent) (void) printf("(%d",level);
    if (MTR_TEST(root,MTR_TERMINAL) || root->child == NULL) {
	if (!silent) (void) printf(",");
    } else {
	node = root->child;
	while (node != NULL) {
	    assert(node->low >= root->low && (int) (node->low + node->size) <= (int) (root->low + root->size));
	    assert(node->parent == root);
	    cuddPrintVarGroups(dd,node,zdd,silent);
	    node = node->younger;
	}
    }
    if (!silent) {
	(void) printf("%d", (int) (level + root->size - 1));
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

} /* end of cuddPrintVarGroups */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Searches the subtables above node for its parents.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void
debugFindParent(
  DdManager * table,
  DdNode * node)
{
    int         i,j;
    int		slots;
    DdNodePtr	*nodelist;
    DdNode	*f;

    for (i = 0; i < cuddI(table,node->index); i++) {
	nodelist = table->subtables[i].nodelist;
	slots = table->subtables[i].slots;

	for (j=0;j<slots;j++) {
	    f = nodelist[j];
	    while (f != NULL) {
		if (cuddT(f) == node || Cudd_Regular(cuddE(f)) == node) {
#if SIZEOF_VOID_P == 8
		    (void) fprintf(table->out,"parent is at 0x%lx, id = %u, ref = %u, then = 0x%lx, else = 0x%lx\n",
			(ptruint)f,f->index,f->ref,(ptruint)cuddT(f),(ptruint)cuddE(f));
#else
		    (void) fprintf(table->out,"parent is at 0x%x, id = %hu, ref = %hu, then = 0x%x, else = 0x%x\n",
			(ptruint)f,f->index,f->ref,(ptruint)cuddT(f),(ptruint)cuddE(f));
#endif
		}
		f = f->next;
	    }
	}
    }

} /* end of debugFindParent */


#if 0
/**Function********************************************************************

  Synopsis    [Reports an error if a (dead) node has a non-dead parent.]

  Description [Searches all the subtables above node. Very expensive.
  The same check is now implemented more efficiently in ddDebugCheck.]

  SideEffects [None]

  SeeAlso     [debugFindParent]

******************************************************************************/
static void
debugCheckParent(
  DdManager * table,
  DdNode * node)
{
    int         i,j;
    int		slots;
    DdNode	**nodelist,*f;

    for (i = 0; i < cuddI(table,node->index); i++) {
	nodelist = table->subtables[i].nodelist;
	slots = table->subtables[i].slots;

	for (j=0;j<slots;j++) {
	    f = nodelist[j];
	    while (f != NULL) {
		if ((Cudd_Regular(cuddE(f)) == node || cuddT(f) == node) && f->ref != 0) {
		    (void) fprintf(table->err,
				   "error with zero ref count\n");
		    (void) fprintf(table->err,"parent is 0x%x, id = %u, ref = %u, then = 0x%x, else = 0x%x\n",f,f->index,f->ref,cuddT(f),cuddE(f));
		    (void) fprintf(table->err,"child  is 0x%x, id = %u, ref = %u, then = 0x%x, else = 0x%x\n",node,node->index,node->ref,cuddT(node),cuddE(node));
		}
		f = f->next;
	    }
	}
    }
}
#endif
