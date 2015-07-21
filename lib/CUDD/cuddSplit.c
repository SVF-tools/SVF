/**CFile***********************************************************************

  FileName    [cuddSplit.c]

  PackageName [cudd]

  Synopsis    [Returns a subset of minterms from a boolean function.]

  Description [External functions included in this modoule:
		<ul>
		<li> Cudd_SplitSet()
		</ul>
	Internal functions included in this module:
		<ul>
		<li> cuddSplitSetRecur()
		</u>
        Static functions included in this module:
		<ul>
		<li> selectMintermsFromUniverse()
		<li> mintermsFromUniverse()
		<li> bddAnnotateMintermCount()
		</ul> ]

  SeeAlso     []

  Author      [Balakrishna Kumthekar]

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
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Structure declarations                                                    */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdNode * selectMintermsFromUniverse (DdManager *manager, int *varSeen, double n);
static DdNode * mintermsFromUniverse (DdManager *manager, DdNode **vars, int numVars, double n, int index);
static double bddAnnotateMintermCount (DdManager *manager, DdNode *node, double max, st_table *table);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Returns m minterms from a BDD.]

  Description [Returns <code>m</code> minterms from a BDD whose
  support has <code>n</code> variables at most.  The procedure tries
  to create as few extra nodes as possible. The function represented
  by <code>S</code> depends on at most <code>n</code> of the variables
  in <code>xVars</code>. Returns a BDD with <code>m</code> minterms
  of the on-set of S if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *
Cudd_SplitSet(
  DdManager * manager,
  DdNode * S,
  DdNode ** xVars,
  int  n,
  double  m)
{
    DdNode *result;
    DdNode *zero, *one;
    double  max, num;
    st_table *mtable;
    int *varSeen;
    int i,index, size;

    size = manager->size;
    one = DD_ONE(manager);
    zero = Cudd_Not(one);

    /* Trivial cases. */
    if (m == 0.0) {
	return(zero);
    }
    if (S == zero) {
	return(NULL);
    }

    max = pow(2.0,(double)n);
    if (m > max)
	return(NULL);

    do {
	manager->reordered = 0;
	/* varSeen is used to mark the variables that are encountered
	** while traversing the BDD S.
	*/
	varSeen = ALLOC(int, size);
	if (varSeen == NULL) {
	    manager->errorCode = CUDD_MEMORY_OUT;
	    return(NULL);
	}
	for (i = 0; i < size; i++) {
	    varSeen[i] = -1;
	}
	for (i = 0; i < n; i++) {
	    index = (xVars[i])->index;
	    varSeen[manager->invperm[index]] = 0;
	}

	if (S == one) {
	    if (m == max) {
		FREE(varSeen);
		return(S);
	    }
	    result = selectMintermsFromUniverse(manager,varSeen,m);
	    if (result)
		cuddRef(result);
	    FREE(varSeen);
	} else {
	    mtable = st_init_table(st_ptrcmp,st_ptrhash);
	    if (mtable == NULL) {
		(void) fprintf(manager->out,
			       "Cudd_SplitSet: out-of-memory.\n");
		FREE(varSeen);
		manager->errorCode = CUDD_MEMORY_OUT;
		return(NULL);
	    }
	    /* The nodes of BDD S are annotated by the number of minterms
	    ** in their onset. The node and the number of minterms in its
	    ** onset are stored in mtable.
	    */
	    num = bddAnnotateMintermCount(manager,S,max,mtable);
	    if (m == num) {
		st_foreach(mtable,cuddStCountfree,NIL(char));
		st_free_table(mtable);
		FREE(varSeen);
		return(S);
	    }
	    
	    result = cuddSplitSetRecur(manager,mtable,varSeen,S,m,max,0);
	    if (result)
		cuddRef(result);
	    st_foreach(mtable,cuddStCountfree,NULL);
	    st_free_table(mtable);
	    FREE(varSeen);
	}
    } while (manager->reordered == 1);

    cuddDeref(result);
    return(result);

} /* end of Cudd_SplitSet */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_SplitSet.]

  Description [Implements the recursive step of Cudd_SplitSet. The
  procedure recursively traverses the BDD and checks to see if any
  node satisfies the minterm requirements as specified by 'n'. At any
  node X, n is compared to the number of minterms in the onset of X's
  children. If either of the child nodes have exactly n minterms, then
  that node is returned; else, if n is greater than the onset of one
  of the child nodes, that node is retained and the difference in the
  number of minterms is extracted from the other child. In case n
  minterms can be extracted from constant 1, the algorithm returns the
  result with at most log(n) nodes.]

  SideEffects [The array 'varSeen' is updated at every recursive call
  to set the variables traversed by the procedure.]

  SeeAlso     []

******************************************************************************/
DdNode*
cuddSplitSetRecur(
  DdManager * manager,
  st_table * mtable,
  int * varSeen,
  DdNode * p,
  double  n,
  double  max,
  int  index)
{
    DdNode *one, *zero, *N, *Nv;
    DdNode *Nnv, *q, *r, *v;
    DdNode *result;
    double *dummy, numT, numE;
    int variable, positive;
  
    statLine(manager);
    one = DD_ONE(manager);
    zero = Cudd_Not(one);

    /* If p is constant, extract n minterms from constant 1.  The procedure by
    ** construction guarantees that minterms will not be extracted from
    ** constant 0.
    */
    if (Cudd_IsConstant(p)) {
	q = selectMintermsFromUniverse(manager,varSeen,n);
	return(q);
    }

    N = Cudd_Regular(p);

    /* Set variable as seen. */
    variable = N->index;
    varSeen[manager->invperm[variable]] = -1;

    Nv = cuddT(N);
    Nnv = cuddE(N);
    if (Cudd_IsComplement(p)) {
	Nv = Cudd_Not(Nv);
	Nnv = Cudd_Not(Nnv);
    }

    /* If both the children of 'p' are constants, extract n minterms from a
    ** constant node.
    */
    if (Cudd_IsConstant(Nv) && Cudd_IsConstant(Nnv)) {
	q = selectMintermsFromUniverse(manager,varSeen,n);
	if (q == NULL) {
	    return(NULL);
	}
	cuddRef(q);
	r = cuddBddAndRecur(manager,p,q);
	if (r == NULL) {
	    Cudd_RecursiveDeref(manager,q);
	    return(NULL);
	}
	cuddRef(r);
	Cudd_RecursiveDeref(manager,q);
	cuddDeref(r);
	return(r);
    }
  
    /* Lookup the # of minterms in the onset of the node from the table. */
    if (!Cudd_IsConstant(Nv)) {
	if (!st_lookup(mtable, Nv, &dummy)) return(NULL);
	numT = *dummy/(2*(1<<index));
    } else if (Nv == one) {
	numT = max/(2*(1<<index));
    } else {
	numT = 0;
    }
  
    if (!Cudd_IsConstant(Nnv)) {
	if (!st_lookup(mtable, Nnv, &dummy)) return(NULL);
	numE = *dummy/(2*(1<<index));
    } else if (Nnv == one) {
	numE = max/(2*(1<<index));
    } else {
	numE = 0;
    }

    v = cuddUniqueInter(manager,variable,one,zero);
    cuddRef(v);

    /* If perfect match. */
    if (numT == n) {
	q = cuddBddAndRecur(manager,v,Nv);
	if (q == NULL) {
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(q);
	Cudd_RecursiveDeref(manager,v);
	cuddDeref(q);
	return(q);
    }
    if (numE == n) {
	q = cuddBddAndRecur(manager,Cudd_Not(v),Nnv);
	if (q == NULL) {
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(q);
	Cudd_RecursiveDeref(manager,v);
	cuddDeref(q);
	return(q);
    }
    /* If n is greater than numT, extract the difference from the ELSE child
    ** and retain the function represented by the THEN branch.
    */
    if (numT < n) {
	q = cuddSplitSetRecur(manager,mtable,varSeen,
			      Nnv,(n-numT),max,index+1);
	if (q == NULL) {
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(q);
	r = cuddBddIteRecur(manager,v,Nv,q);
	if (r == NULL) {
	    Cudd_RecursiveDeref(manager,q);
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(r);
	Cudd_RecursiveDeref(manager,q);
	Cudd_RecursiveDeref(manager,v);
	cuddDeref(r);
	return(r);
    }
    /* If n is greater than numE, extract the difference from the THEN child
    ** and retain the function represented by the ELSE branch.
    */
    if (numE < n) {
	q = cuddSplitSetRecur(manager,mtable,varSeen,
			      Nv, (n-numE),max,index+1);
	if (q == NULL) {
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(q);
	r = cuddBddIteRecur(manager,v,q,Nnv);
	if (r == NULL) {
	    Cudd_RecursiveDeref(manager,q);
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(r);
	Cudd_RecursiveDeref(manager,q);
	Cudd_RecursiveDeref(manager,v);
	cuddDeref(r);    
	return(r);
    }

    /* None of the above cases; (n < numT and n < numE) and either of
    ** the Nv, Nnv or both are not constants. If possible extract the
    ** required minterms the constant branch.
    */
    if (Cudd_IsConstant(Nv) && !Cudd_IsConstant(Nnv)) {
	q = selectMintermsFromUniverse(manager,varSeen,n);
	if (q == NULL) {
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(q);
	result = cuddBddAndRecur(manager,v,q);
	if (result == NULL) {
	    Cudd_RecursiveDeref(manager,q);
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(result);
	Cudd_RecursiveDeref(manager,q);
	Cudd_RecursiveDeref(manager,v);
	cuddDeref(result);
	return(result);
    } else if (!Cudd_IsConstant(Nv) && Cudd_IsConstant(Nnv)) {
	q = selectMintermsFromUniverse(manager,varSeen,n);
	if (q == NULL) {
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(q);
	result = cuddBddAndRecur(manager,Cudd_Not(v),q);
	if (result == NULL) {
	    Cudd_RecursiveDeref(manager,q);
	    Cudd_RecursiveDeref(manager,v);
	    return(NULL);
	}
	cuddRef(result);
	Cudd_RecursiveDeref(manager,q);
	Cudd_RecursiveDeref(manager,v);
	cuddDeref(result);
	return(result);
    }

    /* Both Nv and Nnv are not constants. So choose the one which
    ** has fewer minterms in its onset.
    */
    positive = 0;
    if (numT < numE) {
	q = cuddSplitSetRecur(manager,mtable,varSeen,
			      Nv,n,max,index+1);
	positive = 1;
    } else {
	q = cuddSplitSetRecur(manager,mtable,varSeen,
			      Nnv,n,max,index+1);
    }

    if (q == NULL) {
	Cudd_RecursiveDeref(manager,v);
	return(NULL);
    }
    cuddRef(q);

    if (positive) {
	result = cuddBddAndRecur(manager,v,q);
    } else {
	result = cuddBddAndRecur(manager,Cudd_Not(v),q);
    }
    if (result == NULL) {
	Cudd_RecursiveDeref(manager,q);
	Cudd_RecursiveDeref(manager,v);
	return(NULL);
    }
    cuddRef(result);
    Cudd_RecursiveDeref(manager,q);
    Cudd_RecursiveDeref(manager,v);
    cuddDeref(result);

    return(result);

} /* end of cuddSplitSetRecur */


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [This function prepares an array of variables which have not been
  encountered so far when traversing the procedure cuddSplitSetRecur.]

  Description [This function prepares an array of variables which have not been
  encountered so far when traversing the procedure cuddSplitSetRecur. This
  array is then used to extract the required number of minterms from a constant
  1. The algorithm guarantees that the size of BDD will be utmost \log(n).]

  SideEffects [None]

******************************************************************************/
static DdNode *
selectMintermsFromUniverse(
  DdManager * manager,
  int * varSeen,
  double  n)
{
    int numVars;
    int i, size, j;
     DdNode *one, *zero, *result;
    DdNode **vars;

    numVars = 0;
    size = manager->size;
    one = DD_ONE(manager);
    zero = Cudd_Not(one);

    /* Count the number of variables not encountered so far in procedure
    ** cuddSplitSetRecur.
    */
    for (i = size-1; i >= 0; i--) {
	if(varSeen[i] == 0)
	    numVars++;
    }
    vars = ALLOC(DdNode *, numVars);
    if (!vars) {
	manager->errorCode = CUDD_MEMORY_OUT;
	return(NULL);
    }

    j = 0;
    for (i = size-1; i >= 0; i--) {
	if(varSeen[i] == 0) {
	    vars[j] = cuddUniqueInter(manager,manager->perm[i],one,zero);
	    cuddRef(vars[j]);
	    j++;
	}
    }

    /* Compute a function which has n minterms and depends on at most
    ** numVars variables.
    */
    result = mintermsFromUniverse(manager,vars,numVars,n, 0);
    if (result) 
	cuddRef(result);

    for (i = 0; i < numVars; i++)
	Cudd_RecursiveDeref(manager,vars[i]);
    FREE(vars);

    return(result);

} /* end of selectMintermsFromUniverse */


/**Function********************************************************************

  Synopsis    [Recursive procedure to extract n mintems from constant 1.]

  Description [Recursive procedure to extract n mintems from constant 1.]

  SideEffects [None]

******************************************************************************/
static DdNode *
mintermsFromUniverse(
  DdManager * manager,
  DdNode ** vars,
  int  numVars,
  double  n,
  int  index)
{
    DdNode *one, *zero;
    DdNode *q, *result;
    double max, max2;
    
    statLine(manager);
    one = DD_ONE(manager);
    zero = Cudd_Not(one);

    max = pow(2.0, (double)numVars);
    max2 = max / 2.0;

    if (n == max)
	return(one);
    if (n == 0.0)
	return(zero);
    /* if n == 2^(numVars-1), return a single variable */
    if (n == max2)
	return vars[index];
    else if (n > max2) {
	/* When n > 2^(numVars-1), a single variable vars[index]
	** contains 2^(numVars-1) minterms. The rest are extracted
	** from a constant with 1 less variable.
	*/
	q = mintermsFromUniverse(manager,vars,numVars-1,(n-max2),index+1);
	if (q == NULL)
	    return(NULL);
	cuddRef(q);
	result = cuddBddIteRecur(manager,vars[index],one,q);
    } else {
	/* When n < 2^(numVars-1), a literal of variable vars[index]
	** is selected. The required n minterms are extracted from a
	** constant with 1 less variable.
	*/
	q = mintermsFromUniverse(manager,vars,numVars-1,n,index+1);
	if (q == NULL)
	    return(NULL);
	cuddRef(q);
	result = cuddBddAndRecur(manager,vars[index],q);
    }
    
    if (result == NULL) {
	Cudd_RecursiveDeref(manager,q);
	return(NULL);
    }
    cuddRef(result);
    Cudd_RecursiveDeref(manager,q);
    cuddDeref(result);
    return(result);

} /* end of mintermsFromUniverse */


/**Function********************************************************************

  Synopsis    [Annotates every node in the BDD node with its minterm count.]

  Description [Annotates every node in the BDD node with its minterm count.
  In this function, every node and the minterm count represented by it are
  stored in a hash table.]

  SideEffects [Fills up 'table' with the pair <node,minterm_count>.]

******************************************************************************/
static double
bddAnnotateMintermCount(
  DdManager * manager,
  DdNode * node,
  double  max,
  st_table * table)
{

    DdNode *N,*Nv,*Nnv;
    register double min_v,min_nv;
    register double min_N;
    double *pmin;
    double *dummy;

    statLine(manager);
    N = Cudd_Regular(node);
    if (cuddIsConstant(N)) {
	if (node == DD_ONE(manager)) {
	    return(max);
	} else {
	    return(0.0);
	}
    }

    if (st_lookup(table, node, &dummy)) {
	return(*dummy);
    }	
  
    Nv = cuddT(N);
    Nnv = cuddE(N);
    if (N != node) {
	Nv = Cudd_Not(Nv);
	Nnv = Cudd_Not(Nnv);
    }

    /* Recur on the two branches. */
    min_v  = bddAnnotateMintermCount(manager,Nv,max,table) / 2.0;
    if (min_v == (double)CUDD_OUT_OF_MEM)
	return ((double)CUDD_OUT_OF_MEM);
    min_nv = bddAnnotateMintermCount(manager,Nnv,max,table) / 2.0;
    if (min_nv == (double)CUDD_OUT_OF_MEM)
	return ((double)CUDD_OUT_OF_MEM);
    min_N  = min_v + min_nv;

    pmin = ALLOC(double,1);
    if (pmin == NULL) {
	manager->errorCode = CUDD_MEMORY_OUT;
	return((double)CUDD_OUT_OF_MEM);
    }
    *pmin = min_N;

    if (st_insert(table,(char *)node, (char *)pmin) == ST_OUT_OF_MEM) {
	FREE(pmin);
	return((double)CUDD_OUT_OF_MEM);
    }
    
    return(min_N);

} /* end of bddAnnotateMintermCount */
