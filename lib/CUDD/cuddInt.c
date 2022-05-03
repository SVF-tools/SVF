/**CFile***********************************************************************

  FileName    [cuddAddIte.c]

  PackageName [cudd]

  Synopsis    [ADD ITE function and satellites.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_addIte()
                <li> Cudd_addIteConstant()
                <li> Cudd_addEvalConst()
                <li> Cudd_addCmpl()
                <li> Cudd_addLeq()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddAddIteRecur()
                <li> cuddAddCmplRecur()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> addVarToConst()
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

#include "CUDD/cuddInt.h"
#include "CUDD/util.h"

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
static char rcsid[] DD_UNUSED =
    "$Id: cuddAddIte.c,v 1.16 2012/02/05 01:07:18 fabio Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void addVarToConst(DdNode *f, DdNode **gp, DdNode **hp, DdNode *one,
                          DdNode *zero);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Implements ITEconstant for ADDs.]

  Description [Implements ITEconstant for ADDs.  f must be a 0-1 ADD.
  Returns a pointer to the resulting ADD (which may or may not be
  constant) or DD_NON_CONSTANT. No new nodes are created. This function
  can be used, for instance, to check that g has a constant value
  (specified by h) whenever f is 1. If the constant value is unknown,
  then one should use Cudd_addEvalConst.]

  SideEffects [None]

  SeeAlso     [Cudd_addIte Cudd_addEvalConst Cudd_bddIteConstant]

******************************************************************************/
DdNode *Cudd_addIteConstant(DdManager *dd, DdNode *f, DdNode *g, DdNode *h) {
  DdNode *one, *zero;
  DdNode *Fv, *Fnv, *Gv, *Gnv, *Hv, *Hnv, *r, *t, *e;
  unsigned int topf, topg, toph, v;

  statLine(dd);
  /* Trivial cases. */
  if (f == (one = DD_ONE(dd))) { /* ITE(1,G,H) = G */
    return (g);
  }
  if (f == (zero = DD_ZERO(dd))) { /* ITE(0,G,H) = H */
    return (h);
  }

  /* From now on, f is known not to be a constant. */
  addVarToConst(f, &g, &h, one, zero);

  /* Check remaining one variable cases. */
  if (g == h) { /* ITE(F,G,G) = G */
    return (g);
  }
  if (cuddIsConstant(g) && cuddIsConstant(h)) {
    return (DD_NON_CONSTANT);
  }

  topf = cuddI(dd, f->index);
  topg = cuddI(dd, g->index);
  toph = cuddI(dd, h->index);
  v = ddMin(topg, toph);

  /* ITE(F,G,H) = (x,G,H) (non constant) if F = (x,1,0), x < top(G,H). */
  if (topf < v && cuddIsConstant(cuddT(f)) && cuddIsConstant(cuddE(f))) {
    return (DD_NON_CONSTANT);
  }

  /* Check cache. */
  r = cuddConstantLookup(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h);
  if (r != NULL) {
    return (r);
  }

  /* Compute cofactors. */
  if (topf <= v) {
    v = ddMin(topf, v); /* v = top_var(F,G,H) */
    Fv = cuddT(f);
    Fnv = cuddE(f);
  } else {
    Fv = Fnv = f;
  }
  if (topg == v) {
    Gv = cuddT(g);
    Gnv = cuddE(g);
  } else {
    Gv = Gnv = g;
  }
  if (toph == v) {
    Hv = cuddT(h);
    Hnv = cuddE(h);
  } else {
    Hv = Hnv = h;
  }

  /* Recursive step. */
  t = Cudd_addIteConstant(dd, Fv, Gv, Hv);
  if (t == DD_NON_CONSTANT || !cuddIsConstant(t)) {
    cuddCacheInsert(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
    return (DD_NON_CONSTANT);
  }
  e = Cudd_addIteConstant(dd, Fnv, Gnv, Hnv);
  if (e == DD_NON_CONSTANT || !cuddIsConstant(e) || t != e) {
    cuddCacheInsert(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
    return (DD_NON_CONSTANT);
  }
  cuddCacheInsert(dd, DD_ADD_ITE_CONSTANT_TAG, f, g, h, t);
  return (t);

} /* end of Cudd_addIteConstant */

/**Function********************************************************************

  Synopsis    [Checks whether ADD g is constant whenever ADD f is 1.]

  Description [Checks whether ADD g is constant whenever ADD f is 1.  f
  must be a 0-1 ADD.  Returns a pointer to the resulting ADD (which may
  or may not be constant) or DD_NON_CONSTANT. If f is identically 0,
  the check is assumed to be successful, and the background value is
  returned. No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_addIteConstant Cudd_addLeq]

******************************************************************************/
DdNode *Cudd_addEvalConst(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *zero;
  DdNode *Fv, *Fnv, *Gv, *Gnv, *r, *t, *e;
  unsigned int topf, topg;

#ifdef DD_DEBUG
  assert(!Cudd_IsComplement(f));
#endif

  statLine(dd);
  /* Terminal cases. */
  if (f == DD_ONE(dd) || cuddIsConstant(g)) {
    return (g);
  }
  if (f == (zero = DD_ZERO(dd))) {
    return (dd->background);
  }

#ifdef DD_DEBUG
  assert(!cuddIsConstant(f));
#endif
  /* From now on, f and g are known not to be constants. */

  topf = cuddI(dd, f->index);
  topg = cuddI(dd, g->index);

  /* Check cache. */
  r = cuddConstantLookup(dd, DD_ADD_EVAL_CONST_TAG, f, g, g);
  if (r != NULL) {
    return (r);
  }

  /* Compute cofactors. */
  if (topf <= topg) {
    Fv = cuddT(f);
    Fnv = cuddE(f);
  } else {
    Fv = Fnv = f;
  }
  if (topg <= topf) {
    Gv = cuddT(g);
    Gnv = cuddE(g);
  } else {
    Gv = Gnv = g;
  }

  /* Recursive step. */
  if (Fv != zero) {
    t = Cudd_addEvalConst(dd, Fv, Gv);
    if (t == DD_NON_CONSTANT || !cuddIsConstant(t)) {
      cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, DD_NON_CONSTANT);
      return (DD_NON_CONSTANT);
    }
    if (Fnv != zero) {
      e = Cudd_addEvalConst(dd, Fnv, Gnv);
      if (e == DD_NON_CONSTANT || !cuddIsConstant(e) || t != e) {
        cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, DD_NON_CONSTANT);
        return (DD_NON_CONSTANT);
      }
    }
    cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, t);
    return (t);
  } else { /* Fnv must be != zero */
    e = Cudd_addEvalConst(dd, Fnv, Gnv);
    cuddCacheInsert2(dd, Cudd_addEvalConst, f, g, e);
    return (e);
  }

} /* end of Cudd_addEvalConst */

/**Function********************************************************************

  Synopsis    [Computes the complement of an ADD a la C language.]

  Description [Computes the complement of an ADD a la C language: The
  complement of 0 is 1 and the complement of everything else is 0.
  Returns a pointer to the resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addNegate]

******************************************************************************/
DdNode *Cudd_addCmpl(DdManager *dd, DdNode *f) {
  DdNode *res;

  do {
    dd->reordered = 0;
    res = cuddAddCmplRecur(dd, f);
  } while (dd->reordered == 1);
  return (res);

} /* end of Cudd_addCmpl */

/**Function********************************************************************

  Synopsis    [Determines whether f is less than or equal to g.]

  Description [Returns 1 if f is less than or equal to g; 0 otherwise.
  No new nodes are created. This procedure works for arbitrary ADDs.
  For 0-1 ADDs Cudd_addEvalConst is more efficient.]

  SideEffects [None]

  SeeAlso     [Cudd_addIteConstant Cudd_addEvalConst Cudd_bddLeq]

******************************************************************************/
int Cudd_addLeq(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *tmp, *fv, *fvn, *gv, *gvn;
  unsigned int topf, topg, res;

  /* Terminal cases. */
  if (f == g)
    return (1);

  statLine(dd);
  if (cuddIsConstant(f)) {
    if (cuddIsConstant(g))
      return (cuddV(f) <= cuddV(g));
    if (f == DD_MINUS_INFINITY(dd))
      return (1);
    if (f == DD_PLUS_INFINITY(dd))
      return (0); /* since f != g */
  }
  if (g == DD_PLUS_INFINITY(dd))
    return (1);
  if (g == DD_MINUS_INFINITY(dd))
    return (0); /* since f != g */

  /* Check cache. */
  tmp = cuddCacheLookup2(dd, (DD_CTFP)Cudd_addLeq, f, g);
  if (tmp != NULL) {
    return (tmp == DD_ONE(dd));
  }

  /* Compute cofactors. One of f and g is not constant. */
  topf = cuddI(dd, f->index);
  topg = cuddI(dd, g->index);
  if (topf <= topg) {
    fv = cuddT(f);
    fvn = cuddE(f);
  } else {
    fv = fvn = f;
  }
  if (topg <= topf) {
    gv = cuddT(g);
    gvn = cuddE(g);
  } else {
    gv = gvn = g;
  }

  res = Cudd_addLeq(dd, fvn, gvn) && Cudd_addLeq(dd, fv, gv);

  /* Store result in cache and return. */
  cuddCacheInsert2(dd, (DD_CTFP)Cudd_addLeq, f, g,
                   Cudd_NotCond(DD_ONE(dd), res == 0));
  return (res);

} /* end of Cudd_addLeq */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_addIte(f,g,h).]

  Description [Implements the recursive step of Cudd_addIte(f,g,h).
  Returns a pointer to the resulting ADD if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addIte]

******************************************************************************/
DdNode *cuddAddIteRecur(DdManager *dd, DdNode *f, DdNode *g, DdNode *h) {
  DdNode *one, *zero;
  DdNode *r, *Fv, *Fnv, *Gv, *Gnv, *Hv, *Hnv, *t, *e;
  unsigned int topf, topg, toph, v;
  int index;

  statLine(dd);
  /* Trivial cases. */

  /* One variable cases. */
  if (f == (one = DD_ONE(dd))) { /* ITE(1,G,H) = G */
    return (g);
  }
  if (f == (zero = DD_ZERO(dd))) { /* ITE(0,G,H) = H */
    return (h);
  }

  /* From now on, f is known to not be a constant. */
  addVarToConst(f, &g, &h, one, zero);

  /* Check remaining one variable cases. */
  if (g == h) { /* ITE(F,G,G) = G */
    return (g);
  }

  if (g == one) { /* ITE(F,1,0) = F */
    if (h == zero)
      return (f);
  }

  topf = cuddI(dd, f->index);
  topg = cuddI(dd, g->index);
  toph = cuddI(dd, h->index);
  v = ddMin(topg, toph);

  /* A shortcut: ITE(F,G,H) = (x,G,H) if F=(x,1,0), x < top(G,H). */
  if (topf < v && cuddT(f) == one && cuddE(f) == zero) {
    r = cuddUniqueInter(dd, (int)f->index, g, h);
    return (r);
  }
  if (topf < v && cuddT(f) == zero && cuddE(f) == one) {
    r = cuddUniqueInter(dd, (int)f->index, h, g);
    return (r);
  }

  /* Check cache. */
  r = cuddCacheLookup(dd, DD_ADD_ITE_TAG, f, g, h);
  if (r != NULL) {
    return (r);
  }

  /* Compute cofactors. */
  if (topf <= v) {
    v = ddMin(topf, v); /* v = top_var(F,G,H) */
    index = f->index;
    Fv = cuddT(f);
    Fnv = cuddE(f);
  } else {
    Fv = Fnv = f;
  }
  if (topg == v) {
    index = g->index;
    Gv = cuddT(g);
    Gnv = cuddE(g);
  } else {
    Gv = Gnv = g;
  }
  if (toph == v) {
    index = h->index;
    Hv = cuddT(h);
    Hnv = cuddE(h);
  } else {
    Hv = Hnv = h;
  }

  /* Recursive step. */
  t = cuddAddIteRecur(dd, Fv, Gv, Hv);
  if (t == NULL)
    return (NULL);
  cuddRef(t);

  e = cuddAddIteRecur(dd, Fnv, Gnv, Hnv);
  if (e == NULL) {
    Cudd_RecursiveDeref(dd, t);
    return (NULL);
  }
  cuddRef(e);

  r = (t == e) ? t : cuddUniqueInter(dd, index, t, e);
  if (r == NULL) {
    Cudd_RecursiveDeref(dd, t);
    Cudd_RecursiveDeref(dd, e);
    return (NULL);
  }
  cuddDeref(t);
  cuddDeref(e);

  cuddCacheInsert(dd, DD_ADD_ITE_TAG, f, g, h, r);

  return (r);

} /* end of cuddAddIteRecur */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_addCmpl.]

  Description [Performs the recursive step of Cudd_addCmpl. Returns a
  pointer to the resulting ADD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_addCmpl]

******************************************************************************/
DdNode *cuddAddCmplRecur(DdManager *dd, DdNode *f) {
  DdNode *one, *zero;
  DdNode *r, *Fv, *Fnv, *t, *e;

  statLine(dd);
  one = DD_ONE(dd);
  zero = DD_ZERO(dd);

  if (cuddIsConstant(f)) {
    if (f == zero) {
      return (one);
    } else {
      return (zero);
    }
  }
  r = cuddCacheLookup1(dd, Cudd_addCmpl, f);
  if (r != NULL) {
    return (r);
  }
  Fv = cuddT(f);
  Fnv = cuddE(f);
  t = cuddAddCmplRecur(dd, Fv);
  if (t == NULL)
    return (NULL);
  cuddRef(t);
  e = cuddAddCmplRecur(dd, Fnv);
  if (e == NULL) {
    Cudd_RecursiveDeref(dd, t);
    return (NULL);
  }
  cuddRef(e);
  r = (t == e) ? t : cuddUniqueInter(dd, (int)f->index, t, e);
  if (r == NULL) {
    Cudd_RecursiveDeref(dd, t);
    Cudd_RecursiveDeref(dd, e);
    return (NULL);
  }
  cuddDeref(t);
  cuddDeref(e);
  cuddCacheInsert1(dd, Cudd_addCmpl, f, r);
  return (r);

} /* end of cuddAddCmplRecur */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Replaces variables with constants if possible (part of
  canonical form).]

  Description []

  SideEffects [None]

******************************************************************************/
static void addVarToConst(DdNode *f, DdNode **gp, DdNode **hp, DdNode *one,
                          DdNode *zero) {
  DdNode *g = *gp;
  DdNode *h = *hp;

  if (f == g) { /* ITE(F,F,H) = ITE(F,1,H) = F + H */
    *gp = one;
  }

  if (f == h) { /* ITE(F,G,F) = ITE(F,G,0) = F * G */
    *hp = zero;
  }

} /* end of addVarToConst */

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

//#ifndef lint
// static char rcsid[] DD_UNUSED = "$Id: cuddAnneal.c,v 1.15 2012/02/05 01:07:18
// fabio Exp $"; #endif

#ifdef DD_STATS
extern int ddTotalNumberSwapping;
extern int ddTotalNISwaps;
static int tosses;
static int acceptances;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int stopping_criterion(int c1, int c2, int c3, int c4, double temp);
static double random_generator(void);
static int ddExchange(DdManager *table, int x, int y, double temp);
static int ddJumpingAux(DdManager *table, int x, int x_low, int x_high,
                        double temp);
static Move *ddJumpingUp(DdManager *table, int x, int x_low, int initial_size);
static Move *ddJumpingDown(DdManager *table, int x, int x_high,
                           int initial_size);
static int siftBackwardProb(DdManager *table, Move *moves, int size,
                            double temp);
static void copyOrder(DdManager *table, int *array, int lower, int upper);
static int restoreOrder(DdManager *table, int *array, int lower, int upper);

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
int cuddAnnealing(DdManager *table, int lower, int upper) {
  int nvars;
  int size;
  int x, y;
  int result;
  int c1, c2, c3, c4;
  int BestCost;
  int *BestOrder;
  double NewTemp, temp;
  double rand1;
  int innerloop, maxGen;
  int ecount, ucount, dcount;

  nvars = upper - lower + 1;

  result = cuddSifting(table, lower, upper);
#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
#endif
  if (result == 0)
    return (0);

  size = table->keys - table->isolated;

  /* Keep track of the best order. */
  BestCost = size;
  BestOrder = ALLOC(int, nvars);
  if (BestOrder == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  copyOrder(table, BestOrder, lower, upper);

  temp = BETA * size;
  maxGen = (int)(MAXGEN_RATIO * nvars);

  c1 = size + 10;
  c2 = c1 + 10;
  c3 = size;
  c4 = c2 + 10;
  ecount = ucount = dcount = 0;

  while (!stopping_criterion(c1, c2, c3, c4, temp)) {
#ifdef DD_STATS
    (void)fprintf(table->out, "temp=%f\tsize=%d\tgen=%d\t", temp, size, maxGen);
    tosses = acceptances = 0;
#endif
    for (innerloop = 0; innerloop < maxGen; innerloop++) {
      /* Choose x, y  randomly. */
      x = (int)Cudd_Random() % nvars;
      do {
        y = (int)Cudd_Random() % nvars;
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
        result = ddExchange(table, x, y, temp); /* exchange */
        ecount++;
#if 0
                (void) fprintf(table->out,
			       "Exchange of %d and %d: size = %d\n",
			       x,y,table->keys - table->isolated);
#endif
      } else if (rand1 < EXC_PROB + JUMP_UP_PROB) {
        result = ddJumpingAux(table, y, x, y, temp); /* jumping_up */
        ucount++;
#if 0
                (void) fprintf(table->out,
			       "Jump up of %d to %d: size = %d\n",
			       y,x,table->keys - table->isolated);
#endif
      } else {
        result = ddJumpingAux(table, x, x, y, temp); /* jumping_down */
        dcount++;
#if 0
                (void) fprintf(table->out,
			       "Jump down of %d to %d: size = %d\n",
			       x,y,table->keys - table->isolated);
#endif
      }

      if (!result) {
        FREE(BestOrder);
        return (0);
      }

      size = table->keys - table->isolated; /* keep current size */
      if (size < BestCost) {                /* update best order */
        BestCost = size;
        copyOrder(table, BestOrder, lower, upper);
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
    temp = NewTemp; /* control variable */
#ifdef DD_STATS
    (void)fprintf(table->out, "uphill = %d\taccepted = %d\n", tosses,
                  acceptances);
    fflush(table->out);
#endif
  }

  result = restoreOrder(table, BestOrder, lower, upper);
  FREE(BestOrder);
  if (!result)
    return (0);
#ifdef DD_STATS
  fprintf(table->out, "#:N_EXCHANGE %8d : total exchanges\n", ecount);
  fprintf(table->out, "#:N_JUMPUP   %8d : total jumps up\n", ucount);
  fprintf(table->out, "#:N_JUMPDOWN %8d : total jumps down", dcount);
#endif
  return (1);

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
static int stopping_criterion(int c1, int c2, int c3, int c4, double temp) {
  if (STOP_TEMP < temp) {
    return (0);
  } else if ((c1 == c2) && (c1 == c3) && (c1 == c4)) {
    return (1);
  } else {
    return (0);
  }

} /* end of stopping_criterion */

/**Function********************************************************************

  Synopsis    [Random number generator.]

  Description [Returns a double precision value between 0.0 and 1.0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static double random_generator(void) {
  return ((double)(Cudd_Random() / 2147483561.0));

} /* end of random_generator */

/**Function********************************************************************

  Synopsis    [This function is for exchanging two variables, x and y.]

  Description [This is the same funcion as ddSwapping except for
  comparison expression.  Use probability function, exp(-size_change/temp).]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int ddExchange(DdManager *table, int x, int y, double temp) {
  Move *move, *moves;
  int tmp;
  int x_ref, y_ref;
  int x_next, y_next;
  int size, result;
  int initial_size, limit_size;

  x_ref = x;
  y_ref = y;

  x_next = cuddNextHigh(table, x);
  y_next = cuddNextLow(table, y);
  moves = NULL;
  initial_size = limit_size = table->keys - table->isolated;

  for (;;) {
    if (x_next == y_next) {
      size = cuddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto ddExchangeOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddExchangeOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;
      size = cuddSwapInPlace(table, y_next, y);
      if (size == 0)
        goto ddExchangeOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddExchangeOutOfMem;
      move->x = y_next;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;
      size = cuddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto ddExchangeOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddExchangeOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;

      tmp = x;
      x = y;
      y = tmp;
    } else if (x == y_next) {
      size = cuddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto ddExchangeOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddExchangeOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;
      tmp = x;
      x = y;
      y = tmp;
    } else {
      size = cuddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto ddExchangeOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddExchangeOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;
      size = cuddSwapInPlace(table, y_next, y);
      if (size == 0)
        goto ddExchangeOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddExchangeOutOfMem;
      move->x = y_next;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;
      x = x_next;
      y = y_next;
    }

    x_next = cuddNextHigh(table, x);
    y_next = cuddNextLow(table, y);
    if (x_next > y_ref)
      break;

    if ((double)size > DD_MAX_REORDER_GROWTH * (double)limit_size) {
      break;
    } else if (size < limit_size) {
      limit_size = size;
    }
  }

  if (y_next >= x_ref) {
    size = cuddSwapInPlace(table, y_next, y);
    if (size == 0)
      goto ddExchangeOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddExchangeOutOfMem;
    move->x = y_next;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;
  }

  /* move backward and stop at best position or accept uphill move */
  result = siftBackwardProb(table, moves, initial_size, temp);
  if (!result)
    goto ddExchangeOutOfMem;

  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (1);

ddExchangeOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (0);

} /* end of ddExchange */

/**Function********************************************************************

  Synopsis    [Moves a variable to a specified position.]

  Description [If x==x_low, it executes jumping_down. If x==x_high, it
  executes jumping_up. This funcion is similar to ddSiftingAux. Returns
  1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int ddJumpingAux(DdManager *table, int x, int x_low, int x_high,
                        double temp) {
  Move *move;
  Move *moves; /* list of moves */
  int initial_size;
  int result;

  initial_size = table->keys - table->isolated;

#ifdef DD_DEBUG
  assert(table->subtables[x].keys > 0);
#endif

  moves = NULL;

  if (cuddNextLow(table, x) < x_low) {
    if (cuddNextHigh(table, x) > x_high)
      return (1);
    moves = ddJumpingDown(table, x, x_high, initial_size);
    /* after that point x --> x_high unless early termination */
    if (moves == NULL)
      goto ddJumpingAuxOutOfMem;
    /* move backward and stop at best position or accept uphill move */
    result = siftBackwardProb(table, moves, initial_size, temp);
    if (!result)
      goto ddJumpingAuxOutOfMem;
  } else if (cuddNextHigh(table, x) > x_high) {
    moves = ddJumpingUp(table, x, x_low, initial_size);
    /* after that point x --> x_low unless early termination */
    if (moves == NULL)
      goto ddJumpingAuxOutOfMem;
    /* move backward and stop at best position or accept uphill move */
    result = siftBackwardProb(table, moves, initial_size, temp);
    if (!result)
      goto ddJumpingAuxOutOfMem;
  } else {
    (void)fprintf(table->err, "Unexpected condition in ddJumping\n");
    goto ddJumpingAuxOutOfMem;
  }
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (1);

ddJumpingAuxOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (0);

} /* end of ddJumpingAux */

/**Function********************************************************************

  Synopsis    [This function is for jumping up.]

  Description [This is a simplified version of ddSiftingUp. It does not
  use lower bounding. Returns the set of moves in case of success; NULL
  if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *ddJumpingUp(DdManager *table, int x, int x_low, int initial_size) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limit_size = initial_size;

  moves = NULL;
  y = cuddNextLow(table, x);
  while (y >= x_low) {
    size = cuddSwapInPlace(table, y, x);
    if (size == 0)
      goto ddJumpingUpOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddJumpingUpOutOfMem;
    move->x = y;
    move->y = x;
    move->size = size;
    move->next = moves;
    moves = move;
    if ((double)size > table->maxGrowth * (double)limit_size) {
      break;
    } else if (size < limit_size) {
      limit_size = size;
    }
    x = y;
    y = cuddNextLow(table, x);
  }
  return (moves);

ddJumpingUpOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (NULL);

} /* end of ddJumpingUp */

/**Function********************************************************************

  Synopsis    [This function is for jumping down.]

  Description [This is a simplified version of ddSiftingDown. It does not
  use lower bounding. Returns the set of moves in case of success; NULL
  if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *ddJumpingDown(DdManager *table, int x, int x_high,
                           int initial_size) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limit_size = initial_size;

  moves = NULL;
  y = cuddNextHigh(table, x);
  while (y <= x_high) {
    size = cuddSwapInPlace(table, x, y);
    if (size == 0)
      goto ddJumpingDownOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddJumpingDownOutOfMem;
    move->x = x;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;
    if ((double)size > table->maxGrowth * (double)limit_size) {
      break;
    } else if (size < limit_size) {
      limit_size = size;
    }
    x = y;
    y = cuddNextHigh(table, x);
  }
  return (moves);

ddJumpingDownOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (NULL);

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
static int siftBackwardProb(DdManager *table, Move *moves, int size,
                            double temp) {
  Move *move;
  int res;
  int best_size = size;
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
    threshold = exp(-((double)(table->keys - table->isolated - size)) / temp);
    if (coin < threshold) {
#ifdef DD_STATS
      acceptances++;
#endif
      return (1);
    }
  }

  /* Either there was improvement, or we have decided not to
  ** accept the uphill move. Go to best position.
  */
  res = table->keys - table->isolated;
  for (move = moves; move != NULL; move = move->next) {
    if (res == best_size)
      return (1);
    res = cuddSwapInPlace(table, (int)move->x, (int)move->y);
    if (!res)
      return (0);
  }

  return (1);

} /* end of sift_backward_prob */

/**Function********************************************************************

  Synopsis    [Copies the current variable order to array.]

  Description [Copies the current variable order to array.
  At the same time inverts the permutation.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void copyOrder(DdManager *table, int *array, int lower, int upper) {
  int i;
  int nvars;

  nvars = upper - lower + 1;
  for (i = 0; i < nvars; i++) {
    array[i] = table->invperm[i + lower];
  }

} /* end of copyOrder */

/**Function********************************************************************

  Synopsis    [Restores the variable order in array by a series of sifts up.]

  Description [Restores the variable order in array by a series of sifts up.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int restoreOrder(DdManager *table, int *array, int lower, int upper) {
  int i, x, y, size;
  int nvars = upper - lower + 1;

  for (i = 0; i < nvars; i++) {
    x = table->perm[array[i]];
#ifdef DD_DEBUG
    assert(x >= lower && x <= upper);
#endif
    y = cuddNextLow(table, x);
    while (y >= i + lower) {
      size = cuddSwapInPlace(table, y, x);
      if (size == 0)
        return (0);
      x = y;
      y = cuddNextLow(table, x);
    }
  }

  return (1);

} /* end of restoreOrder */

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
// static char rcsid[] DD_UNUSED = "$Id: cuddAPI.c,v 1.64 2012/02/05 01:07:18
// fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void fixVarTree(MtrNode *treenode, int *perm, int size);
static int addMultiplicityGroups(DdManager *dd, MtrNode *treenode,
                                 int multiplicity, char *vmask, char *lmask);

/**AutomaticEnd***************************************************************/

/**Function********************************************************************

  Synopsis    [Returns the BDD variable with index i.]

  Description [Retrieves the BDD variable with index i if it already
  exists, or creates a new BDD variable.  Returns a pointer to the
  variable if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddNewVar Cudd_addIthVar Cudd_bddNewVarAtLevel
  Cudd_ReadVars]

******************************************************************************/
DdNode *Cudd_bddIthVar(DdManager *dd, int i) {
  DdNode *res;

  if ((unsigned int)i >= CUDD_MAXINDEX - 1)
    return (NULL);
  if (i < dd->size) {
    res = dd->vars[i];
  } else {
    res = cuddUniqueInter(dd, i, dd->one, Cudd_Not(dd->one));
  }

  return (res);

} /* end of Cudd_bddIthVar */

/**Function********************************************************************

  Synopsis    [Returns the one constant of the manager.]

  Description [Returns the one constant of the manager. The one
  constant is common to ADDs and BDDs.]

  SideEffects [None]

  SeeAlso [Cudd_ReadZero Cudd_ReadLogicZero Cudd_ReadZddOne]

******************************************************************************/
DdNode *Cudd_ReadOne(DdManager *dd) {
  return (dd->one);

} /* end of Cudd_ReadOne */

/**Function********************************************************************

  Synopsis    [Returns the logic zero constant of the manager.]

  Description [Returns the zero constant of the manager. The logic zero
  constant is the complement of the one constant, and is distinct from
  the arithmetic zero.]

  SideEffects [None]

  SeeAlso [Cudd_ReadOne Cudd_ReadZero]

******************************************************************************/
DdNode *Cudd_ReadLogicZero(DdManager *dd) {
  return (Cudd_Not(DD_ONE(dd)));

} /* end of Cudd_ReadLogicZero */

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
void Cudd_SetMinHit(DdManager *dd, unsigned int hr) {
  /* Internally, the package manipulates the ratio of hits to
  ** misses instead of the ratio of hits to accesses. */
  dd->minHit = (double)hr / (100.0 - (double)hr);

} /* end of Cudd_SetMinHit */

/**Function********************************************************************

  Synopsis    [Returns the number of BDD variables in existance.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_ReadZddSize]

******************************************************************************/
int Cudd_ReadSize(DdManager *dd) {
  return (dd->size);

} /* end of Cudd_ReadSize */

/**Function********************************************************************

  Synopsis    [Frees the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetTree Cudd_ReadTree Cudd_FreeZddTree]

******************************************************************************/
void Cudd_FreeTree(DdManager *dd) {
  if (dd->tree != NULL) {
    Mtr_FreeTree(dd->tree);
    dd->tree = NULL;
  }
  return;

} /* end of Cudd_FreeTree */

/**Function********************************************************************

  Synopsis    [Frees the variable group tree of the manager.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_SetZddTree Cudd_ReadZddTree Cudd_FreeTree]

******************************************************************************/
void Cudd_FreeZddTree(DdManager *dd) {
  if (dd->treeZ != NULL) {
    Mtr_FreeTree(dd->treeZ);
    dd->treeZ = NULL;
  }
  return;

} /* end of Cudd_FreeZddTree */

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
int Cudd_ReadPerm(DdManager *dd, int i) {
  if (i == CUDD_CONST_INDEX)
    return (CUDD_CONST_INDEX);
  if (i < 0 || i >= dd->size)
    return (-1);
  return (dd->perm[i]);

} /* end of Cudd_ReadPerm */

/**Function********************************************************************

  Synopsis    [Reads the epsilon parameter of the manager.]

  Description [Reads the epsilon parameter of the manager. The epsilon
  parameter control the comparison between floating point numbers.]

  SideEffects [None]

  SeeAlso     [Cudd_SetEpsilon]

******************************************************************************/
CUDD_VALUE_TYPE
Cudd_ReadEpsilon(DdManager *dd) {
  return (dd->epsilon);

} /* end of Cudd_ReadEpsilon */

/**Function********************************************************************

  Synopsis    [Sets the epsilon parameter of the manager to ep.]

  Description [Sets the epsilon parameter of the manager to ep. The epsilon
  parameter control the comparison between floating point numbers.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadEpsilon]

******************************************************************************/
void Cudd_SetEpsilon(DdManager *dd, CUDD_VALUE_TYPE ep) {
  dd->epsilon = ep;

} /* end of Cudd_SetEpsilon */

/**Function********************************************************************

  Synopsis    [Returns the memory in use by the manager measured in bytes.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
unsigned long Cudd_ReadMemoryInUse(DdManager *dd) {
  return (dd->memused);

} /* end of Cudd_ReadMemoryInUse */

/**Function********************************************************************

  Synopsis    [Reports the number of nodes in BDDs and ADDs.]

  Description [Reports the number of live nodes in BDDs and ADDs. This
  number does not include the isolated projection functions and the
  unused constants. These nodes that are not counted are not part of
  the DDs manipulated by the application.]

  SideEffects [None]

  SeeAlso     [Cudd_ReadPeakNodeCount Cudd_zddReadNodeCount]

******************************************************************************/
long Cudd_ReadNodeCount(DdManager *dd) {
  long count;
  int i;

#ifndef DD_NO_DEATH_ROW
  cuddClearDeathRow(dd);
#endif

  count = (long)(dd->keys - dd->dead);

  /* Count isolated projection functions. Their number is subtracted
  ** from the node count because they are not part of the BDDs.
  */
  for (i = 0; i < dd->size; i++) {
    if (dd->vars[i]->ref == 1)
      count--;
  }
  /* Subtract from the count the unused constants. */
  if (DD_ZERO(dd)->ref == 1)
    count--;
  if (DD_PLUS_INFINITY(dd)->ref == 1)
    count--;
  if (DD_MINUS_INFINITY(dd)->ref == 1)
    count--;

  return (count);

} /* end of Cudd_ReadNodeCount */

/**Function********************************************************************

  Synopsis    [Removes a function from a hook.]

  Description [Removes a function from a hook. A hook is a list of
  application-provided functions called on certain occasions by the
  package. Returns 1 if successful; 0 the function was not in the list.]

  SideEffects [None]

  SeeAlso     [Cudd_AddHook]

******************************************************************************/
int Cudd_RemoveHook(DdManager *dd, DD_HFP f, Cudd_HookType where) {
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
    return (0);
  }
  nextHook = *hook;
  while (nextHook != NULL) {
    if (nextHook->f == f) {
      *hook = nextHook->next;
      FREE(nextHook);
      return (1);
    }
    hook = &(nextHook->next);
    nextHook = nextHook->next;
  }

  return (0);

} /* end of Cudd_RemoveHook */

/**Function********************************************************************

  Synopsis    [Reads a corresponding pair index for a given index.]

  Description [Reads a corresponding pair index for a given index.
  These pair indices are present and next state variable.  Returns the
  corresponding variable index if the variable exists; -1 otherwise.]

  SideEffects [modifies the manager]

  SeeAlso     [Cudd_bddSetPairIndex]

******************************************************************************/
int Cudd_bddReadPairIndex(DdManager *dd, int index) {
  if (index >= dd->size || index < 0)
    return -1;
  return dd->subtables[dd->perm[index]].pairIndex;

} /* end of Cudd_bddReadPairIndex */

/**Function********************************************************************

  Synopsis    [Checks whether a variable is set to be grouped.]

  Description [Checks whether a variable is set to be grouped. This
  function is used for lazy sifting.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
int Cudd_bddIsVarToBeGrouped(DdManager *dd, int index) {
  if (index >= dd->size || index < 0)
    return (-1);
  if (dd->subtables[dd->perm[index]].varToBeGrouped == CUDD_LAZY_UNGROUP)
    return (0);
  else
    return (dd->subtables[dd->perm[index]].varToBeGrouped);

} /* end of Cudd_bddIsVarToBeGrouped */

/**Function********************************************************************

  Synopsis    [Checks whether a variable is set to be ungrouped.]

  Description [Checks whether a variable is set to be ungrouped. This
  function is used for lazy sifting.  Returns 1 if the variable is marked
  to be ungrouped; 0 if the variable exists, but it is not marked to be
  ungrouped; -1 if the variable does not exist.]

  SideEffects [none]

  SeeAlso     [Cudd_bddSetVarToBeUngrouped]

******************************************************************************/
int Cudd_bddIsVarToBeUngrouped(DdManager *dd, int index) {
  if (index >= dd->size || index < 0)
    return (-1);
  return dd->subtables[dd->perm[index]].varToBeGrouped == CUDD_LAZY_UNGROUP;

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
static void fixVarTree(MtrNode *treenode, int *perm, int size) {
  treenode->index = treenode->low;
  treenode->low =
      ((int)treenode->index < size) ? perm[treenode->index] : treenode->index;
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
static int addMultiplicityGroups(
    DdManager *dd /* manager */, MtrNode *treenode /* current tree node */,
    int multiplicity /* how many ZDD vars per BDD var */,
    char *vmask /* variable pairs for which a group has been already built */,
    char *lmask /* levels for which a group has already been built*/) {
  int startV, stopV, startL;
  int i, j;
  MtrNode *auxnode = treenode;

  while (auxnode != NULL) {
    if (auxnode->child != NULL) {
      addMultiplicityGroups(dd, auxnode->child, multiplicity, vmask, lmask);
    }
    /* Build remaining groups. */
    startV = dd->permZ[auxnode->index] / multiplicity;
    startL = auxnode->low / multiplicity;
    stopV = startV + auxnode->size / multiplicity;
    /* Walk down vmask starting at startV and build missing groups. */
    for (i = startV, j = startL; i < stopV; i++) {
      if (vmask[i] == 0) {
        MtrNode *node;
        while (lmask[j] == 1)
          j++;
        node =
            Mtr_MakeGroup(auxnode, j * multiplicity, multiplicity, MTR_FIXED);
        if (node == NULL) {
          return (0);
        }
        node->index = dd->invpermZ[i * multiplicity];
        vmask[i] = 1;
        lmask[j] = 1;
      }
    }
    auxnode = auxnode->younger;
  }
  return (1);

} /* end of addMultiplicityGroups */

/**CFile***********************************************************************

  FileName    [cuddBddAbs.c]

  PackageName [cudd]

  Synopsis    [Quantification functions for BDDs.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_bddExistAbstract()
                <li> Cudd_bddExistAbstractLimit()
                <li> Cudd_bddXorExistAbstract()
                <li> Cudd_bddUnivAbstract()
                <li> Cudd_bddBooleanDiff()
                <li> Cudd_bddVarIsDependent()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddBddExistAbstractRecur()
                <li> cuddBddXorExistAbstractRecur()
                <li> cuddBddBooleanDiffRecur()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> bddCheckPositiveCube()
                </ul>
                ]

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
// static char rcsid[] DD_UNUSED = "$Id: cuddBddAbs.c,v 1.28 2012/02/05 01:07:18
// fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int bddCheckPositiveCube(DdManager *manager, DdNode *cube);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Existentially abstracts all the variables in cube from f.]

  Description [Existentially abstracts all the variables in cube from f.
  Returns the abstracted BDD if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddUnivAbstract Cudd_addExistAbstract]

******************************************************************************/
DdNode *Cudd_bddExistAbstract(DdManager *manager, DdNode *f, DdNode *cube) {
  DdNode *res;

  if (bddCheckPositiveCube(manager, cube) == 0) {
    (void)fprintf(manager->err, "Error: Can only abstract positive cubes\n");
    manager->errorCode = CUDD_INVALID_ARG;
    return (NULL);
  }

  do {
    manager->reordered = 0;
    res = cuddBddExistAbstractRecur(manager, f, cube);
  } while (manager->reordered == 1);

  return (res);

} /* end of Cudd_bddExistAbstract */

/**Function********************************************************************

  Synopsis    [Checks whether a variable is dependent on others in a
  function.]

  Description [Checks whether a variable is dependent on others in a
  function.  Returns 1 if the variable is dependent; 0 otherwise. No
  new nodes are created.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int Cudd_bddVarIsDependent(DdManager *dd, /* manager */
                           DdNode *f,     /* function */
                           DdNode *var /* variable */) {
  DdNode *F, *res, *zero, *ft, *fe;
  unsigned topf, level;
  DD_CTFP cacheOp;
  int retval;

  zero = Cudd_Not(DD_ONE(dd));
  if (Cudd_IsConstant(f))
    return (f == zero);

  /* From now on f is not constant. */
  F = Cudd_Regular(f);
  topf = (unsigned)dd->perm[F->index];
  level = (unsigned)dd->perm[var->index];

  /* Check terminal case. If topf > index of var, f does not depend on var.
  ** Therefore, var is not dependent in f. */
  if (topf > level) {
    return (0);
  }

  cacheOp = (DD_CTFP)Cudd_bddVarIsDependent;
  res = cuddCacheLookup2(dd, cacheOp, f, var);
  if (res != NULL) {
    return (res != zero);
  }

  /* Compute cofactors. */
  ft = Cudd_NotCond(cuddT(F), f != F);
  fe = Cudd_NotCond(cuddE(F), f != F);

  if (topf == level) {
    retval = Cudd_bddLeq(dd, ft, Cudd_Not(fe));
  } else {
    retval = Cudd_bddVarIsDependent(dd, ft, var) &&
             Cudd_bddVarIsDependent(dd, fe, var);
  }

  cuddCacheInsert2(dd, cacheOp, f, var, Cudd_NotCond(zero, retval));

  return (retval);

} /* Cudd_bddVarIsDependent */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Performs the recursive steps of Cudd_bddExistAbstract.]

  Description [Performs the recursive steps of Cudd_bddExistAbstract.
  Returns the BDD obtained by abstracting the variables
  of cube from f if successful; NULL otherwise. It is also used by
  Cudd_bddUnivAbstract.]

  SideEffects [None]

  SeeAlso     [Cudd_bddExistAbstract Cudd_bddUnivAbstract]

******************************************************************************/
DdNode *cuddBddExistAbstractRecur(DdManager *manager, DdNode *f, DdNode *cube) {
  DdNode *F, *T, *E, *res, *res1, *res2, *one;

  statLine(manager);
  one = DD_ONE(manager);
  F = Cudd_Regular(f);

  /* Cube is guaranteed to be a cube at this point. */
  if (cube == one || F == one) {
    return (f);
  }
  /* From now on, f and cube are non-constant. */

  /* Abstract a variable that does not appear in f. */
  while (manager->perm[F->index] > manager->perm[cube->index]) {
    cube = cuddT(cube);
    if (cube == one)
      return (f);
  }

  /* Check the cache. */
  if (F->ref != 1 && (res = cuddCacheLookup2(manager, Cudd_bddExistAbstract, f,
                                             cube)) != NULL) {
    return (res);
  }

  /* Compute the cofactors of f. */
  T = cuddT(F);
  E = cuddE(F);
  if (f != F) {
    T = Cudd_Not(T);
    E = Cudd_Not(E);
  }

  /* If the two indices are the same, so are their levels. */
  if (F->index == cube->index) {
    if (T == one || E == one || T == Cudd_Not(E)) {
      return (one);
    }
    res1 = cuddBddExistAbstractRecur(manager, T, cuddT(cube));
    if (res1 == NULL)
      return (NULL);
    if (res1 == one) {
      if (F->ref != 1)
        cuddCacheInsert2(manager, Cudd_bddExistAbstract, f, cube, one);
      return (one);
    }
    cuddRef(res1);
    res2 = cuddBddExistAbstractRecur(manager, E, cuddT(cube));
    if (res2 == NULL) {
      Cudd_IterDerefBdd(manager, res1);
      return (NULL);
    }
    cuddRef(res2);
    res = cuddBddAndRecur(manager, Cudd_Not(res1), Cudd_Not(res2));
    if (res == NULL) {
      Cudd_IterDerefBdd(manager, res1);
      Cudd_IterDerefBdd(manager, res2);
      return (NULL);
    }
    res = Cudd_Not(res);
    cuddRef(res);
    Cudd_IterDerefBdd(manager, res1);
    Cudd_IterDerefBdd(manager, res2);
    if (F->ref != 1)
      cuddCacheInsert2(manager, Cudd_bddExistAbstract, f, cube, res);
    cuddDeref(res);
    return (res);
  } else { /* if (cuddI(manager,F->index) < cuddI(manager,cube->index)) */
    res1 = cuddBddExistAbstractRecur(manager, T, cube);
    if (res1 == NULL)
      return (NULL);
    cuddRef(res1);
    res2 = cuddBddExistAbstractRecur(manager, E, cube);
    if (res2 == NULL) {
      Cudd_IterDerefBdd(manager, res1);
      return (NULL);
    }
    cuddRef(res2);
    /* ITE takes care of possible complementation of res1 and of the
     ** case in which res1 == res2. */
    res = cuddBddIteRecur(manager, manager->vars[F->index], res1, res2);
    if (res == NULL) {
      Cudd_IterDerefBdd(manager, res1);
      Cudd_IterDerefBdd(manager, res2);
      return (NULL);
    }
    cuddDeref(res1);
    cuddDeref(res2);
    if (F->ref != 1)
      cuddCacheInsert2(manager, Cudd_bddExistAbstract, f, cube, res);
    return (res);
  }

} /* end of cuddBddExistAbstractRecur */

/**Function********************************************************************

  Synopsis [Takes the exclusive OR of two BDDs and simultaneously abstracts the
  variables in cube.]

  Description [Takes the exclusive OR of two BDDs and simultaneously abstracts
  the variables in cube. The variables are existentially abstracted.  Returns a
  pointer to the result is successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddAndAbstract]

******************************************************************************/
DdNode *cuddBddXorExistAbstractRecur(DdManager *manager, DdNode *f, DdNode *g,
                                     DdNode *cube) {
  DdNode *F, *fv, *fnv, *G, *gv, *gnv;
  DdNode *one, *zero, *r, *t, *e, *Cube;
  unsigned int topf, topg, topcube, top, index;

  statLine(manager);
  one = DD_ONE(manager);
  zero = Cudd_Not(one);

  /* Terminal cases. */
  if (f == g) {
    return (zero);
  }
  if (f == Cudd_Not(g)) {
    return (one);
  }
  if (cube == one) {
    return (cuddBddXorRecur(manager, f, g));
  }
  if (f == one) {
    return (cuddBddExistAbstractRecur(manager, Cudd_Not(g), cube));
  }
  if (g == one) {
    return (cuddBddExistAbstractRecur(manager, Cudd_Not(f), cube));
  }
  if (f == zero) {
    return (cuddBddExistAbstractRecur(manager, g, cube));
  }
  if (g == zero) {
    return (cuddBddExistAbstractRecur(manager, f, cube));
  }

  /* At this point f, g, and cube are not constant. */

  if (f > g) { /* Try to increase cache efficiency. */
    DdNode *tmp = f;
    f = g;
    g = tmp;
  }

  /* Check cache. */
  r = cuddCacheLookup(manager, DD_BDD_XOR_EXIST_ABSTRACT_TAG, f, g, cube);
  if (r != NULL) {
    return (r);
  }

  /* Here we can skip the use of cuddI, because the operands are known
  ** to be non-constant.
  */
  F = Cudd_Regular(f);
  topf = manager->perm[F->index];
  G = Cudd_Regular(g);
  topg = manager->perm[G->index];
  top = ddMin(topf, topg);
  topcube = manager->perm[cube->index];

  if (topcube < top) {
    return (cuddBddXorExistAbstractRecur(manager, f, g, cuddT(cube)));
  }
  /* Now, topcube >= top. */

  if (topf == top) {
    index = F->index;
    fv = cuddT(F);
    fnv = cuddE(F);
    if (Cudd_IsComplement(f)) {
      fv = Cudd_Not(fv);
      fnv = Cudd_Not(fnv);
    }
  } else {
    index = G->index;
    fv = fnv = f;
  }

  if (topg == top) {
    gv = cuddT(G);
    gnv = cuddE(G);
    if (Cudd_IsComplement(g)) {
      gv = Cudd_Not(gv);
      gnv = Cudd_Not(gnv);
    }
  } else {
    gv = gnv = g;
  }

  if (topcube == top) {
    Cube = cuddT(cube);
  } else {
    Cube = cube;
  }

  t = cuddBddXorExistAbstractRecur(manager, fv, gv, Cube);
  if (t == NULL)
    return (NULL);

  /* Special case: 1 OR anything = 1. Hence, no need to compute
  ** the else branch if t is 1.
  */
  if (t == one && topcube == top) {
    cuddCacheInsert(manager, DD_BDD_XOR_EXIST_ABSTRACT_TAG, f, g, cube, one);
    return (one);
  }
  cuddRef(t);

  e = cuddBddXorExistAbstractRecur(manager, fnv, gnv, Cube);
  if (e == NULL) {
    Cudd_IterDerefBdd(manager, t);
    return (NULL);
  }
  cuddRef(e);

  if (topcube == top) { /* abstract */
    r = cuddBddAndRecur(manager, Cudd_Not(t), Cudd_Not(e));
    if (r == NULL) {
      Cudd_IterDerefBdd(manager, t);
      Cudd_IterDerefBdd(manager, e);
      return (NULL);
    }
    r = Cudd_Not(r);
    cuddRef(r);
    Cudd_IterDerefBdd(manager, t);
    Cudd_IterDerefBdd(manager, e);
    cuddDeref(r);
  } else if (t == e) {
    r = t;
    cuddDeref(t);
    cuddDeref(e);
  } else {
    if (Cudd_IsComplement(t)) {
      r = cuddUniqueInter(manager, (int)index, Cudd_Not(t), Cudd_Not(e));
      if (r == NULL) {
        Cudd_IterDerefBdd(manager, t);
        Cudd_IterDerefBdd(manager, e);
        return (NULL);
      }
      r = Cudd_Not(r);
    } else {
      r = cuddUniqueInter(manager, (int)index, t, e);
      if (r == NULL) {
        Cudd_IterDerefBdd(manager, t);
        Cudd_IterDerefBdd(manager, e);
        return (NULL);
      }
    }
    cuddDeref(e);
    cuddDeref(t);
  }
  cuddCacheInsert(manager, DD_BDD_XOR_EXIST_ABSTRACT_TAG, f, g, cube, r);
  return (r);

} /* end of cuddBddXorExistAbstractRecur */

/**Function********************************************************************

  Synopsis    [Performs the recursive steps of Cudd_bddBoleanDiff.]

  Description [Performs the recursive steps of Cudd_bddBoleanDiff.
  Returns the BDD obtained by XORing the cofactors of f with respect to
  var if successful; NULL otherwise. Exploits the fact that dF/dx =
  dF'/dx.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddBddBooleanDiffRecur(DdManager *manager, DdNode *f, DdNode *var) {
  DdNode *T, *E, *res, *res1, *res2;

  statLine(manager);
  if (cuddI(manager, f->index) > manager->perm[var->index]) {
    /* f does not depend on var. */
    return (Cudd_Not(DD_ONE(manager)));
  }

  /* From now on, f is non-constant. */

  /* If the two indices are the same, so are their levels. */
  if (f->index == var->index) {
    res = cuddBddXorRecur(manager, cuddT(f), cuddE(f));
    return (res);
  }

  /* From now on, cuddI(manager,f->index) < cuddI(manager,cube->index). */

  /* Check the cache. */
  res = cuddCacheLookup2(manager, cuddBddBooleanDiffRecur, f, var);
  if (res != NULL) {
    return (res);
  }

  /* Compute the cofactors of f. */
  T = cuddT(f);
  E = cuddE(f);

  res1 = cuddBddBooleanDiffRecur(manager, T, var);
  if (res1 == NULL)
    return (NULL);
  cuddRef(res1);
  res2 = cuddBddBooleanDiffRecur(manager, Cudd_Regular(E), var);
  if (res2 == NULL) {
    Cudd_IterDerefBdd(manager, res1);
    return (NULL);
  }
  cuddRef(res2);
  /* ITE takes care of possible complementation of res1 and of the
  ** case in which res1 == res2. */
  res = cuddBddIteRecur(manager, manager->vars[f->index], res1, res2);
  if (res == NULL) {
    Cudd_IterDerefBdd(manager, res1);
    Cudd_IterDerefBdd(manager, res2);
    return (NULL);
  }
  cuddDeref(res1);
  cuddDeref(res2);
  cuddCacheInsert2(manager, cuddBddBooleanDiffRecur, f, var, res);
  return (res);

} /* end of cuddBddBooleanDiffRecur */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Checks whether cube is an BDD representing the product of
  positive literals.]

  Description [Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int bddCheckPositiveCube(DdManager *manager, DdNode *cube) {
  if (Cudd_IsComplement(cube))
    return (0);
  if (cube == DD_ONE(manager))
    return (1);
  if (cuddIsConstant(cube))
    return (0);
  if (cuddE(cube) == Cudd_Not(DD_ONE(manager))) {
    return (bddCheckPositiveCube(manager, cuddT(cube)));
  }
  return (0);

} /* end of bddCheckPositiveCube */

/**CFile***********************************************************************

  FileName    [cuddBddIte.c]

  PackageName [cudd]

  Synopsis    [BDD ITE function and satellites.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_bddIte()
                <li> Cudd_bddIteLimit()
                <li> Cudd_bddIteConstant()
                <li> Cudd_bddIntersect()
                <li> Cudd_bddAnd()
                <li> Cudd_bddAndLimit()
                <li> Cudd_bddOr()
                <li> Cudd_bddOrLimit()
                <li> Cudd_bddNand()
                <li> Cudd_bddNor()
                <li> Cudd_bddXor()
                <li> Cudd_bddXnor()
                <li> Cudd_bddXnorLimit()
                <li> Cudd_bddLeq()
                </ul>
       Internal procedures included in this module:
                <ul>
                <li> cuddBddIteRecur()
                <li> cuddBddIntersectRecur()
                <li> cuddBddAndRecur()
                <li> cuddBddXorRecur()
                </ul>
       Static procedures included in this module:
                <ul>
                <li> bddVarToConst()
                <li> bddVarToCanonical()
                <li> bddVarToCanonicalSimple()
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
// static char rcsid[] DD_UNUSED = "$Id: cuddBddIte.c,v 1.26 2012/02/05 01:07:18
// fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void bddVarToConst(DdNode *f, DdNode **gp, DdNode **hp, DdNode *one);
static int bddVarToCanonical(DdManager *dd, DdNode **fp, DdNode **gp,
                             DdNode **hp, unsigned int *topfp,
                             unsigned int *topgp, unsigned int *tophp);
static int bddVarToCanonicalSimple(DdManager *dd, DdNode **fp, DdNode **gp,
                                   DdNode **hp, unsigned int *topfp,
                                   unsigned int *topgp, unsigned int *tophp);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Implements ITE(f,g,h).]

  Description [Implements ITE(f,g,h). Returns a pointer to the
  resulting BDD if successful; NULL if the intermediate result blows
  up.]

  SideEffects [None]

  SeeAlso     [Cudd_addIte Cudd_bddIteConstant Cudd_bddIntersect]

******************************************************************************/
DdNode *Cudd_bddIte(DdManager *dd, DdNode *f, DdNode *g, DdNode *h) {
  DdNode *res;

  do {
    dd->reordered = 0;
    res = cuddBddIteRecur(dd, f, g, h);
  } while (dd->reordered == 1);
  return (res);

} /* end of Cudd_bddIte */

/**Function********************************************************************

  Synopsis    [Implements ITEconstant(f,g,h).]

  Description [Implements ITEconstant(f,g,h). Returns a pointer to the
  resulting BDD (which may or may not be constant) or DD_NON_CONSTANT.
  No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_bddIntersect Cudd_bddLeq Cudd_addIteConstant]

******************************************************************************/
DdNode *Cudd_bddIteConstant(DdManager *dd, DdNode *f, DdNode *g, DdNode *h) {
  DdNode *r, *Fv, *Fnv, *Gv, *Gnv, *H, *Hv, *Hnv, *t, *e;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = Cudd_Not(one);
  int comple;
  unsigned int topf, topg, toph, v;

  statLine(dd);
  /* Trivial cases. */
  if (f == one) /* ITE(1,G,H) => G */
    return (g);

  if (f == zero) /* ITE(0,G,H) => H */
    return (h);

  /* f now not a constant. */
  bddVarToConst(f, &g, &h, one); /* possibly convert g or h */
  /* to constants */

  if (g == h) /* ITE(F,G,G) => G */
    return (g);

  if (Cudd_IsConstant(g) && Cudd_IsConstant(h))
    return (DD_NON_CONSTANT); /* ITE(F,1,0) or ITE(F,0,1) */
  /* => DD_NON_CONSTANT */

  if (g == Cudd_Not(h))
    return (DD_NON_CONSTANT); /* ITE(F,G,G') => DD_NON_CONSTANT */
  /* if F != G and F != G' */

  comple = bddVarToCanonical(dd, &f, &g, &h, &topf, &topg, &toph);

  /* Cache lookup. */
  r = cuddConstantLookup(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h);
  if (r != NULL) {
    return (Cudd_NotCond(r, comple && r != DD_NON_CONSTANT));
  }

  v = ddMin(topg, toph);

  /* ITE(F,G,H) = (v,G,H) (non constant) if F = (v,1,0), v < top(G,H). */
  if (topf < v && cuddT(f) == one && cuddE(f) == zero) {
    return (DD_NON_CONSTANT);
  }

  /* Compute cofactors. */
  if (topf <= v) {
    v = ddMin(topf, v); /* v = top_var(F,G,H) */
    Fv = cuddT(f);
    Fnv = cuddE(f);
  } else {
    Fv = Fnv = f;
  }

  if (topg == v) {
    Gv = cuddT(g);
    Gnv = cuddE(g);
  } else {
    Gv = Gnv = g;
  }

  if (toph == v) {
    H = Cudd_Regular(h);
    Hv = cuddT(H);
    Hnv = cuddE(H);
    if (Cudd_IsComplement(h)) {
      Hv = Cudd_Not(Hv);
      Hnv = Cudd_Not(Hnv);
    }
  } else {
    Hv = Hnv = h;
  }

  /* Recursion. */
  t = Cudd_bddIteConstant(dd, Fv, Gv, Hv);
  if (t == DD_NON_CONSTANT || !Cudd_IsConstant(t)) {
    cuddCacheInsert(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
    return (DD_NON_CONSTANT);
  }
  e = Cudd_bddIteConstant(dd, Fnv, Gnv, Hnv);
  if (e == DD_NON_CONSTANT || !Cudd_IsConstant(e) || t != e) {
    cuddCacheInsert(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h, DD_NON_CONSTANT);
    return (DD_NON_CONSTANT);
  }
  cuddCacheInsert(dd, DD_BDD_ITE_CONSTANT_TAG, f, g, h, t);
  return (Cudd_NotCond(t, comple));

} /* end of Cudd_bddIteConstant */

/**Function********************************************************************

  Synopsis    [Returns a function included in the intersection of f and g.]

  Description [Computes a function included in the intersection of f and
  g. (That is, a witness that the intersection is not empty.)
  Cudd_bddIntersect tries to build as few new nodes as possible. If the
  only result of interest is whether f and g intersect,
  Cudd_bddLeq should be used instead.]

  SideEffects [None]

  SeeAlso     [Cudd_bddLeq Cudd_bddIteConstant]

******************************************************************************/
DdNode *Cudd_bddIntersect(DdManager *dd /* manager */,
                          DdNode *f /* first operand */,
                          DdNode *g /* second operand */) {
  DdNode *res;

  do {
    dd->reordered = 0;
    res = cuddBddIntersectRecur(dd, f, g);
  } while (dd->reordered == 1);

  return (res);

} /* end of Cudd_bddIntersect */

/**Function********************************************************************

  Synopsis    [Computes the conjunction of two BDDs f and g.]

  Description [Computes the conjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAndAbstract Cudd_bddIntersect
  Cudd_bddOr Cudd_bddNand Cudd_bddNor Cudd_bddXor Cudd_bddXnor]

******************************************************************************/
DdNode *Cudd_bddAnd(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *res;

  do {
    dd->reordered = 0;
    res = cuddBddAndRecur(dd, f, g);
  } while (dd->reordered == 1);
  return (res);

} /* end of Cudd_bddAnd */

/**Function********************************************************************

  Synopsis    [Computes the conjunction of two BDDs f and g.  Returns
  NULL if too many nodes are required.]

  Description [Computes the conjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up or more new nodes than <code>limit</code> are
  required.]

  SideEffects [None]

  SeeAlso     [Cudd_bddAnd]

******************************************************************************/
DdNode *Cudd_bddAndLimit(DdManager *dd, DdNode *f, DdNode *g,
                         unsigned int limit) {
  DdNode *res;
  unsigned int saveLimit = dd->maxLive;

  dd->maxLive = (dd->keys - dd->dead) + (dd->keysZ - dd->deadZ) + limit;
  do {
    dd->reordered = 0;
    res = cuddBddAndRecur(dd, f, g);
  } while (dd->reordered == 1);
  dd->maxLive = saveLimit;
  return (res);

} /* end of Cudd_bddAndLimit */

/**Function********************************************************************

  Synopsis    [Computes the disjunction of two BDDs f and g.]

  Description [Computes the disjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddNand Cudd_bddNor
  Cudd_bddXor Cudd_bddXnor]

******************************************************************************/
DdNode *Cudd_bddOr(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *res;

  do {
    dd->reordered = 0;
    res = cuddBddAndRecur(dd, Cudd_Not(f), Cudd_Not(g));
  } while (dd->reordered == 1);
  res = Cudd_NotCond(res, res != NULL);
  return (res);

} /* end of Cudd_bddOr */

/**Function********************************************************************

  Synopsis    [Computes the disjunction of two BDDs f and g.  Returns
  NULL if too many nodes are required.]

  Description [Computes the disjunction of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up or more new nodes than <code>limit</code> are
  required.]

  SideEffects [None]

  SeeAlso     [Cudd_bddOr]

******************************************************************************/
DdNode *Cudd_bddOrLimit(DdManager *dd, DdNode *f, DdNode *g,
                        unsigned int limit) {
  DdNode *res;
  unsigned int saveLimit = dd->maxLive;

  dd->maxLive = (dd->keys - dd->dead) + (dd->keysZ - dd->deadZ) + limit;
  do {
    dd->reordered = 0;
    res = cuddBddAndRecur(dd, Cudd_Not(f), Cudd_Not(g));
  } while (dd->reordered == 1);
  dd->maxLive = saveLimit;
  res = Cudd_NotCond(res, res != NULL);
  return (res);

} /* end of Cudd_bddOrLimit */

/* end of Cudd_bddNand */

/**Function********************************************************************

  Synopsis    [Computes the exclusive OR of two BDDs f and g.]

  Description [Computes the exclusive OR of two BDDs f and g. Returns a
  pointer to the resulting BDD if successful; NULL if the intermediate
  result blows up.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIte Cudd_addApply Cudd_bddAnd Cudd_bddOr
  Cudd_bddNand Cudd_bddNor Cudd_bddXnor]

******************************************************************************/
DdNode *Cudd_bddXor(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *res;

  do {
    dd->reordered = 0;
    res = cuddBddXorRecur(dd, f, g);
  } while (dd->reordered == 1);
  return (res);

} /* end of Cudd_bddXor */

/**Function********************************************************************

  Synopsis    [Determines whether f is less than or equal to g.]

  Description [Returns 1 if f is less than or equal to g; 0 otherwise.
  No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_bddIteConstant Cudd_addEvalConst]

******************************************************************************/
int Cudd_bddLeq(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *one, *zero, *tmp, *F, *fv, *fvn, *gv, *gvn;
  unsigned int topf, topg, res;

  statLine(dd);
  /* Terminal cases and normalization. */
  if (f == g)
    return (1);

  if (Cudd_IsComplement(g)) {
    /* Special case: if f is regular and g is complemented,
    ** f(1,...,1) = 1 > 0 = g(1,...,1).
    */
    if (!Cudd_IsComplement(f))
      return (0);
    /* Both are complemented: Swap and complement because
    ** f <= g <=> g' <= f' and we want the second argument to be regular.
    */
    tmp = g;
    g = Cudd_Not(f);
    f = Cudd_Not(tmp);
  } else if (Cudd_IsComplement(f) && g < f) {
    tmp = g;
    g = Cudd_Not(f);
    f = Cudd_Not(tmp);
  }

  /* Now g is regular and, if f is not regular, f < g. */
  one = DD_ONE(dd);
  if (g == one)
    return (1); /* no need to test against zero */
  if (f == one)
    return (0); /* since at this point g != one */
  if (Cudd_Not(f) == g)
    return (0); /* because neither is constant */
  zero = Cudd_Not(one);
  if (f == zero)
    return (1);

  /* Here neither f nor g is constant. */

  /* Check cache. */
  tmp = cuddCacheLookup2(dd, (DD_CTFP)Cudd_bddLeq, f, g);
  if (tmp != NULL) {
    return (tmp == one);
  }

  /* Compute cofactors. */
  F = Cudd_Regular(f);
  topf = dd->perm[F->index];
  topg = dd->perm[g->index];
  if (topf <= topg) {
    fv = cuddT(F);
    fvn = cuddE(F);
    if (f != F) {
      fv = Cudd_Not(fv);
      fvn = Cudd_Not(fvn);
    }
  } else {
    fv = fvn = f;
  }
  if (topg <= topf) {
    gv = cuddT(g);
    gvn = cuddE(g);
  } else {
    gv = gvn = g;
  }

  /* Recursive calls. Since we want to maximize the probability of
  ** the special case f(1,...,1) > g(1,...,1), we consider the negative
  ** cofactors first. Indeed, the complementation parity of the positive
  ** cofactors is the same as the one of the parent functions.
  */
  res = Cudd_bddLeq(dd, fvn, gvn) && Cudd_bddLeq(dd, fv, gv);

  /* Store result in cache and return. */
  cuddCacheInsert2(dd, (DD_CTFP)Cudd_bddLeq, f, g, (res ? one : zero));
  return (res);

} /* end of Cudd_bddLeq */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_bddIte.]

  Description [Implements the recursive step of Cudd_bddIte. Returns a
  pointer to the resulting BDD. NULL if the intermediate result blows
  up or if reordering occurs.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddBddIteRecur(DdManager *dd, DdNode *f, DdNode *g, DdNode *h) {
  DdNode *one, *zero, *res;
  DdNode *r, *Fv, *Fnv, *Gv, *Gnv, *H, *Hv, *Hnv, *t, *e;
  unsigned int topf, topg, toph, v;
  int index;
  int comple;

  statLine(dd);
  /* Terminal cases. */

  /* One variable cases. */
  if (f == (one = DD_ONE(dd))) /* ITE(1,G,H) = G */
    return (g);

  if (f == (zero = Cudd_Not(one))) /* ITE(0,G,H) = H */
    return (h);

  /* From now on, f is known not to be a constant. */
  if (g == one || f == g) { /* ITE(F,F,H) = ITE(F,1,H) = F + H */
    if (h == zero) {        /* ITE(F,1,0) = F */
      return (f);
    } else {
      res = cuddBddAndRecur(dd, Cudd_Not(f), Cudd_Not(h));
      return (Cudd_NotCond(res, res != NULL));
    }
  } else if (g == zero ||
             f == Cudd_Not(g)) { /* ITE(F,!F,H) = ITE(F,0,H) = !F * H */
    if (h == one) {              /* ITE(F,0,1) = !F */
      return (Cudd_Not(f));
    } else {
      res = cuddBddAndRecur(dd, Cudd_Not(f), h);
      return (res);
    }
  }
  if (h == zero || f == h) { /* ITE(F,G,F) = ITE(F,G,0) = F * G */
    res = cuddBddAndRecur(dd, f, g);
    return (res);
  } else if (h == one ||
             f == Cudd_Not(h)) { /* ITE(F,G,!F) = ITE(F,G,1) = !F + G */
    res = cuddBddAndRecur(dd, f, Cudd_Not(g));
    return (Cudd_NotCond(res, res != NULL));
  }

  /* Check remaining one variable case. */
  if (g == h) { /* ITE(F,G,G) = G */
    return (g);
  } else if (g == Cudd_Not(h)) { /* ITE(F,G,!G) = F <-> G */
    res = cuddBddXorRecur(dd, f, h);
    return (res);
  }

  /* From here, there are no constants. */
  comple = bddVarToCanonicalSimple(dd, &f, &g, &h, &topf, &topg, &toph);

  /* f & g are now regular pointers */

  v = ddMin(topg, toph);

  /* A shortcut: ITE(F,G,H) = (v,G,H) if F = (v,1,0), v < top(G,H). */
  if (topf < v && cuddT(f) == one && cuddE(f) == zero) {
    r = cuddUniqueInter(dd, (int)f->index, g, h);
    return (Cudd_NotCond(r, comple && r != NULL));
  }

  /* Check cache. */
  r = cuddCacheLookup(dd, DD_BDD_ITE_TAG, f, g, h);
  if (r != NULL) {
    return (Cudd_NotCond(r, comple));
  }

  /* Compute cofactors. */
  if (topf <= v) {
    v = ddMin(topf, v); /* v = top_var(F,G,H) */
    index = f->index;
    Fv = cuddT(f);
    Fnv = cuddE(f);
  } else {
    Fv = Fnv = f;
  }
  if (topg == v) {
    index = g->index;
    Gv = cuddT(g);
    Gnv = cuddE(g);
  } else {
    Gv = Gnv = g;
  }
  if (toph == v) {
    H = Cudd_Regular(h);
    index = H->index;
    Hv = cuddT(H);
    Hnv = cuddE(H);
    if (Cudd_IsComplement(h)) {
      Hv = Cudd_Not(Hv);
      Hnv = Cudd_Not(Hnv);
    }
  } else {
    Hv = Hnv = h;
  }

  /* Recursive step. */
  t = cuddBddIteRecur(dd, Fv, Gv, Hv);
  if (t == NULL)
    return (NULL);
  cuddRef(t);

  e = cuddBddIteRecur(dd, Fnv, Gnv, Hnv);
  if (e == NULL) {
    Cudd_IterDerefBdd(dd, t);
    return (NULL);
  }
  cuddRef(e);

  r = (t == e) ? t : cuddUniqueInter(dd, index, t, e);
  if (r == NULL) {
    Cudd_IterDerefBdd(dd, t);
    Cudd_IterDerefBdd(dd, e);
    return (NULL);
  }
  cuddDeref(t);
  cuddDeref(e);

  cuddCacheInsert(dd, DD_BDD_ITE_TAG, f, g, h, r);
  return (Cudd_NotCond(r, comple));

} /* end of cuddBddIteRecur */

/**Function********************************************************************

  Synopsis    [Implements the recursive step of Cudd_bddIntersect.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_bddIntersect]

******************************************************************************/
DdNode *cuddBddIntersectRecur(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *res;
  DdNode *F, *G, *t, *e;
  DdNode *fv, *fnv, *gv, *gnv;
  DdNode *one, *zero;
  unsigned int index, topf, topg;

  statLine(dd);
  one = DD_ONE(dd);
  zero = Cudd_Not(one);

  /* Terminal cases. */
  if (f == zero || g == zero || f == Cudd_Not(g))
    return (zero);
  if (f == g || g == one)
    return (f);
  if (f == one)
    return (g);

  /* At this point f and g are not constant. */
  if (f > g) {
    DdNode *tmp = f;
    f = g;
    g = tmp;
  }
  res = cuddCacheLookup2(dd, Cudd_bddIntersect, f, g);
  if (res != NULL)
    return (res);

  /* Find splitting variable. Here we can skip the use of cuddI,
  ** because the operands are known to be non-constant.
  */
  F = Cudd_Regular(f);
  topf = dd->perm[F->index];
  G = Cudd_Regular(g);
  topg = dd->perm[G->index];

  /* Compute cofactors. */
  if (topf <= topg) {
    index = F->index;
    fv = cuddT(F);
    fnv = cuddE(F);
    if (Cudd_IsComplement(f)) {
      fv = Cudd_Not(fv);
      fnv = Cudd_Not(fnv);
    }
  } else {
    index = G->index;
    fv = fnv = f;
  }

  if (topg <= topf) {
    gv = cuddT(G);
    gnv = cuddE(G);
    if (Cudd_IsComplement(g)) {
      gv = Cudd_Not(gv);
      gnv = Cudd_Not(gnv);
    }
  } else {
    gv = gnv = g;
  }

  /* Compute partial results. */
  t = cuddBddIntersectRecur(dd, fv, gv);
  if (t == NULL)
    return (NULL);
  cuddRef(t);
  if (t != zero) {
    e = zero;
  } else {
    e = cuddBddIntersectRecur(dd, fnv, gnv);
    if (e == NULL) {
      Cudd_IterDerefBdd(dd, t);
      return (NULL);
    }
  }
  cuddRef(e);

  if (t == e) { /* both equal zero */
    res = t;
  } else if (Cudd_IsComplement(t)) {
    res = cuddUniqueInter(dd, (int)index, Cudd_Not(t), Cudd_Not(e));
    if (res == NULL) {
      Cudd_IterDerefBdd(dd, t);
      Cudd_IterDerefBdd(dd, e);
      return (NULL);
    }
    res = Cudd_Not(res);
  } else {
    res = cuddUniqueInter(dd, (int)index, t, e);
    if (res == NULL) {
      Cudd_IterDerefBdd(dd, t);
      Cudd_IterDerefBdd(dd, e);
      return (NULL);
    }
  }
  cuddDeref(e);
  cuddDeref(t);

  cuddCacheInsert2(dd, Cudd_bddIntersect, f, g, res);

  return (res);

} /* end of cuddBddIntersectRecur */

/**Function********************************************************************

  Synopsis [Implements the recursive step of Cudd_bddAnd.]

  Description [Implements the recursive step of Cudd_bddAnd by taking
  the conjunction of two BDDs.  Returns a pointer to the result is
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddAnd]

******************************************************************************/
DdNode *cuddBddAndRecur(DdManager *manager, DdNode *f, DdNode *g) {
  DdNode *F, *fv, *fnv, *G, *gv, *gnv;
  DdNode *one, *r, *t, *e;
  unsigned int topf, topg, index;

  statLine(manager);
  one = DD_ONE(manager);

  /* Terminal cases. */
  F = Cudd_Regular(f);
  G = Cudd_Regular(g);
  if (F == G) {
    if (f == g)
      return (f);
    else
      return (Cudd_Not(one));
  }
  if (F == one) {
    if (f == one)
      return (g);
    else
      return (f);
  }
  if (G == one) {
    if (g == one)
      return (f);
    else
      return (g);
  }

  /* At this point f and g are not constant. */
  if (f > g) { /* Try to increase cache efficiency. */
    DdNode *tmp = f;
    f = g;
    g = tmp;
    F = Cudd_Regular(f);
    G = Cudd_Regular(g);
  }

  /* Check cache. */
  if (F->ref != 1 || G->ref != 1) {
    r = cuddCacheLookup2(manager, Cudd_bddAnd, f, g);
    if (r != NULL)
      return (r);
  }

  /* Here we can skip the use of cuddI, because the operands are known
  ** to be non-constant.
  */
  topf = manager->perm[F->index];
  topg = manager->perm[G->index];

  /* Compute cofactors. */
  if (topf <= topg) {
    index = F->index;
    fv = cuddT(F);
    fnv = cuddE(F);
    if (Cudd_IsComplement(f)) {
      fv = Cudd_Not(fv);
      fnv = Cudd_Not(fnv);
    }
  } else {
    index = G->index;
    fv = fnv = f;
  }

  if (topg <= topf) {
    gv = cuddT(G);
    gnv = cuddE(G);
    if (Cudd_IsComplement(g)) {
      gv = Cudd_Not(gv);
      gnv = Cudd_Not(gnv);
    }
  } else {
    gv = gnv = g;
  }

  t = cuddBddAndRecur(manager, fv, gv);
  if (t == NULL)
    return (NULL);
  cuddRef(t);

  e = cuddBddAndRecur(manager, fnv, gnv);
  if (e == NULL) {
    Cudd_IterDerefBdd(manager, t);
    return (NULL);
  }
  cuddRef(e);

  if (t == e) {
    r = t;
  } else {
    if (Cudd_IsComplement(t)) {
      r = cuddUniqueInter(manager, (int)index, Cudd_Not(t), Cudd_Not(e));
      if (r == NULL) {
        Cudd_IterDerefBdd(manager, t);
        Cudd_IterDerefBdd(manager, e);
        return (NULL);
      }
      r = Cudd_Not(r);
    } else {
      r = cuddUniqueInter(manager, (int)index, t, e);
      if (r == NULL) {
        Cudd_IterDerefBdd(manager, t);
        Cudd_IterDerefBdd(manager, e);
        return (NULL);
      }
    }
  }
  cuddDeref(e);
  cuddDeref(t);
  if (F->ref != 1 || G->ref != 1)
    cuddCacheInsert2(manager, Cudd_bddAnd, f, g, r);
  return (r);

} /* end of cuddBddAndRecur */

/**Function********************************************************************

  Synopsis [Implements the recursive step of Cudd_bddXor.]

  Description [Implements the recursive step of Cudd_bddXor by taking
  the exclusive OR of two BDDs.  Returns a pointer to the result is
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddXor]

******************************************************************************/
DdNode *cuddBddXorRecur(DdManager *manager, DdNode *f, DdNode *g) {
  DdNode *fv, *fnv, *G, *gv, *gnv;
  DdNode *one, *zero, *r, *t, *e;
  unsigned int topf, topg, index;

  statLine(manager);
  one = DD_ONE(manager);
  zero = Cudd_Not(one);

  /* Terminal cases. */
  if (f == g)
    return (zero);
  if (f == Cudd_Not(g))
    return (one);
  if (f > g) { /* Try to increase cache efficiency and simplify tests. */
    DdNode *tmp = f;
    f = g;
    g = tmp;
  }
  if (g == zero)
    return (f);
  if (g == one)
    return (Cudd_Not(f));
  if (Cudd_IsComplement(f)) {
    f = Cudd_Not(f);
    g = Cudd_Not(g);
  }
  /* Now the first argument is regular. */
  if (f == one)
    return (Cudd_Not(g));

  /* At this point f and g are not constant. */

  /* Check cache. */
  r = cuddCacheLookup2(manager, Cudd_bddXor, f, g);
  if (r != NULL)
    return (r);

  /* Here we can skip the use of cuddI, because the operands are known
  ** to be non-constant.
  */
  topf = manager->perm[f->index];
  G = Cudd_Regular(g);
  topg = manager->perm[G->index];

  /* Compute cofactors. */
  if (topf <= topg) {
    index = f->index;
    fv = cuddT(f);
    fnv = cuddE(f);
  } else {
    index = G->index;
    fv = fnv = f;
  }

  if (topg <= topf) {
    gv = cuddT(G);
    gnv = cuddE(G);
    if (Cudd_IsComplement(g)) {
      gv = Cudd_Not(gv);
      gnv = Cudd_Not(gnv);
    }
  } else {
    gv = gnv = g;
  }

  t = cuddBddXorRecur(manager, fv, gv);
  if (t == NULL)
    return (NULL);
  cuddRef(t);

  e = cuddBddXorRecur(manager, fnv, gnv);
  if (e == NULL) {
    Cudd_IterDerefBdd(manager, t);
    return (NULL);
  }
  cuddRef(e);

  if (t == e) {
    r = t;
  } else {
    if (Cudd_IsComplement(t)) {
      r = cuddUniqueInter(manager, (int)index, Cudd_Not(t), Cudd_Not(e));
      if (r == NULL) {
        Cudd_IterDerefBdd(manager, t);
        Cudd_IterDerefBdd(manager, e);
        return (NULL);
      }
      r = Cudd_Not(r);
    } else {
      r = cuddUniqueInter(manager, (int)index, t, e);
      if (r == NULL) {
        Cudd_IterDerefBdd(manager, t);
        Cudd_IterDerefBdd(manager, e);
        return (NULL);
      }
    }
  }
  cuddDeref(e);
  cuddDeref(t);
  cuddCacheInsert2(manager, Cudd_bddXor, f, g, r);
  return (r);

} /* end of cuddBddXorRecur */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Replaces variables with constants if possible.]

  Description [This function performs part of the transformation to
  standard form by replacing variables with constants if possible.]

  SideEffects [None]

  SeeAlso     [bddVarToCanonical bddVarToCanonicalSimple]

******************************************************************************/
static void bddVarToConst(DdNode *f, DdNode **gp, DdNode **hp, DdNode *one) {
  DdNode *g = *gp;
  DdNode *h = *hp;

  if (f == g) { /* ITE(F,F,H) = ITE(F,1,H) = F + H */
    *gp = one;
  } else if (f == Cudd_Not(g)) { /* ITE(F,!F,H) = ITE(F,0,H) = !F * H */
    *gp = Cudd_Not(one);
  }
  if (f == h) { /* ITE(F,G,F) = ITE(F,G,0) = F * G */
    *hp = Cudd_Not(one);
  } else if (f == Cudd_Not(h)) { /* ITE(F,G,!F) = ITE(F,G,1) = !F + G */
    *hp = one;
  }

} /* end of bddVarToConst */

/**Function********************************************************************

  Synopsis [Picks unique member from equiv expressions.]

  Description [Reduces 2 variable expressions to canonical form.]

  SideEffects [None]

  SeeAlso     [bddVarToConst bddVarToCanonicalSimple]

******************************************************************************/
static int bddVarToCanonical(DdManager *dd, DdNode **fp, DdNode **gp,
                             DdNode **hp, unsigned int *topfp,
                             unsigned int *topgp, unsigned int *tophp) {
  register DdNode *F, *G, *H, *r, *f, *g, *h;
  register unsigned int topf, topg, toph;
  DdNode *one = dd->one;
  int comple, change;

  f = *fp;
  g = *gp;
  h = *hp;
  F = Cudd_Regular(f);
  G = Cudd_Regular(g);
  H = Cudd_Regular(h);
  topf = cuddI(dd, F->index);
  topg = cuddI(dd, G->index);
  toph = cuddI(dd, H->index);

  change = 0;

  if (G == one) { /* ITE(F,c,H) */
    if ((topf > toph) || (topf == toph && f > h)) {
      r = h;
      h = f;
      f = r;             /* ITE(F,1,H) = ITE(H,1,F) */
      if (g != one) {    /* g == zero */
        f = Cudd_Not(f); /* ITE(F,0,H) = ITE(!H,0,!F) */
        h = Cudd_Not(h);
      }
      change = 1;
    }
  } else if (H == one) { /* ITE(F,G,c) */
    if ((topf > topg) || (topf == topg && f > g)) {
      r = g;
      g = f;
      f = r; /* ITE(F,G,0) = ITE(G,F,0) */
      if (h == one) {
        f = Cudd_Not(f); /* ITE(F,G,1) = ITE(!G,!F,1) */
        g = Cudd_Not(g);
      }
      change = 1;
    }
  } else if (g == Cudd_Not(h)) { /* ITE(F,G,!G) = ITE(G,F,!F) */
    if ((topf > topg) || (topf == topg && f > g)) {
      r = f;
      f = g;
      g = r;
      h = Cudd_Not(r);
      change = 1;
    }
  }
  /* adjust pointers so that the first 2 arguments to ITE are regular */
  if (Cudd_IsComplement(f) != 0) { /* ITE(!F,G,H) = ITE(F,H,G) */
    f = Cudd_Not(f);
    r = g;
    g = h;
    h = r;
    change = 1;
  }
  comple = 0;
  if (Cudd_IsComplement(g) != 0) { /* ITE(F,!G,H) = !ITE(F,G,!H) */
    g = Cudd_Not(g);
    h = Cudd_Not(h);
    change = 1;
    comple = 1;
  }
  if (change != 0) {
    *fp = f;
    *gp = g;
    *hp = h;
  }
  *topfp = cuddI(dd, f->index);
  *topgp = cuddI(dd, g->index);
  *tophp = cuddI(dd, Cudd_Regular(h)->index);

  return (comple);

} /* end of bddVarToCanonical */

/**Function********************************************************************

  Synopsis [Picks unique member from equiv expressions.]

  Description [Makes sure the first two pointers are regular.  This
  mat require the complementation of the result, which is signaled by
  returning 1 instead of 0.  This function is simpler than the general
  case because it assumes that no two arguments are the same or
  complementary, and no argument is constant.]

  SideEffects [None]

  SeeAlso     [bddVarToConst bddVarToCanonical]

******************************************************************************/
static int bddVarToCanonicalSimple(DdManager *dd, DdNode **fp, DdNode **gp,
                                   DdNode **hp, unsigned int *topfp,
                                   unsigned int *topgp, unsigned int *tophp) {
  register DdNode *r, *f, *g, *h;
  int comple, change;

  f = *fp;
  g = *gp;
  h = *hp;

  change = 0;

  /* adjust pointers so that the first 2 arguments to ITE are regular */
  if (Cudd_IsComplement(f)) { /* ITE(!F,G,H) = ITE(F,H,G) */
    f = Cudd_Not(f);
    r = g;
    g = h;
    h = r;
    change = 1;
  }
  comple = 0;
  if (Cudd_IsComplement(g)) { /* ITE(F,!G,H) = !ITE(F,G,!H) */
    g = Cudd_Not(g);
    h = Cudd_Not(h);
    change = 1;
    comple = 1;
  }
  if (change) {
    *fp = f;
    *gp = g;
    *hp = h;
  }

  /* Here we can skip the use of cuddI, because the operands are known
  ** to be non-constant.
  */
  *topfp = dd->perm[f->index];
  *topgp = dd->perm[g->index];
  *tophp = dd->perm[Cudd_Regular(h)->index];

  return (comple);

} /* end of bddVarToCanonicalSimple */

/**CFile***********************************************************************

  FileName    [cuddCache.c]

  PackageName [cudd]

  Synopsis    [Functions for cache insertion and lookup.]

  Description [Internal procedures included in this module:
                <ul>
                <li> cuddInitCache()
                <li> cuddCacheInsert()
                <li> cuddCacheInsert2()
                <li> cuddCacheLookup()
                <li> cuddCacheLookupZdd()
                <li> cuddCacheLookup2()
                <li> cuddCacheLookup2Zdd()
                <li> cuddConstantLookup()
                <li> cuddCacheProfile()
                <li> cuddCacheResize()
                <li> cuddCacheFlush()
                <li> cuddComputeFloorLog2()
                </ul>
            Static procedures included in this module:
                <ul>
                </ul> ]

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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifdef DD_CACHE_PROFILE
#define DD_HYSTO_BINS 8
#endif

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
// static char rcsid[] DD_UNUSED = "$Id: cuddCache.c,v 1.36 2012/02/05 01:07:18
// fabio Exp $"; #endif

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

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Initializes the computed table.]

  Description [Initializes the computed table. It is called by
  Cudd_Init. Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Init]

******************************************************************************/
int cuddInitCache(
    DdManager *unique /* unique table */,
    unsigned int cacheSize /* initial size of the cache */,
    unsigned int
        maxCacheSize /* cache size beyond which no resizing occurs */) {
  int i;
  unsigned int logSize;
#ifndef DD_CACHE_PROFILE
  DdNodePtr *mem;
  ptruint offset;
#endif

  /* Round cacheSize to largest power of 2 not greater than the requested
  ** initial cache size. */
  logSize = cuddComputeFloorLog2(ddMax(cacheSize, unique->slots / 2));
  cacheSize = 1 << logSize;
  unique->acache = ALLOC(DdCache, cacheSize + 1);
  if (unique->acache == NULL) {
    unique->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  /* If the size of the cache entry is a power of 2, we want to
  ** enforce alignment to that power of two. This happens when
  ** DD_CACHE_PROFILE is not defined. */
#ifdef DD_CACHE_PROFILE
  unique->cache = unique->acache;
  unique->memused += (cacheSize) * sizeof(DdCache);
#else
  mem = (DdNodePtr *)unique->acache;
  offset = (ptruint)mem & (sizeof(DdCache) - 1);
  mem += (sizeof(DdCache) - offset) / sizeof(DdNodePtr);
  unique->cache = (DdCache *)mem;
  assert(((ptruint)unique->cache & (sizeof(DdCache) - 1)) == 0);
  unique->memused += (cacheSize + 1) * sizeof(DdCache);
#endif
  unique->cacheSlots = cacheSize;
  unique->cacheShift = sizeof(int) * 8 - logSize;
  unique->maxCacheHard = maxCacheSize;
  /* If cacheSlack is non-negative, we can resize. */
  unique->cacheSlack =
      (int)ddMin(maxCacheSize, DD_MAX_CACHE_TO_SLOTS_RATIO * unique->slots) -
      2 * (int)cacheSize;
  Cudd_SetMinHit(unique, DD_MIN_HIT);
  /* Initialize to avoid division by 0 and immediate resizing. */
  unique->cacheMisses = (double)(int)(cacheSize * unique->minHit + 1);
  unique->cacheHits = 0;
  unique->totCachehits = 0;
  /* The sum of cacheMisses and totCacheMisses is always correct,
  ** even though cacheMisses is larger than it should for the reasons
  ** explained above. */
  unique->totCacheMisses = -unique->cacheMisses;
  unique->cachecollisions = 0;
  unique->cacheinserts = 0;
  unique->cacheLastInserts = 0;
  unique->cachedeletions = 0;

  /* Initialize the cache */
  for (i = 0; (unsigned)i < cacheSize; i++) {
    unique->cache[i].h = 0;       /* unused slots */
    unique->cache[i].data = NULL; /* invalid entry */
#ifdef DD_CACHE_PROFILE
    unique->cache[i].count = 0;
#endif
  }

  return (1);

} /* end of cuddInitCache */

/**Function********************************************************************

  Synopsis    [Inserts a result in the cache for a function with three
  operands.]

  Description [Inserts a result in the cache for a function with three
  operands.  The operator tag (see CUDD/cuddInt.h for details) is split and
stored into unused bits of the first two pointers.]

  SideEffects [None]

  SeeAlso     [cuddCacheInsert2 cuddCacheInsert1]

******************************************************************************/
void cuddCacheInsert(DdManager *table, ptruint op, DdNode *f, DdNode *g,
                     DdNode *h, DdNode *data) {
  int posn;
  register DdCache *entry;
  ptruint uf, ug, uh;

  uf = (ptruint)f | (op & 0xe);
  ug = (ptruint)g | (op >> 4);
  uh = (ptruint)h;

  posn = ddCHash2(uh, uf, ug, table->cacheShift);
  entry = &table->cache[posn];

  table->cachecollisions += entry->data != NULL;
  table->cacheinserts++;

  entry->f = (DdNode *)uf;
  entry->g = (DdNode *)ug;
  entry->h = uh;
  entry->data = data;
#ifdef DD_CACHE_PROFILE
  entry->count++;
#endif

} /* end of cuddCacheInsert */

/**Function********************************************************************

  Synopsis    [Inserts a result in the cache for a function with two
  operands.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddCacheInsert cuddCacheInsert1]

******************************************************************************/
void cuddCacheInsert2(DdManager *table, DD_CTFP op, DdNode *f, DdNode *g,
                      DdNode *data) {
  int posn;
  register DdCache *entry;

  posn = ddCHash2(op, f, g, table->cacheShift);
  entry = &table->cache[posn];

  if (entry->data != NULL) {
    table->cachecollisions++;
  }
  table->cacheinserts++;

  entry->f = f;
  entry->g = g;
  entry->h = (ptruint)op;
  entry->data = data;
#ifdef DD_CACHE_PROFILE
  entry->count++;
#endif

} /* end of cuddCacheInsert2 */

/**Function********************************************************************

  Synopsis    [Inserts a result in the cache for a function with two
  operands.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddCacheInsert cuddCacheInsert2]

******************************************************************************/
void cuddCacheInsert1(DdManager *table, DD_CTFP1 op, DdNode *f, DdNode *data) {
  int posn;
  register DdCache *entry;

  posn = ddCHash2(op, f, f, table->cacheShift);
  entry = &table->cache[posn];

  if (entry->data != NULL) {
    table->cachecollisions++;
  }
  table->cacheinserts++;

  entry->f = f;
  entry->g = f;
  entry->h = (ptruint)op;
  entry->data = data;
#ifdef DD_CACHE_PROFILE
  entry->count++;
#endif

} /* end of cuddCacheInsert1 */

/**Function********************************************************************

  Synopsis    [Looks up in the cache for the result of op applied to f,
  g, and h.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup2 cuddCacheLookup1]

******************************************************************************/
DdNode *cuddCacheLookup(DdManager *table, ptruint op, DdNode *f, DdNode *g,
                        DdNode *h) {
  int posn;
  DdCache *en, *cache;
  DdNode *data;
  ptruint uf, ug, uh;

  uf = (ptruint)f | (op & 0xe);
  ug = (ptruint)g | (op >> 4);
  uh = (ptruint)h;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif

  posn = ddCHash2(uh, uf, ug, table->cacheShift);
  en = &cache[posn];
  if (en->data != NULL && en->f == (DdNodePtr)uf && en->g == (DdNodePtr)ug &&
      en->h == uh) {
    data = Cudd_Regular(en->data);
    table->cacheHits++;
    if (data->ref == 0) {
      cuddReclaim(table, data);
    }
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddCacheLookup */

/**Function********************************************************************

  Synopsis    [Looks up in the cache for the result of op applied to f,
  g, and h.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup2Zdd cuddCacheLookup1Zdd]

******************************************************************************/
DdNode *cuddCacheLookupZdd(DdManager *table, ptruint op, DdNode *f, DdNode *g,
                           DdNode *h) {
  int posn;
  DdCache *en, *cache;
  DdNode *data;
  ptruint uf, ug, uh;

  uf = (ptruint)f | (op & 0xe);
  ug = (ptruint)g | (op >> 4);
  uh = (ptruint)h;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif

  posn = ddCHash2(uh, uf, ug, table->cacheShift);
  en = &cache[posn];
  if (en->data != NULL && en->f == (DdNodePtr)uf && en->g == (DdNodePtr)ug &&
      en->h == uh) {
    data = Cudd_Regular(en->data);
    table->cacheHits++;
    if (data->ref == 0) {
      cuddReclaimZdd(table, data);
    }
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddCacheLookupZdd */

/**Function********************************************************************

  Synopsis    [Looks up in the cache for the result of op applied to f
  and g.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup cuddCacheLookup1]

******************************************************************************/
DdNode *cuddCacheLookup2(DdManager *table, DD_CTFP op, DdNode *f, DdNode *g) {
  int posn;
  DdCache *en, *cache;
  DdNode *data;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif

  posn = ddCHash2(op, f, g, table->cacheShift);
  en = &cache[posn];
  if (en->data != NULL && en->f == f && en->g == g && en->h == (ptruint)op) {
    data = Cudd_Regular(en->data);
    table->cacheHits++;
    if (data->ref == 0) {
      cuddReclaim(table, data);
    }
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddCacheLookup2 */

/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup cuddCacheLookup2]

******************************************************************************/
DdNode *cuddCacheLookup1(DdManager *table, DD_CTFP1 op, DdNode *f) {
  int posn;
  DdCache *en, *cache;
  DdNode *data;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif

  posn = ddCHash2(op, f, f, table->cacheShift);
  en = &cache[posn];
  if (en->data != NULL && en->f == f && en->h == (ptruint)op) {
    data = Cudd_Regular(en->data);
    table->cacheHits++;
    if (data->ref == 0) {
      cuddReclaim(table, data);
    }
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddCacheLookup1 */

/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f
  and g.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookupZdd cuddCacheLookup1Zdd]

******************************************************************************/
DdNode *cuddCacheLookup2Zdd(DdManager *table, DD_CTFP op, DdNode *f,
                            DdNode *g) {
  int posn;
  DdCache *en, *cache;
  DdNode *data;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif

  posn = ddCHash2(op, f, g, table->cacheShift);
  en = &cache[posn];
  if (en->data != NULL && en->f == f && en->g == g && en->h == (ptruint)op) {
    data = Cudd_Regular(en->data);
    table->cacheHits++;
    if (data->ref == 0) {
      cuddReclaimZdd(table, data);
    }
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddCacheLookup2Zdd */

/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f.]

  Description [Returns the result if found; it returns NULL if no
  result is found.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookupZdd cuddCacheLookup2Zdd]

******************************************************************************/
DdNode *cuddCacheLookup1Zdd(DdManager *table, DD_CTFP1 op, DdNode *f) {
  int posn;
  DdCache *en, *cache;
  DdNode *data;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif

  posn = ddCHash2(op, f, f, table->cacheShift);
  en = &cache[posn];
  if (en->data != NULL && en->f == f && en->h == (ptruint)op) {
    data = Cudd_Regular(en->data);
    table->cacheHits++;
    if (data->ref == 0) {
      cuddReclaimZdd(table, data);
    }
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddCacheLookup1Zdd */

/**Function********************************************************************

  Synopsis [Looks up in the cache for the result of op applied to f,
  g, and h.]

  Description [Looks up in the cache for the result of op applied to f,
  g, and h. Assumes that the calling procedure (e.g.,
  Cudd_bddIteConstant) is only interested in whether the result is
  constant or not. Returns the result if found (possibly
  DD_NON_CONSTANT); otherwise it returns NULL.]

  SideEffects [None]

  SeeAlso     [cuddCacheLookup]

******************************************************************************/
DdNode *cuddConstantLookup(DdManager *table, ptruint op, DdNode *f, DdNode *g,
                           DdNode *h) {
  int posn;
  DdCache *en, *cache;
  ptruint uf, ug, uh;

  uf = (ptruint)f | (op & 0xe);
  ug = (ptruint)g | (op >> 4);
  uh = (ptruint)h;

  cache = table->cache;
#ifdef DD_DEBUG
  if (cache == NULL) {
    return (NULL);
  }
#endif
  posn = ddCHash2(uh, uf, ug, table->cacheShift);
  en = &cache[posn];

  /* We do not reclaim here because the result should not be
   * referenced, but only tested for being a constant.
   */
  if (en->data != NULL && en->f == (DdNodePtr)uf && en->g == (DdNodePtr)ug &&
      en->h == uh) {
    table->cacheHits++;
    return (en->data);
  }

  /* Cache miss: decide whether to resize. */
  table->cacheMisses++;

  if (table->cacheSlack >= 0 &&
      table->cacheHits > table->cacheMisses * table->minHit) {
    cuddCacheResize(table);
  }

  return (NULL);

} /* end of cuddConstantLookup */

/**Function********************************************************************

  Synopsis    [Computes and prints a profile of the cache usage.]

  Description [Computes and prints a profile of the cache usage.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddCacheProfile(DdManager *table, FILE *fp) {
  DdCache *cache = table->cache;
  int slots = table->cacheSlots;
  int nzeroes = 0;
  int i, retval;
  double exUsed;

#ifdef DD_CACHE_PROFILE
  double count, mean, meansq, stddev, expected;
  long max, min;
  int imax, imin;
  double *hystogramQ, *hystogramR; /* histograms by quotient and remainder */
  int nbins = DD_HYSTO_BINS;
  int bin;
  long thiscount;
  double totalcount, exStddev;

  meansq = mean = expected = 0.0;
  max = min = (long)cache[0].count;
  imax = imin = 0;
  totalcount = 0.0;

  hystogramQ = ALLOC(double, nbins);
  if (hystogramQ == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  hystogramR = ALLOC(double, nbins);
  if (hystogramR == NULL) {
    FREE(hystogramQ);
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (i = 0; i < nbins; i++) {
    hystogramQ[i] = 0;
    hystogramR[i] = 0;
  }

  for (i = 0; i < slots; i++) {
    thiscount = (long)cache[i].count;
    if (thiscount > max) {
      max = thiscount;
      imax = i;
    }
    if (thiscount < min) {
      min = thiscount;
      imin = i;
    }
    if (thiscount == 0) {
      nzeroes++;
    }
    count = (double)thiscount;
    mean += count;
    meansq += count * count;
    totalcount += count;
    expected += count * (double)i;
    bin = (i * nbins) / slots;
    hystogramQ[bin] += (double)thiscount;
    bin = i % nbins;
    hystogramR[bin] += (double)thiscount;
  }
  mean /= (double)slots;
  meansq /= (double)slots;

  /* Compute the standard deviation from both the data and the
  ** theoretical model for a random distribution. */
  stddev = sqrt(meansq - mean * mean);
  exStddev = sqrt((1 - 1 / (double)slots) * totalcount / (double)slots);

  retval = fprintf(fp, "Cache average accesses = %g\n", mean);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "Cache access standard deviation = %g ", stddev);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "(expected = %g)\n", exStddev);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "Cache max accesses = %ld for slot %d\n", max, imax);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "Cache min accesses = %ld for slot %d\n", min, imin);
  if (retval == EOF)
    return (0);
  exUsed = 100.0 * (1.0 - exp(-totalcount / (double)slots));
  retval = fprintf(fp, "Cache used slots = %.2f%% (expected %.2f%%)\n",
                   100.0 - (double)nzeroes * 100.0 / (double)slots, exUsed);
  if (retval == EOF)
    return (0);

  if (totalcount > 0) {
    expected /= totalcount;
    retval = fprintf(fp, "Cache access hystogram for %d bins", nbins);
    if (retval == EOF)
      return (0);
    retval = fprintf(fp, " (expected bin value = %g)\nBy quotient:", expected);
    if (retval == EOF)
      return (0);
    for (i = nbins - 1; i >= 0; i--) {
      retval = fprintf(fp, " %.0f", hystogramQ[i]);
      if (retval == EOF)
        return (0);
    }
    retval = fprintf(fp, "\nBy residue: ");
    if (retval == EOF)
      return (0);
    for (i = nbins - 1; i >= 0; i--) {
      retval = fprintf(fp, " %.0f", hystogramR[i]);
      if (retval == EOF)
        return (0);
    }
    retval = fprintf(fp, "\n");
    if (retval == EOF)
      return (0);
  }

  FREE(hystogramQ);
  FREE(hystogramR);
#else
  for (i = 0; i < slots; i++) {
    nzeroes += cache[i].h == 0;
  }
  exUsed = 100.0 * (1.0 - exp(-(table->cacheinserts - table->cacheLastInserts) /
                              (double)slots));
  retval = fprintf(fp, "Cache used slots = %.2f%% (expected %.2f%%)\n",
                   100.0 - (double)nzeroes * 100.0 / (double)slots, exUsed);
  if (retval == EOF)
    return (0);
#endif
  return (1);

} /* end of cuddCacheProfile */

/**Function********************************************************************

  Synopsis    [Resizes the cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddCacheResize(DdManager *table) {
  DdCache *cache, *oldcache, *oldacache, *entry, *old;
  int i;
  int posn, shift;
  unsigned int slots, oldslots;
  double offset;
  int moved = 0;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;
#ifndef DD_CACHE_PROFILE
  ptruint misalignment;
  DdNodePtr *mem;
#endif

  oldcache = table->cache;
  oldacache = table->acache;
  oldslots = table->cacheSlots;
  slots = table->cacheSlots = oldslots << 1;

#ifdef DD_VERBOSE
  (void)fprintf(table->err, "Resizing the cache from %d to %d entries\n",
                oldslots, slots);
  (void)fprintf(table->err, "\thits = %g\tmisses = %g\thit ratio = %5.3f\n",
                table->cacheHits, table->cacheMisses,
                table->cacheHits / (table->cacheHits + table->cacheMisses));
#endif

  saveHandler = MMoutOfMemory;
  MMoutOfMemory = Cudd_OutOfMem;
  table->acache = cache = ALLOC(DdCache, slots + 1);
  MMoutOfMemory = saveHandler;
  /* If we fail to allocate the new table we just give up. */
  if (cache == NULL) {
#ifdef DD_VERBOSE
    (void)fprintf(table->err, "Resizing failed. Giving up.\n");
#endif
    table->cacheSlots = oldslots;
    table->acache = oldacache;
    /* Do not try to resize again. */
    table->maxCacheHard = oldslots - 1;
    table->cacheSlack = -(int)(oldslots + 1);
    return;
  }
  /* If the size of the cache entry is a power of 2, we want to
  ** enforce alignment to that power of two. This happens when
  ** DD_CACHE_PROFILE is not defined. */
#ifdef DD_CACHE_PROFILE
  table->cache = cache;
#else
  mem = (DdNodePtr *)cache;
  misalignment = (ptruint)mem & (sizeof(DdCache) - 1);
  mem += (sizeof(DdCache) - misalignment) / sizeof(DdNodePtr);
  table->cache = cache = (DdCache *)mem;
  assert(((ptruint)table->cache & (sizeof(DdCache) - 1)) == 0);
#endif
  shift = --(table->cacheShift);
  table->memused += (slots - oldslots) * sizeof(DdCache);
  table->cacheSlack -= slots; /* need these many slots to double again */

  /* Clear new cache. */
  for (i = 0; (unsigned)i < slots; i++) {
    cache[i].data = NULL;
    cache[i].h = 0;
#ifdef DD_CACHE_PROFILE
    cache[i].count = 0;
#endif
  }

  /* Copy from old cache to new one. */
  for (i = 0; (unsigned)i < oldslots; i++) {
    old = &oldcache[i];
    if (old->data != NULL) {
      posn = ddCHash2(old->h, old->f, old->g, shift);
      entry = &cache[posn];
      entry->f = old->f;
      entry->g = old->g;
      entry->h = old->h;
      entry->data = old->data;
#ifdef DD_CACHE_PROFILE
      entry->count = 1;
#endif
      moved++;
    }
  }

  FREE(oldacache);

  /* Reinitialize measurements so as to avoid division by 0 and
  ** immediate resizing.
  */
  offset = (double)(int)(slots * table->minHit + 1);
  table->totCacheMisses += table->cacheMisses - offset;
  table->cacheMisses = offset;
  table->totCachehits += table->cacheHits;
  table->cacheHits = 0;
  table->cacheLastInserts = table->cacheinserts - (double)moved;

} /* end of cuddCacheResize */

/**Function********************************************************************

  Synopsis    [Flushes the cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddCacheFlush(DdManager *table) {
  int i, slots;
  DdCache *cache;

  slots = table->cacheSlots;
  cache = table->cache;
  for (i = 0; i < slots; i++) {
    table->cachedeletions += cache[i].data != NULL;
    cache[i].data = NULL;
  }
  table->cacheLastInserts = table->cacheinserts;

  return;

} /* end of cuddCacheFlush */

/**Function********************************************************************

  Synopsis    [Returns the floor of the logarithm to the base 2.]

  Description [Returns the floor of the logarithm to the base 2.
  The input value is assumed to be greater than 0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddComputeFloorLog2(unsigned int value) {
  int floorLog = 0;
#ifdef DD_DEBUG
  assert(value > 0);
#endif
  while (value > 1) {
    floorLog++;
    value >>= 1;
  }
  return (floorLog);

} /* end of cuddComputeFloorLog2 */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
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
// static char rcsid[] DD_UNUSED = "$Id: cuddCheck.c,v 1.37 2012/02/05 01:07:18
// fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void debugFindParent(DdManager *table, DdNode *node);
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
int Cudd_DebugCheck(DdManager *table) {
  unsigned int i;
  int j, count;
  int slots;
  DdNodePtr *nodelist;
  DdNode *f;
  DdNode *sentinel = &(table->sentinel);
  st_table *edgeTable; /* stores internal ref count for each node */
  st_generator *gen;
  int flag = 0;
  int totalNode;
  int deadNode;
  int index;
  int shift;

  edgeTable = st_init_table(st_ptrcmp, st_ptrhash);
  if (edgeTable == NULL)
    return (CUDD_OUT_OF_MEM);

  /* Check the BDD/ADD subtables. */
  for (i = 0; i < (unsigned)table->size; i++) {
    index = table->invperm[i];
    if (i != (unsigned)table->perm[index]) {
      (void)fprintf(table->err,
                    "Permutation corrupted: invperm[%u] = %d\t perm[%d] = %d\n",
                    i, index, index, table->perm[index]);
    }
    nodelist = table->subtables[i].nodelist;
    slots = table->subtables[i].slots;
    shift = table->subtables[i].shift;

    totalNode = 0;
    deadNode = 0;
    for (j = 0; j < slots; j++) { /* for each subtable slot */
      f = nodelist[j];
      while (f != sentinel) {
        totalNode++;
        if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref != 0) {
          if ((int)f->index != index) {
            (void)fprintf(table->err, "Error: node has illegal index\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if ((unsigned)cuddI(table, cuddT(f)->index) <= i ||
              (unsigned)cuddI(table, Cudd_Regular(cuddE(f))->index) <= i) {
            (void)fprintf(table->err, "Error: node has illegal children\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if (Cudd_Regular(cuddT(f)) != cuddT(f)) {
            (void)fprintf(table->err, "Error: node has illegal form\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if (cuddT(f) == cuddE(f)) {
            (void)fprintf(table->err, "Error: node has identical children\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if (cuddT(f)->ref == 0 || Cudd_Regular(cuddE(f))->ref == 0) {
            (void)fprintf(table->err, "Error: live node has dead children\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if (ddHash(cuddT(f), cuddE(f), shift) != j) {
            (void)fprintf(table->err, "Error: misplaced node\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          /* Increment the internal reference count for the
          ** then child of the current node.
          */
          if (st_lookup_int(edgeTable, (char *)cuddT(f), &count)) {
            count++;
          } else {
            count = 1;
          }
          if (st_insert(edgeTable, (char *)cuddT(f), (char *)(long)count) ==
              ST_OUT_OF_MEM) {
            st_free_table(edgeTable);
            return (CUDD_OUT_OF_MEM);
          }

          /* Increment the internal reference count for the
          ** else child of the current node.
          */
          if (st_lookup_int(edgeTable, (char *)Cudd_Regular(cuddE(f)),
                            &count)) {
            count++;
          } else {
            count = 1;
          }
          if (st_insert(edgeTable, (char *)Cudd_Regular(cuddE(f)),
                        (char *)(long)count) == ST_OUT_OF_MEM) {
            st_free_table(edgeTable);
            return (CUDD_OUT_OF_MEM);
          }
        } else if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref == 0) {
          deadNode++;
#if 0
                    debugCheckParent(table,f);
#endif
        } else {
          fprintf(table->err,
                  "Error: node has illegal Then or Else pointers\n");
          cuddPrintNode(f, table->err);
          flag = 1;
        }

        f = f->next;
      } /* for each element of the collision list */
    }   /* for each subtable slot */

    if ((unsigned)totalNode != table->subtables[i].keys) {
      fprintf(table->err, "Error: wrong number of total nodes\n");
      flag = 1;
    }
    if ((unsigned)deadNode != table->subtables[i].dead) {
      fprintf(table->err, "Error: wrong number of dead nodes\n");
      flag = 1;
    }
  } /* for each BDD/ADD subtable */

  /* Check the ZDD subtables. */
  for (i = 0; i < (unsigned)table->sizeZ; i++) {
    index = table->invpermZ[i];
    if (i != (unsigned)table->permZ[index]) {
      (void)fprintf(
          table->err,
          "Permutation corrupted: invpermZ[%u] = %d\t permZ[%d] = %d in ZDD\n",
          i, index, index, table->permZ[index]);
    }
    nodelist = table->subtableZ[i].nodelist;
    slots = table->subtableZ[i].slots;

    totalNode = 0;
    deadNode = 0;
    for (j = 0; j < slots; j++) { /* for each subtable slot */
      f = nodelist[j];
      while (f != NULL) {
        totalNode++;
        if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref != 0) {
          if ((int)f->index != index) {
            (void)fprintf(table->err, "Error: ZDD node has illegal index\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if (Cudd_IsComplement(cuddT(f)) || Cudd_IsComplement(cuddE(f))) {
            (void)fprintf(table->err,
                          "Error: ZDD node has complemented children\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if ((unsigned)cuddIZ(table, cuddT(f)->index) <= i ||
              (unsigned)cuddIZ(table, cuddE(f)->index) <= i) {
            (void)fprintf(table->err, "Error: ZDD node has illegal children\n");
            cuddPrintNode(f, table->err);
            cuddPrintNode(cuddT(f), table->err);
            cuddPrintNode(cuddE(f), table->err);
            flag = 1;
          }
          if (cuddT(f) == DD_ZERO(table)) {
            (void)fprintf(table->err, "Error: ZDD node has zero then child\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          if (cuddT(f)->ref == 0 || cuddE(f)->ref == 0) {
            (void)fprintf(table->err,
                          "Error: ZDD live node has dead children\n");
            cuddPrintNode(f, table->err);
            flag = 1;
          }
          /* Increment the internal reference count for the
          ** then child of the current node.
          */
          if (st_lookup_int(edgeTable, (char *)cuddT(f), &count)) {
            count++;
          } else {
            count = 1;
          }
          if (st_insert(edgeTable, (char *)cuddT(f), (char *)(long)count) ==
              ST_OUT_OF_MEM) {
            st_free_table(edgeTable);
            return (CUDD_OUT_OF_MEM);
          }

          /* Increment the internal reference count for the
          ** else child of the current node.
          */
          if (st_lookup_int(edgeTable, (char *)cuddE(f), &count)) {
            count++;
          } else {
            count = 1;
          }
          if (st_insert(edgeTable, (char *)cuddE(f), (char *)(long)count) ==
              ST_OUT_OF_MEM) {
            st_free_table(edgeTable);
            table->errorCode = CUDD_MEMORY_OUT;
            return (CUDD_OUT_OF_MEM);
          }
        } else if (cuddT(f) != NULL && cuddE(f) != NULL && f->ref == 0) {
          deadNode++;
#if 0
                    debugCheckParent(table,f);
#endif
        } else {
          fprintf(table->err,
                  "Error: ZDD node has illegal Then or Else pointers\n");
          cuddPrintNode(f, table->err);
          flag = 1;
        }

        f = f->next;
      } /* for each element of the collision list */
    }   /* for each subtable slot */

    if ((unsigned)totalNode != table->subtableZ[i].keys) {
      fprintf(table->err, "Error: wrong number of total nodes in ZDD\n");
      flag = 1;
    }
    if ((unsigned)deadNode != table->subtableZ[i].dead) {
      fprintf(table->err, "Error: wrong number of dead nodes in ZDD\n");
      flag = 1;
    }
  } /* for each ZDD subtable */

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
          fprintf(table->err, "Error: node has illegal index\n");
#if SIZEOF_VOID_P == 8
          fprintf(table->err,
                  "       node 0x%lx, id = %u, ref = %u, value = %g\n",
                  (ptruint)f, f->index, f->ref, cuddV(f));
#else
          fprintf(table->err,
                  "       node 0x%x, id = %hu, ref = %hu, value = %g\n",
                  (ptruint)f, f->index, f->ref, cuddV(f));
#endif
          flag = 1;
        }
      } else {
        deadNode++;
      }
      f = f->next;
    }
  }
  if ((unsigned)totalNode != table->constants.keys) {
    (void)fprintf(table->err,
                  "Error: wrong number of total nodes in constants\n");
    flag = 1;
  }
  if ((unsigned)deadNode != table->constants.dead) {
    (void)fprintf(table->err,
                  "Error: wrong number of dead nodes in constants\n");
    flag = 1;
  }
  gen = st_init_gen(edgeTable);
  while (st_gen(gen, &f, &count)) {
    if (count > (int)(f->ref) && f->ref != DD_MAXREF) {
#if SIZEOF_VOID_P == 8
      fprintf(table->err,
              "ref count error at node 0x%lx, count = %d, id = %u, ref = %u, "
              "then = 0x%lx, else = 0x%lx\n",
              (ptruint)f, count, f->index, f->ref, (ptruint)cuddT(f),
              (ptruint)cuddE(f));
#else
      fprintf(table->err,
              "ref count error at node 0x%x, count = %d, id = %hu, ref = %hu, "
              "then = 0x%x, else = 0x%x\n",
              (ptruint)f, count, f->index, f->ref, (ptruint)cuddT(f),
              (ptruint)cuddE(f));
#endif
      debugFindParent(table, f);
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
int Cudd_CheckKeys(DdManager *table) {
  int size;
  int i, j;
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
      (void)fprintf(table->err, "Unique table %d is not the right power of 2\n",
                    i);
      (void)fprintf(table->err, "    slots = %u shift = %d\n", slots, shift);
    }
    totalSlots += slots;
    totalDead += dead;
    for (j = 0; (unsigned)j < slots; j++) {
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
      (void)fprintf(table->err, "Wrong number of keys found \
in unique table %d (difference=%d)\n",
                    i, keys);
      count++;
    }
    if (dead != 0) {
      (void)fprintf(table->err, "Wrong number of dead found \
in unique table no. %d (difference=%d)\n",
                    i, dead);
    }
  } /* for each BDD/ADD subtable */

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
    for (j = 0; (unsigned)j < subtable->slots; j++) {
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
      (void)fprintf(table->err, "Wrong number of keys found \
in ZDD unique table no. %d (difference=%d)\n",
                    i, keys);
      count++;
    }
    if (dead != 0) {
      (void)fprintf(table->err, "Wrong number of dead found \
in ZDD unique table no. %d (difference=%d)\n",
                    i, dead);
    }
  } /* for each ZDD subtable */

  /* Check the constant table. */
  subtable = &(table->constants);
  nodelist = subtable->nodelist;
  keys = subtable->keys;
  dead = subtable->dead;
  totalKeys += keys;
  totalSlots += subtable->slots;
  totalDead += dead;
  for (j = 0; (unsigned)j < subtable->slots; j++) {
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
    (void)fprintf(table->err, "Wrong number of keys found \
in the constant table (difference=%d)\n",
                  keys);
    count++;
  }
  if (dead != 0) {
    (void)fprintf(table->err, "Wrong number of dead found \
in the constant table (difference=%d)\n",
                  dead);
  }
  if ((unsigned)totalKeys != table->keys + table->keysZ) {
    (void)fprintf(table->err, "Wrong number of total keys found \
(difference=%d)\n",
                  (int)(totalKeys - table->keys));
  }
  if ((unsigned)totalSlots != table->slots) {
    (void)fprintf(table->err, "Wrong number of total slots found \
(difference=%d)\n",
                  (int)(totalSlots - table->slots));
  }
  if (table->minDead != (unsigned)(table->gcFrac * table->slots)) {
    (void)fprintf(table->err, "Wrong number of minimum dead found \
(%u vs. %u)\n",
                  table->minDead,
                  (unsigned)(table->gcFrac * (double)table->slots));
  }
  if ((unsigned)totalDead != table->dead + table->deadZ) {
    (void)fprintf(table->err, "Wrong number of total dead found \
(difference=%d)\n",
                  (int)(totalDead - table->dead));
  }
  (void)fprintf(table->out, "Average length of non-empty lists = %g\n",
                (double)table->keys / (double)nonEmpty);

  return (count);

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
int cuddHeapProfile(DdManager *dd) {
  int ntables = dd->size;
  DdSubtable *subtables = dd->subtables;
  int i,             /* loop index */
      nodes,         /* live nodes in i-th layer */
      retval,        /* return value of fprintf */
      largest = -1,  /* index of the table with most live nodes */
      maxnodes = -1, /* maximum number of live nodes in a table */
      nonempty = 0;  /* number of tables with live nodes */

  /* Print header. */
#if SIZEOF_VOID_P == 8
  retval = fprintf(dd->out, "*** DD heap profile for 0x%lx ***\n", (ptruint)dd);
#else
  retval = fprintf(dd->out, "*** DD heap profile for 0x%x ***\n", (ptruint)dd);
#endif
  if (retval == EOF)
    return 0;

  /* Print number of live nodes for each nonempty table. */
  for (i = 0; i < ntables; i++) {
    nodes = subtables[i].keys - subtables[i].dead;
    if (nodes) {
      nonempty++;
      retval = fprintf(dd->out, "%5d: %5d nodes\n", i, nodes);
      if (retval == EOF)
        return 0;
      if (nodes > maxnodes) {
        maxnodes = nodes;
        largest = i;
      }
    }
  }

  nodes = dd->constants.keys - dd->constants.dead;
  if (nodes) {
    nonempty++;
    retval = fprintf(dd->out, "const: %5d nodes\n", nodes);
    if (retval == EOF)
      return 0;
    if (nodes > maxnodes) {
      maxnodes = nodes;
      largest = CUDD_CONST_INDEX;
    }
  }

  /* Print summary. */
  retval = fprintf(dd->out, "Summary: %d tables, %d non-empty, largest: %d ",
                   ntables + 1, nonempty, largest);
  if (retval == EOF)
    return 0;
  retval = fprintf(dd->out, "(with %d nodes)\n", maxnodes);
  if (retval == EOF)
    return 0;

  return (1);

} /* end of cuddHeapProfile */

/**Function********************************************************************

  Synopsis    [Prints out information on a node.]

  Description [Prints out information on a node.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddPrintNode(DdNode *f, FILE *fp) {
  f = Cudd_Regular(f);
#if SIZEOF_VOID_P == 8
  (void)fprintf(
      fp, "       node 0x%lx, id = %u, ref = %u, then = 0x%lx, else = 0x%lx\n",
      (ptruint)f, f->index, f->ref, (ptruint)cuddT(f), (ptruint)cuddE(f));
#else
  (void)fprintf(
      fp, "       node 0x%x, id = %hu, ref = %hu, then = 0x%x, else = 0x%x\n",
      (ptruint)f, f->index, f->ref, (ptruint)cuddT(f), (ptruint)cuddE(f));
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
void cuddPrintVarGroups(DdManager *dd /* manager */,
                        MtrNode *root /* root of the group tree */,
                        int zdd /* 0: BDD; 1: ZDD */,
                        int silent /* flag to check tree syntax only */) {
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
  if (!silent)
    (void)printf("(%d", level);
  if (MTR_TEST(root, MTR_TERMINAL) || root->child == NULL) {
    if (!silent)
      (void)printf(",");
  } else {
    node = root->child;
    while (node != NULL) {
      assert(node->low >= root->low &&
             (int)(node->low + node->size) <= (int)(root->low + root->size));
      assert(node->parent == root);
      cuddPrintVarGroups(dd, node, zdd, silent);
      node = node->younger;
    }
  }
  if (!silent) {
    (void)printf("%d", (int)(level + root->size - 1));
    if (root->flags != MTR_DEFAULT) {
      (void)printf("|");
      if (MTR_TEST(root, MTR_FIXED))
        (void)printf("F");
      if (MTR_TEST(root, MTR_NEWNODE))
        (void)printf("N");
      if (MTR_TEST(root, MTR_SOFT))
        (void)printf("S");
    }
    (void)printf(")");
    if (root->parent == NULL)
      (void)printf("\n");
  }
  assert((root->flags & ~(MTR_TERMINAL | MTR_SOFT | MTR_FIXED | MTR_NEWNODE)) ==
         0);
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
static void debugFindParent(DdManager *table, DdNode *node) {
  int i, j;
  int slots;
  DdNodePtr *nodelist;
  DdNode *f;

  for (i = 0; i < cuddI(table, node->index); i++) {
    nodelist = table->subtables[i].nodelist;
    slots = table->subtables[i].slots;

    for (j = 0; j < slots; j++) {
      f = nodelist[j];
      while (f != NULL) {
        if (cuddT(f) == node || Cudd_Regular(cuddE(f)) == node) {
#if SIZEOF_VOID_P == 8
          (void)fprintf(table->out,
                        "parent is at 0x%lx, id = %u, ref = %u, then = 0x%lx, "
                        "else = 0x%lx\n",
                        (ptruint)f, f->index, f->ref, (ptruint)cuddT(f),
                        (ptruint)cuddE(f));
#else
          (void)fprintf(table->out,
                        "parent is at 0x%x, id = %hu, ref = %hu, then = 0x%x, "
                        "else = 0x%x\n",
                        (ptruint)f, f->index, f->ref, (ptruint)cuddT(f),
                        (ptruint)cuddE(f));
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
/**CFile***********************************************************************

  FileName    [cuddCof.c]

  PackageName [cudd]

  Synopsis    [Cofactoring functions.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_Cofactor()
                <li> Cudd_CheckCube()
                </ul>
               Internal procedures included in this module:
                <ul>
                <li> cuddGetBranches()
                <li> cuddCofactorRecur()
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
// static char rcsid[] DD_UNUSED = "$Id: cuddCof.c,v 1.11 2012/02/05 01:07:18
// fabio Exp $"; #endif

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

  Synopsis    [Computes the cofactor of f with respect to g.]

  Description [Computes the cofactor of f with respect to g; g must be
  the BDD or the ADD of a cube. Returns a pointer to the cofactor if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_bddConstrain Cudd_bddRestrict]

******************************************************************************/
DdNode *Cudd_Cofactor(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *res, *zero;

  zero = Cudd_Not(DD_ONE(dd));
  if (g == zero || g == DD_ZERO(dd)) {
    (void)fprintf(dd->err, "Cudd_Cofactor: Invalid restriction 1\n");
    dd->errorCode = CUDD_INVALID_ARG;
    return (NULL);
  }
  do {
    dd->reordered = 0;
    res = cuddCofactorRecur(dd, f, g);
  } while (dd->reordered == 1);
  return (res);

} /* end of Cudd_Cofactor */

/**Function********************************************************************

  Synopsis    [Checks whether g is the BDD of a cube.]

  Description [Checks whether g is the BDD of a cube. Returns 1 in case
  of success; 0 otherwise. The constant 1 is a valid cube, but all other
  constant functions cause cuddCheckCube to return 0.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int Cudd_CheckCube(DdManager *dd, DdNode *g) {
  DdNode *g1, *g0, *one, *zero;

  one = DD_ONE(dd);
  if (g == one)
    return (1);
  if (Cudd_IsConstant(g))
    return (0);

  zero = Cudd_Not(one);
  cuddGetBranches(g, &g1, &g0);

  if (g0 == zero) {
    return (Cudd_CheckCube(dd, g1));
  }
  if (g1 == zero) {
    return (Cudd_CheckCube(dd, g0));
  }
  return (0);

} /* end of Cudd_CheckCube */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Computes the children of g.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddGetBranches(DdNode *g, DdNode **g1, DdNode **g0) {
  DdNode *G = Cudd_Regular(g);

  *g1 = cuddT(G);
  *g0 = cuddE(G);
  if (Cudd_IsComplement(g)) {
    *g1 = Cudd_Not(*g1);
    *g0 = Cudd_Not(*g0);
  }

} /* end of cuddGetBranches */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_Cofactor.]

  Description [Performs the recursive step of Cudd_Cofactor. Returns a
  pointer to the cofactor if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Cofactor]

******************************************************************************/
DdNode *cuddCofactorRecur(DdManager *dd, DdNode *f, DdNode *g) {
  DdNode *one, *zero, *F, *G, *g1, *g0, *f1, *f0, *t, *e, *r;
  unsigned int topf, topg;
  int comple;

  statLine(dd);
  F = Cudd_Regular(f);
  if (cuddIsConstant(F))
    return (f);

  one = DD_ONE(dd);

  /* The invariant g != 0 is true on entry to this procedure and is
  ** recursively maintained by it. Therefore it suffices to test g
  ** against one to make sure it is not constant.
  */
  if (g == one)
    return (f);
  /* From now on, f and g are known not to be constants. */

  comple = f != F;
  r = cuddCacheLookup2(dd, Cudd_Cofactor, F, g);
  if (r != NULL) {
    return (Cudd_NotCond(r, comple));
  }

  topf = dd->perm[F->index];
  G = Cudd_Regular(g);
  topg = dd->perm[G->index];

  /* We take the cofactors of F because we are going to rely on
  ** the fact that the cofactors of the complement are the complements
  ** of the cofactors to better utilize the cache. Variable comple
  ** remembers whether we have to complement the result or not.
  */
  if (topf <= topg) {
    f1 = cuddT(F);
    f0 = cuddE(F);
  } else {
    f1 = f0 = F;
  }
  if (topg <= topf) {
    g1 = cuddT(G);
    g0 = cuddE(G);
    if (g != G) {
      g1 = Cudd_Not(g1);
      g0 = Cudd_Not(g0);
    }
  } else {
    g1 = g0 = g;
  }

  zero = Cudd_Not(one);
  if (topf >= topg) {
    if (g0 == zero || g0 == DD_ZERO(dd)) {
      r = cuddCofactorRecur(dd, f1, g1);
    } else if (g1 == zero || g1 == DD_ZERO(dd)) {
      r = cuddCofactorRecur(dd, f0, g0);
    } else {
      (void)fprintf(dd->out, "Cudd_Cofactor: Invalid restriction 2\n");
      dd->errorCode = CUDD_INVALID_ARG;
      return (NULL);
    }
    if (r == NULL)
      return (NULL);
  } else /* if (topf < topg) */ {
    t = cuddCofactorRecur(dd, f1, g);
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = cuddCofactorRecur(dd, f0, g);
    if (e == NULL) {
      Cudd_RecursiveDeref(dd, t);
      return (NULL);
    }
    cuddRef(e);

    if (t == e) {
      r = t;
    } else if (Cudd_IsComplement(t)) {
      r = cuddUniqueInter(dd, (int)F->index, Cudd_Not(t), Cudd_Not(e));
      if (r != NULL)
        r = Cudd_Not(r);
    } else {
      r = cuddUniqueInter(dd, (int)F->index, t, e);
    }
    if (r == NULL) {
      Cudd_RecursiveDeref(dd, e);
      Cudd_RecursiveDeref(dd, t);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(dd, Cudd_Cofactor, F, g, r);

  return (Cudd_NotCond(r, comple));

} /* end of cuddCofactorRecur */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**CFile***********************************************************************

  FileName    [cuddExact.c]

  PackageName [cudd]

  Synopsis    [Functions for exact variable reordering.]

  Description [External procedures included in this file:
                <ul>
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddExact()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> getMaxBinomial()
                <li> gcd()
                <li> getMatrix()
                <li> freeMatrix()
                <li> getLevelKeys()
                <li> ddShuffle()
                <li> ddSiftUp()
                <li> updateUB()
                <li> ddCountRoots()
                <li> ddClearGlobal()
                <li> computeLB()
                <li> updateEntry()
                <li> pushDown()
                <li> initSymmInfo()
                </ul>]

  Author      [Cheng Hua, Fabio Somenzi]

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
// static char rcsid[] DD_UNUSED = "$Id: cuddExact.c,v 1.30 2012/02/05 01:07:18
// fabio Exp $"; #endif

#ifdef DD_STATS
static int ddTotalShuffles;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int getMaxBinomial(int n);
static DdHalfWord **getMatrix(int rows, int cols);
static void freeMatrix(DdHalfWord **matrix);
static int getLevelKeys(DdManager *table, int l);
static int ddShuffle(DdManager *table, DdHalfWord *permutation, int lower,
                     int upper);
static int ddSiftUp(DdManager *table, int x, int xLow);
static int updateUB(DdManager *table, int oldBound, DdHalfWord *bestOrder,
                    int lower, int upper);
static int ddCountRoots(DdManager *table, int lower, int upper);
static void ddClearGlobal(DdManager *table, int lower, int maxlevel);
static int computeLB(DdManager *table, DdHalfWord *order, int roots, int cost,
                     int lower, int upper, int level);
static int updateEntry(DdManager *table, DdHalfWord *order, int level, int cost,
                       DdHalfWord **orders, int *costs, int subsets, char *mask,
                       int lower, int upper);
static void pushDown(DdHalfWord *order, int j, int level);
static DdHalfWord *initSymmInfo(DdManager *table, int lower, int upper);
static int checkSymmInfo(DdManager *table, DdHalfWord *symmInfo, int index,
                         int level);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Exact variable ordering algorithm.]

  Description [Exact variable ordering algorithm. Finds an optimum
  order for the variables between lower and upper.  Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddExact(DdManager *table, int lower, int upper) {
  int k, i, j;
  int maxBinomial, oldSubsets, newSubsets;
  int subsetCost;
  int size; /* number of variables to be reordered */
  int unused, nvars, level, result;
  int upperBound, lowerBound, cost;
  int roots;
  char *mask = NULL;
  DdHalfWord *symmInfo = NULL;
  DdHalfWord **newOrder = NULL;
  DdHalfWord **oldOrder = NULL;
  int *newCost = NULL;
  int *oldCost = NULL;
  DdHalfWord **tmpOrder;
  int *tmpCost;
  DdHalfWord *bestOrder = NULL;
  DdHalfWord *order;
#ifdef DD_STATS
  int ddTotalSubsets;
#endif

  /* Restrict the range to be reordered by excluding unused variables
  ** at the two ends. */
  while (table->subtables[lower].keys == 1 &&
         table->vars[table->invperm[lower]]->ref == 1 && lower < upper)
    lower++;
  while (table->subtables[upper].keys == 1 &&
         table->vars[table->invperm[upper]]->ref == 1 && lower < upper)
    upper--;
  if (lower == upper)
    return (1); /* trivial problem */

  /* Apply symmetric sifting to get a good upper bound and to extract
  ** symmetry information. */
  result = cuddSymmSiftingConv(table, lower, upper);
  if (result == 0)
    goto cuddExactOutOfMem;

#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
  ddTotalShuffles = 0;
  ddTotalSubsets = 0;
#endif

  /* Initialization. */
  nvars = table->size;
  size = upper - lower + 1;
  /* Count unused variable among those to be reordered.  This is only
  ** used to compute maxBinomial. */
  unused = 0;
  for (i = lower + 1; i < upper; i++) {
    if (table->subtables[i].keys == 1 &&
        table->vars[table->invperm[i]]->ref == 1)
      unused++;
  }

  /* Find the maximum number of subsets we may have to store. */
  maxBinomial = getMaxBinomial(size - unused);
  if (maxBinomial == -1)
    goto cuddExactOutOfMem;

  newOrder = getMatrix(maxBinomial, size);
  if (newOrder == NULL)
    goto cuddExactOutOfMem;

  newCost = ALLOC(int, maxBinomial);
  if (newCost == NULL)
    goto cuddExactOutOfMem;

  oldOrder = getMatrix(maxBinomial, size);
  if (oldOrder == NULL)
    goto cuddExactOutOfMem;

  oldCost = ALLOC(int, maxBinomial);
  if (oldCost == NULL)
    goto cuddExactOutOfMem;

  bestOrder = ALLOC(DdHalfWord, size);
  if (bestOrder == NULL)
    goto cuddExactOutOfMem;

  mask = ALLOC(char, nvars);
  if (mask == NULL)
    goto cuddExactOutOfMem;

  symmInfo = initSymmInfo(table, lower, upper);
  if (symmInfo == NULL)
    goto cuddExactOutOfMem;

  roots = ddCountRoots(table, lower, upper);

  /* Initialize the old order matrix for the empty subset and the best
  ** order to the current order. The cost for the empty subset includes
  ** the cost of the levels between upper and the constants. These levels
  ** are not going to change. Hence, we count them only once.
  */
  oldSubsets = 1;
  for (i = 0; i < size; i++) {
    oldOrder[0][i] = bestOrder[i] = (DdHalfWord)table->invperm[i + lower];
  }
  subsetCost = table->constants.keys;
  for (i = upper + 1; i < nvars; i++)
    subsetCost += getLevelKeys(table, i);
  oldCost[0] = subsetCost;
  /* The upper bound is initialized to the current size of the BDDs. */
  upperBound = table->keys - table->isolated;

  /* Now consider subsets of increasing size. */
  for (k = 1; k <= size; k++) {
#ifdef DD_STATS
    (void)fprintf(table->out, "Processing subsets of size %d\n", k);
    fflush(table->out);
#endif
    newSubsets = 0;
    level = size - k; /* offset of first bottom variable */

    for (i = 0; i < oldSubsets; i++) { /* for each subset of size k-1 */
      order = oldOrder[i];
      cost = oldCost[i];
      lowerBound = computeLB(table, order, roots, cost, lower, upper, level);
      if (lowerBound >= upperBound)
        continue;
      /* Impose new order. */
      result = ddShuffle(table, order, lower, upper);
      if (result == 0)
        goto cuddExactOutOfMem;
      upperBound = updateUB(table, upperBound, bestOrder, lower, upper);
      /* For each top bottom variable. */
      for (j = level; j >= 0; j--) {
        /* Skip unused variables. */
        if (table->subtables[j + lower - 1].keys == 1 &&
            table->vars[table->invperm[j + lower - 1]]->ref == 1)
          continue;
        /* Find cost under this order. */
        subsetCost = cost + getLevelKeys(table, lower + level);
        newSubsets = updateEntry(table, order, level, subsetCost, newOrder,
                                 newCost, newSubsets, mask, lower, upper);
        if (j == 0)
          break;
        if (checkSymmInfo(table, symmInfo, order[j - 1], level) == 0)
          continue;
        pushDown(order, j - 1, level);
        /* Impose new order. */
        result = ddShuffle(table, order, lower, upper);
        if (result == 0)
          goto cuddExactOutOfMem;
        upperBound = updateUB(table, upperBound, bestOrder, lower, upper);
      } /* for each bottom variable */
    }   /* for each subset of size k */

    /* New orders become old orders in preparation for next iteration. */
    tmpOrder = oldOrder;
    tmpCost = oldCost;
    oldOrder = newOrder;
    oldCost = newCost;
    newOrder = tmpOrder;
    newCost = tmpCost;
#ifdef DD_STATS
    ddTotalSubsets += newSubsets;
#endif
    oldSubsets = newSubsets;
  }
  result = ddShuffle(table, bestOrder, lower, upper);
  if (result == 0)
    goto cuddExactOutOfMem;
#ifdef DD_STATS
#ifdef DD_VERBOSE
  (void)fprintf(table->out, "\n");
#endif
  (void)fprintf(table->out, "#:S_EXACT   %8d: total subsets\n", ddTotalSubsets);
  (void)fprintf(table->out, "#:H_EXACT   %8d: total shuffles", ddTotalShuffles);
#endif

  freeMatrix(newOrder);
  freeMatrix(oldOrder);
  FREE(bestOrder);
  FREE(oldCost);
  FREE(newCost);
  FREE(symmInfo);
  FREE(mask);
  return (1);

cuddExactOutOfMem:

  if (newOrder != NULL)
    freeMatrix(newOrder);
  if (oldOrder != NULL)
    freeMatrix(oldOrder);
  if (bestOrder != NULL)
    FREE(bestOrder);
  if (oldCost != NULL)
    FREE(oldCost);
  if (newCost != NULL)
    FREE(newCost);
  if (symmInfo != NULL)
    FREE(symmInfo);
  if (mask != NULL)
    FREE(mask);
  table->errorCode = CUDD_MEMORY_OUT;
  return (0);

} /* end of cuddExact */

/**Function********************************************************************

  Synopsis    [Returns the maximum value of (n choose k) for a given n.]

  Description [Computes the maximum value of (n choose k) for a given
  n.  The maximum value occurs for k = n/2 when n is even, or k =
  (n-1)/2 when n is odd.  The algorithm used in this procedure avoids
  intermediate overflow problems.  It is based on the identity
  <pre>
    binomial(n,k) = n/k * binomial(n-1,k-1).
  </pre>
  Returns the computed value if successful; -1 if out of range.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int getMaxBinomial(int n) {
  double i, j, result;

  if (n < 0 || n > 33)
    return (-1); /* error */
  if (n < 2)
    return (1);

  for (result = (double)((n + 3) / 2), i = result + 1, j = 2; i <= n;
       i++, j++) {
    result *= i;
    result /= j;
  }

  return ((int)result);

} /* end of getMaxBinomial */

#if 0
/**Function********************************************************************

  Synopsis    [Returns the gcd of two integers.]

  Description [Returns the gcd of two integers. Uses the binary GCD
  algorithm described in Cormen, Leiserson, and Rivest.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int
gcd(
  int  x,
  int  y)
{
    int a;
    int b;
    int lsbMask;

    /* GCD(n,0) = n. */
    if (x == 0) return(y);
    if (y == 0) return(x);

    a = x; b = y; lsbMask = 1;

    /* Here both a and b are != 0. The iteration maintains this invariant.
    ** Hence, we only need to check for when they become equal.
    */
    while (a != b) {
	if (a & lsbMask) {
	    if (b & lsbMask) {	/* both odd */
		if (a < b) {
		    b = (b - a) >> 1;
		} else {
		    a = (a - b) >> 1;
		}
	    } else {		/* a odd, b even */
		b >>= 1;
	    }
	} else {
	    if (b & lsbMask) {	/* a even, b odd */
		a >>= 1;
	    } else {		/* both even */
		lsbMask <<= 1;
	    }
	}
    }

    return(a);

} /* end of gcd */
#endif

/**Function********************************************************************

  Synopsis    [Allocates a two-dimensional matrix of ints.]

  Description [Allocates a two-dimensional matrix of ints.
  Returns the pointer to the matrix if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [freeMatrix]

******************************************************************************/
static DdHalfWord **getMatrix(int rows /* number of rows */,
                              int cols /* number of columns */) {
  DdHalfWord **matrix;
  int i;

  if (cols * rows == 0)
    return (NULL);
  matrix = ALLOC(DdHalfWord *, rows);
  if (matrix == NULL)
    return (NULL);
  matrix[0] = ALLOC(DdHalfWord, cols * rows);
  if (matrix[0] == NULL) {
    FREE(matrix);
    return (NULL);
  }
  for (i = 1; i < rows; i++) {
    matrix[i] = matrix[i - 1] + cols;
  }
  return (matrix);

} /* end of getMatrix */

/**Function********************************************************************

  Synopsis    [Frees a two-dimensional matrix allocated by getMatrix.]

  Description []

  SideEffects [None]

  SeeAlso     [getMatrix]

******************************************************************************/
static void freeMatrix(DdHalfWord **matrix) {
  FREE(matrix[0]);
  FREE(matrix);
  return;

} /* end of freeMatrix */

/**Function********************************************************************

  Synopsis    [Returns the number of nodes at one level of a unique table.]

  Description [Returns the number of nodes at one level of a unique table.
  The projection function, if isolated, is not counted.]

  SideEffects [None]

  SeeAlso []

******************************************************************************/
static int getLevelKeys(DdManager *table, int l) {
  int isolated;
  int x; /* x is an index */

  x = table->invperm[l];
  isolated = table->vars[x]->ref == 1;

  return (table->subtables[l].keys - isolated);

} /* end of getLevelKeys */

/**Function********************************************************************

  Synopsis    [Reorders variables according to a given permutation.]

  Description [Reorders variables according to a given permutation.
  The i-th permutation array contains the index of the variable that
  should be brought to the i-th level. ddShuffle assumes that no
  dead nodes are present and that the interaction matrix is properly
  initialized.  The reordering is achieved by a series of upward sifts.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso []

******************************************************************************/
static int ddShuffle(DdManager *table, DdHalfWord *permutation, int lower,
                     int upper) {
  DdHalfWord index;
  int level;
  int position;
#if 0
    int		numvars;
#endif
  int result;
#ifdef DD_STATS
  unsigned long localTime;
  int initialSize;
#ifdef DD_VERBOSE
  int finalSize;
#endif
  int previousSize;
#endif

#ifdef DD_STATS
  localTime = util_cpu_time();
  initialSize = table->keys - table->isolated;
#endif

#if 0
    numvars = table->size;

    (void) fprintf(table->out,"%d:", ddTotalShuffles);
    for (level = 0; level < numvars; level++) {
	(void) fprintf(table->out," %d", table->invperm[level]);
    }
    (void) fprintf(table->out,"\n");
#endif

  for (level = 0; level <= upper - lower; level++) {
    index = permutation[level];
    position = table->perm[index];
#ifdef DD_STATS
    previousSize = table->keys - table->isolated;
#endif
    result = ddSiftUp(table, position, level + lower);
    if (!result)
      return (0);
  }

#ifdef DD_STATS
  ddTotalShuffles++;
#ifdef DD_VERBOSE
  finalSize = table->keys - table->isolated;
  if (finalSize < initialSize) {
    (void)fprintf(table->out, "-");
  } else if (finalSize > initialSize) {
    (void)fprintf(table->out, "+");
  } else {
    (void)fprintf(table->out, "=");
  }
  if ((ddTotalShuffles & 63) == 0)
    (void)fprintf(table->out, "\n");
  fflush(table->out);
#endif
#endif

  return (1);

} /* end of ddShuffle */

/**Function********************************************************************

  Synopsis    [Moves one variable up.]

  Description [Takes a variable from position x and sifts it up to
  position xLow;  xLow should be less than or equal to x.
  Returns 1 if successful; 0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int ddSiftUp(DdManager *table, int x, int xLow) {
  int y;
  int size;

  y = cuddNextLow(table, x);
  while (y >= xLow) {
    size = cuddSwapInPlace(table, y, x);
    if (size == 0) {
      return (0);
    }
    x = y;
    y = cuddNextLow(table, x);
  }
  return (1);

} /* end of ddSiftUp */

/**Function********************************************************************

  Synopsis    [Updates the upper bound and saves the best order seen so far.]

  Description [Updates the upper bound and saves the best order seen so far.
  Returns the current value of the upper bound.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int updateUB(DdManager *table, int oldBound, DdHalfWord *bestOrder,
                    int lower, int upper) {
  int i;
  int newBound = table->keys - table->isolated;

  if (newBound < oldBound) {
#ifdef DD_STATS
    (void)fprintf(table->out, "New upper bound = %d\n", newBound);
    fflush(table->out);
#endif
    for (i = lower; i <= upper; i++)
      bestOrder[i - lower] = (DdHalfWord)table->invperm[i];
    return (newBound);
  } else {
    return (oldBound);
  }

} /* end of updateUB */

/**Function********************************************************************

  Synopsis    [Counts the number of roots.]

  Description [Counts the number of roots at the levels between lower and
  upper.  The computation is based on breadth-first search.
  A node is a root if it is not reachable from any previously visited node.
  (All the nodes at level lower are therefore considered roots.)
  The visited flag uses the LSB of the next pointer.  Returns the root
  count. The roots that are constant nodes are always ignored.]

  SideEffects [None]

  SeeAlso     [ddClearGlobal]

******************************************************************************/
static int ddCountRoots(DdManager *table, int lower, int upper) {
  int i, j;
  DdNode *f;
  DdNodePtr *nodelist;
  DdNode *sentinel = &(table->sentinel);
  int slots;
  int roots = 0;
  int maxlevel = lower;

  for (i = lower; i <= upper; i++) {
    nodelist = table->subtables[i].nodelist;
    slots = table->subtables[i].slots;
    for (j = 0; j < slots; j++) {
      f = nodelist[j];
      while (f != sentinel) {
        /* A node is a root of the DAG if it cannot be
        ** reached by nodes above it. If a node was never
        ** reached during the previous depth-first searches,
        ** then it is a root, and we start a new depth-first
        ** search from it.
        */
        if (!Cudd_IsComplement(f->next)) {
          if (f != table->vars[f->index]) {
            roots++;
          }
        }
        if (!Cudd_IsConstant(cuddT(f))) {
          cuddT(f)->next = Cudd_Complement(cuddT(f)->next);
          if (table->perm[cuddT(f)->index] > maxlevel)
            maxlevel = table->perm[cuddT(f)->index];
        }
        if (!Cudd_IsConstant(cuddE(f))) {
          Cudd_Regular(cuddE(f))->next =
              Cudd_Complement(Cudd_Regular(cuddE(f))->next);
          if (table->perm[Cudd_Regular(cuddE(f))->index] > maxlevel)
            maxlevel = table->perm[Cudd_Regular(cuddE(f))->index];
        }
        f = Cudd_Regular(f->next);
      }
    }
  }
  ddClearGlobal(table, lower, maxlevel);

  return (roots);

} /* end of ddCountRoots */

/**Function********************************************************************

  Synopsis    [Scans the DD and clears the LSB of the next pointers.]

  Description [Scans the DD and clears the LSB of the next pointers.
  The LSB of the next pointers are used as markers to tell whether a
  node was reached. Once the roots are counted, these flags are
  reset.]

  SideEffects [None]

  SeeAlso     [ddCountRoots]

******************************************************************************/
static void ddClearGlobal(DdManager *table, int lower, int maxlevel) {
  int i, j;
  DdNode *f;
  DdNodePtr *nodelist;
  DdNode *sentinel = &(table->sentinel);
  int slots;

  for (i = lower; i <= maxlevel; i++) {
    nodelist = table->subtables[i].nodelist;
    slots = table->subtables[i].slots;
    for (j = 0; j < slots; j++) {
      f = nodelist[j];
      while (f != sentinel) {
        f->next = Cudd_Regular(f->next);
        f = f->next;
      }
    }
  }

} /* end of ddClearGlobal */

/**Function********************************************************************

  Synopsis    [Computes a lower bound on the size of a BDD.]

  Description [Computes a lower bound on the size of a BDD from the
  following factors:
  <ul>
  <li> size of the lower part of it;
  <li> size of the part of the upper part not subjected to reordering;
  <li> number of roots in the part of the BDD subjected to reordering;
  <li> variable in the support of the roots in the upper part of the
       BDD subjected to reordering.
  <ul/>]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int computeLB(DdManager *table /* manager */,
                     DdHalfWord *order /* optimal order for the subset */,
                     int roots /* roots between lower and upper */,
                     int cost /* minimum cost for the subset */,
                     int lower /* lower level to be reordered */,
                     int upper /* upper level to be reordered */,
                     int level /* offset for the current top bottom var */
) {
  int i;
  int lb = cost;
  int lb1 = 0;
  int lb2;
  int support;
  DdHalfWord ref;

  /* The levels not involved in reordering are not going to change.
  ** Add their sizes to the lower bound.
  */
  for (i = 0; i < lower; i++) {
    lb += getLevelKeys(table, i);
  }
  /* If a variable is in the support, then there is going
  ** to be at least one node labeled by that variable.
  */
  for (i = lower; i <= lower + level; i++) {
    support =
        table->subtables[i].keys > 1 || table->vars[order[i - lower]]->ref > 1;
    lb1 += support;
  }

  /* Estimate the number of nodes required to connect the roots to
  ** the nodes in the bottom part. */
  if (lower + level + 1 < table->size) {
    if (lower + level < upper)
      ref = table->vars[order[level + 1]]->ref;
    else
      ref = table->vars[table->invperm[upper + 1]]->ref;
    lb2 = table->subtables[lower + level + 1].keys - (ref > (DdHalfWord)1) -
          roots;
  } else {
    lb2 = 0;
  }

  lb += lb1 > lb2 ? lb1 : lb2;

  return (lb);

} /* end of computeLB */

/**Function********************************************************************

  Synopsis    [Updates entry for a subset.]

  Description [Updates entry for a subset. Finds the subset, if it exists.
  If the new order for the subset has lower cost, or if the subset did not
  exist, it stores the new order and cost. Returns the number of subsets
  currently in the table.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int updateEntry(DdManager *table, DdHalfWord *order, int level, int cost,
                       DdHalfWord **orders, int *costs, int subsets, char *mask,
                       int lower, int upper) {
  int i, j;
  int size = upper - lower + 1;

  /* Build a mask that says what variables are in this subset. */
  for (i = lower; i <= upper; i++)
    mask[table->invperm[i]] = 0;
  for (i = level; i < size; i++)
    mask[order[i]] = 1;

  /* Check each subset until a match is found or all subsets are examined. */
  for (i = 0; i < subsets; i++) {
    DdHalfWord *subset = orders[i];
    for (j = level; j < size; j++) {
      if (mask[subset[j]] == 0)
        break;
    }
    if (j == size) /* no mismatches: success */
      break;
  }
  if (i == subsets || cost < costs[i]) { /* add or replace */
    for (j = 0; j < size; j++)
      orders[i][j] = order[j];
    costs[i] = cost;
    subsets += (i == subsets);
  }
  return (subsets);

} /* end of updateEntry */

/**Function********************************************************************

  Synopsis    [Pushes a variable in the order down to position "level."]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void pushDown(DdHalfWord *order, int j, int level) {
  int i;
  DdHalfWord tmp;

  tmp = order[j];
  for (i = j; i < level; i++) {
    order[i] = order[i + 1];
  }
  order[level] = tmp;
  return;

} /* end of pushDown */

/**Function********************************************************************

  Synopsis    [Gathers symmetry information.]

  Description [Translates the symmetry information stored in the next
  field of each subtable from level to indices. This procedure is called
  immediately after symmetric sifting, so that the next fields are correct.
  By translating this informaton in terms of indices, we make it independent
  of subsequent reorderings. The format used is that of the next fields:
  a circular list where each variable points to the next variable in the
  same symmetry group. Only the entries between lower and upper are
  considered.  The procedure returns a pointer to an array
  holding the symmetry information if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [checkSymmInfo]

******************************************************************************/
static DdHalfWord *initSymmInfo(DdManager *table, int lower, int upper) {
  int level, index, next, nextindex;
  DdHalfWord *symmInfo;

  symmInfo = ALLOC(DdHalfWord, table->size);
  if (symmInfo == NULL)
    return (NULL);

  for (level = lower; level <= upper; level++) {
    index = table->invperm[level];
    next = table->subtables[level].next;
    nextindex = table->invperm[next];
    symmInfo[index] = nextindex;
  }
  return (symmInfo);

} /* end of initSymmInfo */

/**Function********************************************************************

  Synopsis    [Check symmetry condition.]

  Description [Returns 1 if a variable is the one with the highest index
  among those belonging to a symmetry group that are in the top part of
  the BDD.  The top part is given by level.]

  SideEffects [None]

  SeeAlso     [initSymmInfo]

******************************************************************************/
static int checkSymmInfo(DdManager *table, DdHalfWord *symmInfo, int index,
                         int level) {
  int i;

  i = symmInfo[index];
  while (i != index) {
    if (index < i && table->perm[i] <= level)
      return (0);
    i = symmInfo[i];
  }
  return (1);

} /* end of checkSymmInfo */

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
// static char rcsid[] DD_UNUSED = "$Id: cuddGenetic.c,v 1.30 2012/02/05
// 01:07:18 fabio Exp $"; #endif

static int popsize; /* the size of the population */
static int numvars; /* the number of input variables in the ckt. */
/* storedd stores the population orders and sizes. This table has two
** extra rows and one extras column. The two extra rows are used for the
** offspring produced by a crossover. Each row stores one order and its
** size. The order is stored by storing the indices of variables in the
** order in which they appear in the order. The table is in reality a
** one-dimensional array which is accessed via a macro to give the illusion
** it is a two-dimensional structure.
*/
static int *storedd;
static st_table *computed; /* hash table to identify existing orders */
static int *repeat;        /* how many times an order is present */
static int large;          /* stores the index of the population with
                           ** the largest number of nodes in the DD */
static int result;
static int cross; /* the number of crossovers to perform */

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/* macro used to access the population table as if it were a
** two-dimensional structure.
*/
#define STOREDD(i, j) storedd[(i) * (numvars + 1) + (j)]

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int make_random(DdManager *table, int lower);
static int sift_up(DdManager *table, int x, int x_low);
static int build_dd(DdManager *table, int num, int lower, int upper);
static int largest(void);
static int rand_int(int a);
static int array_hash(char *array, int modulus);
static int array_compare(const char *array1, const char *array2);
static int find_best(void);
#ifdef DD_STATS
static double find_average_fitness(void);
#endif
static int PMX(int maxvar);
static int roulette(int *p1, int *p2);

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
int cuddGa(DdManager *table /* manager */,
           int lower /* lowest level to be reordered */,
           int upper /* highest level to be reorderded */) {
  int i, n, m; /* dummy/loop vars */
  int index;
#ifdef DD_STATS
  double average_fitness;
#endif
  int small; /* index of smallest DD in population */

  /* Do an initial sifting to produce at least one reasonable individual. */
  if (!cuddSifting(table, lower, upper))
    return (0);

  /* Get the initial values. */
  numvars = upper - lower + 1; /* number of variables to be reordered */
  if (table->populationSize == 0) {
    popsize = 3 * numvars; /* population size is 3 times # of vars */
    if (popsize > 120) {
      popsize = 120; /* Maximum population size is 120 */
    }
  } else {
    popsize = table->populationSize; /* user specified value */
  }
  if (popsize < 4)
    popsize = 4; /* enforce minimum population size */

  /* Allocate population table. */
  storedd = ALLOC(int, (popsize + 2) * (numvars + 1));
  if (storedd == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }

  /* Initialize the computed table. This table is made up of two data
  ** structures: A hash table with the key given by the order, which says
  ** if a given order is present in the population; and the repeat
  ** vector, which says how many copies of a given order are stored in
  ** the population table. If there are multiple copies of an order, only
  ** one has a repeat count greater than 1. This copy is the one pointed
  ** by the computed table.
  */
  repeat = ALLOC(int, popsize);
  if (repeat == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    FREE(storedd);
    return (0);
  }
  for (i = 0; i < popsize; i++) {
    repeat[i] = 0;
  }
  computed = st_init_table(array_compare, array_hash);
  if (computed == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    FREE(storedd);
    FREE(repeat);
    return (0);
  }

  /* Copy the current DD and its size to the population table. */
  for (i = 0; i < numvars; i++) {
    STOREDD(0, i) = table->invperm[i + lower]; /* order of initial DD */
  }
  STOREDD(0, numvars) = table->keys - table->isolated; /* size of initial DD */

  /* Store the initial order in the computed table. */
  if (st_insert(computed, (char *)storedd, (char *)0) == ST_OUT_OF_MEM) {
    FREE(storedd);
    FREE(repeat);
    st_free_table(computed);
    return (0);
  }
  repeat[0]++;

  /* Insert the reverse order as second element of the population. */
  for (i = 0; i < numvars; i++) {
    STOREDD(1, numvars - 1 - i) = table->invperm[i + lower]; /* reverse order */
  }

  /* Now create the random orders. make_random fills the population
  ** table with random permutations. The successive loop builds and sifts
  ** the DDs for the reverse order and each random permutation, and stores
  ** the results in the computed table.
  */
  if (!make_random(table, lower)) {
    table->errorCode = CUDD_MEMORY_OUT;
    FREE(storedd);
    FREE(repeat);
    st_free_table(computed);
    return (0);
  }
  for (i = 1; i < popsize; i++) {
    result = build_dd(table, i, lower, upper); /* build and sift order */
    if (!result) {
      FREE(storedd);
      FREE(repeat);
      st_free_table(computed);
      return (0);
    }
    if (st_lookup_int(computed, (char *)&STOREDD(i, 0), &index)) {
      repeat[index]++;
    } else {
      if (st_insert(computed, (char *)&STOREDD(i, 0), (char *)(long)i) ==
          ST_OUT_OF_MEM) {
        FREE(storedd);
        FREE(repeat);
        st_free_table(computed);
        return (0);
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
  (void)fprintf(
      table->out,
      "\nInitial population: best fitness = %d, average fitness %8.3f",
      STOREDD(small, numvars), average_fitness);
#endif

  /* Decide how many crossovers should be tried. */
  if (table->numberXovers == 0) {
    cross = 3 * numvars;
    if (cross > 60) { /* do a maximum of 50 crossovers */
      cross = 60;
    }
  } else {
    cross = table->numberXovers; /* use user specified value */
  }
  if (cross >= popsize) {
    cross = popsize;
  }

  /* Perform the crossovers to get the best order. */
  for (m = 0; m < cross; m++) {
    if (!PMX(table->size)) { /* perform one crossover */
      table->errorCode = CUDD_MEMORY_OUT;
      FREE(storedd);
      FREE(repeat);
      st_free_table(computed);
      return (0);
    }
    /* The offsprings are left in the last two entries of the
    ** population table. These are now considered in turn.
    */
    for (i = popsize; i <= popsize + 1; i++) {
      result = build_dd(table, i, lower, upper); /* build and sift child */
      if (!result) {
        FREE(storedd);
        FREE(repeat);
        st_free_table(computed);
        return (0);
      }
      large = largest(); /* find the largest DD in population */

      /* If the new child is smaller than the largest DD in the current
      ** population, enter it into the population in place of the
      ** largest DD.
      */
      if (STOREDD(i, numvars) < STOREDD(large, numvars)) {
        /* Look up the largest DD in the computed table.
        ** Decrease its repetition count. If the repetition count
        ** goes to 0, remove the largest DD from the computed table.
        */
        result = st_lookup_int(computed, (char *)&STOREDD(large, 0), &index);
        if (!result) {
          FREE(storedd);
          FREE(repeat);
          st_free_table(computed);
          return (0);
        }
        repeat[index]--;
        if (repeat[index] == 0) {
          int *pointer = &STOREDD(index, 0);
          result = st_delete(computed, &pointer, NULL);
          if (!result) {
            FREE(storedd);
            FREE(repeat);
            st_free_table(computed);
            return (0);
          }
        }
        /* Copy the new individual to the entry of the
        ** population table just made available and update the
        ** computed table.
        */
        for (n = 0; n <= numvars; n++) {
          STOREDD(large, n) = STOREDD(i, n);
        }
        if (st_lookup_int(computed, (char *)&STOREDD(large, 0), &index)) {
          repeat[index]++;
        } else {
          if (st_insert(computed, (char *)&STOREDD(large, 0),
                        (char *)(long)large) == ST_OUT_OF_MEM) {
            FREE(storedd);
            FREE(repeat);
            st_free_table(computed);
            return (0);
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
  (void)fprintf(table->out,
                "\nFinal population: best fitness = %d, average fitness %8.3f",
                STOREDD(small, numvars), average_fitness);
#endif

  /* Clean up, build the result DD, and return. */
  st_free_table(computed);
  computed = NULL;
  result = build_dd(table, small, lower, upper);
  FREE(storedd);
  FREE(repeat);
  return (result);

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
static int make_random(DdManager *table, int lower) {
  int i, j;  /* loop variables */
  int *used; /* is a number already in a permutation */
  int next;  /* next random number without repetitions */

  used = ALLOC(int, numvars);
  if (used == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
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
        next = rand_int(numvars - 1);
      } while (used[next] != 0);
      used[next] = 1;
      STOREDD(i, j) = table->invperm[next + lower];
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
  return (1);

} /* end of make_random */

/**Function********************************************************************

  Synopsis    [Moves one variable up.]

  Description [Takes a variable from position x and sifts it up to
  position x_low;  x_low should be less than x. Returns 1 if successful;
  0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int sift_up(DdManager *table, int x, int x_low) {
  int y;
  int size;

  y = cuddNextLow(table, x);
  while (y >= x_low) {
    size = cuddSwapInPlace(table, y, x);
    if (size == 0) {
      return (0);
    }
    x = y;
    y = cuddNextLow(table, x);
  }
  return (1);

} /* end of sift_up */

/**Function********************************************************************

  Synopsis [Builds a DD from a given order.]

  Description [Builds a DD from a given order.  This procedure also
  sifts the final order and inserts into the array the size in nodes
  of the result. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int build_dd(DdManager *table,
                    int num /* the index of the individual to be built */,
                    int lower, int upper) {
  int i, j; /* loop vars */
  int position;
  int index;
  int limit; /* how large the DD for this order can grow */
  int size;

  /* Check the computed table. If the order already exists, it
  ** suffices to copy the size from the existing entry.
  */
  if (computed && st_lookup_int(computed, (char *)&STOREDD(num, 0), &index)) {
    STOREDD(num, numvars) = STOREDD(index, numvars);
#ifdef DD_STATS
    (void)fprintf(table->out, "\nCache hit for index %d", index);
#endif
    return (1);
  }

  /* Stop if the DD grows 20 times larges than the reference size. */
  limit = 20 * STOREDD(0, numvars);

  /* Sift up the variables so as to build the desired permutation.
  ** First the variable that has to be on top is sifted to the top.
  ** Then the variable that has to occupy the secon position is sifted
  ** up to the second position, and so on.
  */
  for (j = 0; j < numvars; j++) {
    i = STOREDD(num, j);
    position = table->perm[i];
    result = sift_up(table, position, j + lower);
    if (!result)
      return (0);
    size = table->keys - table->isolated;
    if (size > limit)
      break;
  }

  /* Sift the DD just built. */
#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
#endif
  result = cuddSifting(table, lower, upper);
  if (!result)
    return (0);

  /* Copy order and size to table. */
  for (j = 0; j < numvars; j++) {
    STOREDD(num, j) = table->invperm[lower + j];
  }
  STOREDD(num, numvars) = table->keys - table->isolated; /* size of new DD */
  return (1);

} /* end of build_dd */

/**Function********************************************************************

  Synopsis    [Finds the largest DD in the population.]

  Description [Finds the largest DD in the population. If an order is
  repeated, it avoids choosing the copy that is in the computed table
  (it has repeat[i] > 1).]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int largest(void) {
  int i;   /* loop var */
  int big; /* temporary holder to return result */

  big = 0;
  while (repeat[big] > 1)
    big++;
  for (i = big + 1; i < popsize; i++) {
    if (STOREDD(i, numvars) >= STOREDD(big, numvars) && repeat[i] <= 1) {
      big = i;
    }
  }
  return (big);

} /* end of largest */

/**Function********************************************************************

  Synopsis    [Generates a random number between 0 and the integer a.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int rand_int(int a) {
  return (Cudd_Random() % (a + 1));

} /* end of rand_int */

/**Function********************************************************************

  Synopsis    [Hash function for the computed table.]

  Description [Hash function for the computed table. Returns the bucket
  number.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int array_hash(char *array, int modulus) {
  int val = 0;
  int i;
  int *intarray;

  intarray = (int *)array;

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
static int array_compare(const char *array1, const char *array2) {
  int i;
  int *intarray1, *intarray2;

  intarray1 = (int *)array1;
  intarray2 = (int *)array2;

  for (i = 0; i < numvars; i++) {
    if (intarray1[i] != intarray2[i])
      return (1);
  }
  return (0);

} /* end of array_compare */

/**Function********************************************************************

  Synopsis    [Returns the index of the fittest individual.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int find_best(void) {
  int i, small;

  small = 0;
  for (i = 1; i < popsize; i++) {
    if (STOREDD(i, numvars) < STOREDD(small, numvars)) {
      small = i;
    }
  }
  return (small);

} /* end of find_best */

/**Function********************************************************************

  Synopsis    [Returns the average fitness of the population.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
#ifdef DD_STATS
static double find_average_fitness(void) {
  int i;
  int total_fitness = 0;
  double average_fitness;

  for (i = 0; i < popsize; i++) {
    total_fitness += STOREDD(i, numvars);
  }
  average_fitness = (double)total_fitness / (double)popsize;
  return (average_fitness);

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
static int PMX(int maxvar) {
  int cut1, cut2; /* the two cut positions (random) */
  int mom, dad;   /* the two randomly chosen parents */
  int *inv1;      /* inverse permutations for repair algo */
  int *inv2;
  int i;    /* loop vars */
  int u, v; /* aux vars */

  inv1 = ALLOC(int, maxvar);
  if (inv1 == NULL) {
    return (0);
  }
  inv2 = ALLOC(int, maxvar);
  if (inv2 == NULL) {
    FREE(inv1);
    return (0);
  }

  /* Choose two orders from the population using roulette wheel. */
  if (!roulette(&mom, &dad)) {
    FREE(inv1);
    FREE(inv2);
    return (0);
  }

  /* Choose two random cut positions. A cut in position i means that
  ** the cut immediately precedes position i.  If cut1 < cut2, we
  ** exchange the middle of the two orderings; otherwise, we
  ** exchange the beginnings and the ends.
  */
  cut1 = rand_int(numvars - 1);
  do {
    cut2 = rand_int(numvars - 1);
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
  for (i = cut1; i != cut2; i = (i == numvars - 1) ? 0 : i + 1) {
    STOREDD(popsize, i) = STOREDD(dad, i);
    inv1[STOREDD(popsize, i)] = i;
    STOREDD(popsize + 1, i) = STOREDD(mom, i);
    inv2[STOREDD(popsize + 1, i)] = i;
  }

  /* Now apply the repair algorithm outside the cuts. */
  for (i = cut2; i != cut1; i = (i == numvars - 1) ? 0 : i + 1) {
    v = i;
    do {
      u = STOREDD(mom, v);
      v = inv1[u];
    } while (v != -1);
    STOREDD(popsize, i) = u;
    inv1[u] = i;
    v = i;
    do {
      u = STOREDD(dad, v);
      v = inv2[u];
    } while (v != -1);
    STOREDD(popsize + 1, i) = u;
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
  return (1);

} /* end of PMX */

/**Function********************************************************************

  Synopsis    [Selects two parents with the roulette wheel method.]

  Description [Selects two distinct parents with the roulette wheel method.]

  SideEffects [The indices of the selected parents are returned as side
  effects.]

  SeeAlso     []

******************************************************************************/
static int roulette(int *p1, int *p2) {
  double *wheel;
  double spin;
  int i;

  wheel = ALLOC(double, popsize);
  if (wheel == NULL) {
    return (0);
  }

  /* The fitness of an individual is the reciprocal of its size. */
  wheel[0] = 1.0 / (double)STOREDD(0, numvars);

  for (i = 1; i < popsize; i++) {
    wheel[i] = wheel[i - 1] + 1.0 / (double)STOREDD(i, numvars);
  }

  /* Get a random number between 0 and wheel[popsize-1] (that is,
  ** the sum of all fitness values. 2147483561 is the largest number
  ** returned by Cudd_Random.
  */
  spin = wheel[numvars - 1] * (double)Cudd_Random() / 2147483561.0;

  /* Find the lucky element by scanning the wheel. */
  for (i = 0; i < popsize; i++) {
    if (spin <= wheel[i])
      break;
  }
  *p1 = i;

  /* Repeat the process for the second parent, making sure it is
  ** distinct from the first.
  */
  do {
    spin = wheel[popsize - 1] * (double)Cudd_Random() / 2147483561.0;
    for (i = 0; i < popsize; i++) {
      if (spin <= wheel[i])
        break;
    }
  } while (i == *p1);
  *p2 = i;

  FREE(wheel);
  return (1);

} /* end of roulette */

/**CFile***********************************************************************

  FileName    [cuddGroup.c]

  PackageName [cudd]

  Synopsis    [Functions for group sifting.]

  Description [External procedures included in this file:
                <ul>
                <li> Cudd_MakeTreeNode()
                </ul>
        Internal procedures included in this file:
                <ul>
                <li> cuddTreeSifting()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> ddTreeSiftingAux()
                <li> ddCountInternalMtrNodes()
                <li> ddReorderChildren()
                <li> ddFindNodeHiLo()
                <li> ddUniqueCompareGroup()
                <li> ddGroupSifting()
                <li> ddCreateGroup()
                <li> ddGroupSiftingAux()
                <li> ddGroupSiftingUp()
                <li> ddGroupSiftingDown()
                <li> ddGroupMove()
                <li> ddGroupMoveBackward()
                <li> ddGroupSiftingBackward()
                <li> ddMergeGroups()
                <li> ddDissolveGroup()
                <li> ddNoCheck()
                <li> ddSecDiffCheck()
                <li> ddExtSymmCheck()
                <li> ddVarGroupCheck()
                <li> ddSetVarHandled()
                <li> ddResetVarHandled()
                <li> ddIsVarHandled()
                <li> ddFixTree()
                </ul>]

  Author      [Shipra Panda, Fabio Somenzi]

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

/* Constants for lazy sifting */
#define DD_NORMAL_SIFT 0
#define DD_LAZY_SIFT 1

/* Constants for sifting up and down */
#define DD_SIFT_DOWN 0
#define DD_SIFT_UP 1

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif
typedef int (*DD_CHKFP)(DdManager *, int, int);
#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

//#ifndef lint
// static char rcsid[] DD_UNUSED = "$Id: cuddGroup.c,v 1.49 2012/02/05 01:07:18
// fabio Exp $"; #endif

static int *entry;
extern int ddTotalNumberSwapping;
#ifdef DD_STATS
extern int ddTotalNISwaps;
static int extsymmcalls;
static int extsymm;
static int secdiffcalls;
static int secdiff;
static int secdiffmisfire;
#endif
#ifdef DD_DEBUG
static int pr = 0; /* flag to enable printing while debugging */
                   /* by depositing a 1 into it */
#endif
static unsigned int originalSize;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddTreeSiftingAux(DdManager *table, MtrNode *treenode,
                            Cudd_ReorderingType method);
#ifdef DD_STATS
static int ddCountInternalMtrNodes(DdManager *table, MtrNode *treenode);
#endif
static int ddReorderChildren(DdManager *table, MtrNode *treenode,
                             Cudd_ReorderingType method);
static void ddFindNodeHiLo(DdManager *table, MtrNode *treenode, int *lower,
                           int *upper);
static int ddUniqueCompareGroup(int *ptrX, int *ptrY);
static int ddGroupSifting(DdManager *table, int lower, int upper,
                          DD_CHKFP checkFunction, int lazyFlag);
static void ddCreateGroup(DdManager *table, int x, int y);
static int ddGroupSiftingAux(DdManager *table, int x, int xLow, int xHigh,
                             DD_CHKFP checkFunction, int lazyFlag);
static int ddGroupSiftingUp(DdManager *table, int y, int xLow,
                            DD_CHKFP checkFunction, Move **moves);
static int ddGroupSiftingDown(DdManager *table, int x, int xHigh,
                              DD_CHKFP checkFunction, Move **moves);
static int ddGroupMove(DdManager *table, int x, int y, Move **moves);
static int ddGroupMoveBackward(DdManager *table, int x, int y);
static int ddGroupSiftingBackward(DdManager *table, Move *moves, int size,
                                  int upFlag, int lazyFlag);
static void ddMergeGroups(DdManager *table, MtrNode *treenode, int low,
                          int high);
static void ddDissolveGroup(DdManager *table, int x, int y);
static int ddNoCheck(DdManager *table, int x, int y);
static int ddSecDiffCheck(DdManager *table, int x, int y);
static int ddExtSymmCheck(DdManager *table, int x, int y);
static int ddVarGroupCheck(DdManager *table, int x, int y);
static int ddSetVarHandled(DdManager *dd, int index);
static int ddResetVarHandled(DdManager *dd, int index);
static int ddIsVarHandled(DdManager *dd, int index);

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

  Synopsis    [Tree sifting algorithm.]

  Description [Tree sifting algorithm. Assumes that a tree representing
  a group hierarchy is passed as a parameter. It then reorders each
  group in postorder fashion by calling ddTreeSiftingAux.  Assumes that
  no dead nodes are present.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddTreeSifting(
    DdManager *table /* DD table */,
    Cudd_ReorderingType
        method /* reordering method for the groups of leaves */) {
  int i;
  int nvars;
  int result;
  int tempTree;

  /* If no tree is provided we create a temporary one in which all
  ** variables are in a single group. After reordering this tree is
  ** destroyed.
  */
  tempTree = table->tree == NULL;
  if (tempTree) {
    table->tree = Mtr_InitGroupTree(0, table->size);
    table->tree->index = table->invperm[0];
  }
  nvars = table->size;

#ifdef DD_DEBUG
  if (pr > 0 && !tempTree)
    (void)fprintf(table->out, "cuddTreeSifting:");
  Mtr_PrintGroups(table->tree, pr <= 0);
#endif

#ifdef DD_STATS
  extsymmcalls = 0;
  extsymm = 0;
  secdiffcalls = 0;
  secdiff = 0;
  secdiffmisfire = 0;

  (void)fprintf(table->out, "\n");
  if (!tempTree)
    (void)fprintf(table->out, "#:IM_NODES  %8d: group tree nodes\n",
                  ddCountInternalMtrNodes(table, table->tree));
#endif

  /* Initialize the group of each subtable to itself. Initially
  ** there are no groups. Groups are created according to the tree
  ** structure in postorder fashion.
  */
  for (i = 0; i < nvars; i++)
    table->subtables[i].next = i;

  /* Reorder. */
  result = ddTreeSiftingAux(table, table->tree, method);

#ifdef DD_STATS /* print stats */
  if (!tempTree && method == CUDD_REORDER_GROUP_SIFT &&
      (table->groupcheck == CUDD_GROUP_CHECK7 ||
       table->groupcheck == CUDD_GROUP_CHECK5)) {
    (void)fprintf(table->out, "\nextsymmcalls = %d\n", extsymmcalls);
    (void)fprintf(table->out, "extsymm = %d", extsymm);
  }
  if (!tempTree && method == CUDD_REORDER_GROUP_SIFT &&
      table->groupcheck == CUDD_GROUP_CHECK7) {
    (void)fprintf(table->out, "\nsecdiffcalls = %d\n", secdiffcalls);
    (void)fprintf(table->out, "secdiff = %d\n", secdiff);
    (void)fprintf(table->out, "secdiffmisfire = %d", secdiffmisfire);
  }
#endif

  if (tempTree)
    Cudd_FreeTree(table);
  else
    Mtr_ReorderGroups(table->tree, table->perm);

  return (result);

} /* end of cuddTreeSifting */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Visits the group tree and reorders each group.]

  Description [Recursively visits the group tree and reorders each
  group in postorder fashion.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddTreeSiftingAux(DdManager *table, MtrNode *treenode,
                            Cudd_ReorderingType method) {
  MtrNode *auxnode;
  int res;
  Cudd_AggregationType saveCheck;

#ifdef DD_DEBUG
  Mtr_PrintGroups(treenode, 1);
#endif

  auxnode = treenode;
  while (auxnode != NULL) {
    if (auxnode->child != NULL) {
      if (!ddTreeSiftingAux(table, auxnode->child, method))
        return (0);
      saveCheck = table->groupcheck;
      table->groupcheck = CUDD_NO_CHECK;
      if (method != CUDD_REORDER_LAZY_SIFT)
        res = ddReorderChildren(table, auxnode, CUDD_REORDER_GROUP_SIFT);
      else
        res = ddReorderChildren(table, auxnode, CUDD_REORDER_LAZY_SIFT);
      table->groupcheck = saveCheck;

      if (res == 0)
        return (0);
    } else if (auxnode->size > 1) {
      if (!ddReorderChildren(table, auxnode, method))
        return (0);
    }
    auxnode = auxnode->younger;
  }

  return (1);

} /* end of ddTreeSiftingAux */

#ifdef DD_STATS
/**Function********************************************************************

  Synopsis    [Counts the number of internal nodes of the group tree.]

  Description [Counts the number of internal nodes of the group tree.
  Returns the count.]

  SideEffects [None]

******************************************************************************/
static int ddCountInternalMtrNodes(DdManager *table, MtrNode *treenode) {
  MtrNode *auxnode;
  int count, nodeCount;

  nodeCount = 0;
  auxnode = treenode;
  while (auxnode != NULL) {
    if (!(MTR_TEST(auxnode, MTR_TERMINAL))) {
      nodeCount++;
      count = ddCountInternalMtrNodes(table, auxnode->child);
      nodeCount += count;
    }
    auxnode = auxnode->younger;
  }

  return (nodeCount);

} /* end of ddCountInternalMtrNodes */
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
static int ddReorderChildren(DdManager *table, MtrNode *treenode,
                             Cudd_ReorderingType method) {
  int lower;
  int upper;
  int result;
  unsigned int initialSize;

  ddFindNodeHiLo(table, treenode, &lower, &upper);
  /* If upper == -1 these variables do not exist yet. */
  if (upper == -1)
    return (1);

  if (treenode->flags == MTR_FIXED) {
    result = 1;
  } else {
#ifdef DD_STATS
    (void)fprintf(table->out, " ");
#endif
    switch (method) {
    case CUDD_REORDER_RANDOM:
    case CUDD_REORDER_RANDOM_PIVOT:
      result = cuddSwapping(table, lower, upper, method);
      break;
    case CUDD_REORDER_SIFT:
      result = cuddSifting(table, lower, upper);
      break;
    case CUDD_REORDER_SIFT_CONVERGE:
      do {
        initialSize = table->keys - table->isolated;
        result = cuddSifting(table, lower, upper);
        if (initialSize <= table->keys - table->isolated)
          break;
#ifdef DD_STATS
        else
          (void)fprintf(table->out, "\n");
#endif
      } while (result != 0);
      break;
    case CUDD_REORDER_SYMM_SIFT:
      result = cuddSymmSifting(table, lower, upper);
      break;
    case CUDD_REORDER_SYMM_SIFT_CONV:
      result = cuddSymmSiftingConv(table, lower, upper);
      break;
    case CUDD_REORDER_GROUP_SIFT:
      if (table->groupcheck == CUDD_NO_CHECK) {
        result = ddGroupSifting(table, lower, upper, ddNoCheck, DD_NORMAL_SIFT);
      } else if (table->groupcheck == CUDD_GROUP_CHECK5) {
        result =
            ddGroupSifting(table, lower, upper, ddExtSymmCheck, DD_NORMAL_SIFT);
      } else if (table->groupcheck == CUDD_GROUP_CHECK7) {
        result =
            ddGroupSifting(table, lower, upper, ddExtSymmCheck, DD_NORMAL_SIFT);
      } else {
        (void)fprintf(table->err, "Unknown group ckecking method\n");
        result = 0;
      }
      break;
    case CUDD_REORDER_GROUP_SIFT_CONV:
      do {
        initialSize = table->keys - table->isolated;
        if (table->groupcheck == CUDD_NO_CHECK) {
          result =
              ddGroupSifting(table, lower, upper, ddNoCheck, DD_NORMAL_SIFT);
        } else if (table->groupcheck == CUDD_GROUP_CHECK5) {
          result = ddGroupSifting(table, lower, upper, ddExtSymmCheck,
                                  DD_NORMAL_SIFT);
        } else if (table->groupcheck == CUDD_GROUP_CHECK7) {
          result = ddGroupSifting(table, lower, upper, ddExtSymmCheck,
                                  DD_NORMAL_SIFT);
        } else {
          (void)fprintf(table->err, "Unknown group ckecking method\n");
          result = 0;
        }
#ifdef DD_STATS
        (void)fprintf(table->out, "\n");
#endif
        result = cuddWindowReorder(table, lower, upper, CUDD_REORDER_WINDOW4);
        if (initialSize <= table->keys - table->isolated)
          break;
#ifdef DD_STATS
        else
          (void)fprintf(table->out, "\n");
#endif
      } while (result != 0);
      break;
    case CUDD_REORDER_WINDOW2:
    case CUDD_REORDER_WINDOW3:
    case CUDD_REORDER_WINDOW4:
    case CUDD_REORDER_WINDOW2_CONV:
    case CUDD_REORDER_WINDOW3_CONV:
    case CUDD_REORDER_WINDOW4_CONV:
      result = cuddWindowReorder(table, lower, upper, method);
      break;
    case CUDD_REORDER_ANNEALING:
      result = cuddAnnealing(table, lower, upper);
      break;
    case CUDD_REORDER_GENETIC:
      result = cuddGa(table, lower, upper);
      break;
    case CUDD_REORDER_LINEAR:
      result = cuddLinearAndSifting(table, lower, upper);
      break;
    case CUDD_REORDER_LINEAR_CONVERGE:
      do {
        initialSize = table->keys - table->isolated;
        result = cuddLinearAndSifting(table, lower, upper);
        if (initialSize <= table->keys - table->isolated)
          break;
#ifdef DD_STATS
        else
          (void)fprintf(table->out, "\n");
#endif
      } while (result != 0);
      break;
    case CUDD_REORDER_EXACT:
      result = cuddExact(table, lower, upper);
      break;
    case CUDD_REORDER_LAZY_SIFT:
      result =
          ddGroupSifting(table, lower, upper, ddVarGroupCheck, DD_LAZY_SIFT);
      break;
    default:
      return (0);
    }
  }

  /* Create a single group for all the variables that were sifted,
  ** so that they will be treated as a single block by successive
  ** invocations of ddGroupSifting.
  */
  ddMergeGroups(table, treenode, lower, upper);

#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "ddReorderChildren:");
#endif

  return (result);

} /* end of ddReorderChildren */

/**Function********************************************************************

  Synopsis    [Finds the lower and upper bounds of the group represented
  by treenode.]

  Description [Finds the lower and upper bounds of the group
  represented by treenode.  From the index and size fields we need to
  derive the current positions, and find maximum and minimum.]

  SideEffects [The bounds are returned as side effects.]

  SeeAlso     []

******************************************************************************/
static void ddFindNodeHiLo(DdManager *table, MtrNode *treenode, int *lower,
                           int *upper) {
  int low;
  int high;

  /* Check whether no variables in this group already exist.
  ** If so, return immediately. The calling procedure will know from
  ** the values of upper that no reordering is needed.
  */
  if ((int)treenode->low >= table->size) {
    *lower = table->size;
    *upper = -1;
    return;
  }

  *lower = low = (unsigned int)table->perm[treenode->index];
  high = (int)(low + treenode->size - 1);

  if (high >= table->size) {
    /* This is the case of a partially existing group. The aim is to
    ** reorder as many variables as safely possible.  If the tree
    ** node is terminal, we just reorder the subset of the group
    ** that is currently in existence.  If the group has
    ** subgroups, then we only reorder those subgroups that are
    ** fully instantiated.  This way we avoid breaking up a group.
    */
    MtrNode *auxnode = treenode->child;
    if (auxnode == NULL) {
      *upper = (unsigned int)table->size - 1;
    } else {
      /* Search the subgroup that strands the table->size line.
      ** If the first group starts at 0 and goes past table->size
      ** upper will get -1, thus correctly signaling that no reordering
      ** should take place.
      */
      while (auxnode != NULL) {
        int thisLower = table->perm[auxnode->low];
        int thisUpper = thisLower + auxnode->size - 1;
        if (thisUpper >= table->size && thisLower < table->size)
          *upper = (unsigned int)thisLower - 1;
        auxnode = auxnode->younger;
      }
    }
  } else {
    /* Normal case: All the variables of the group exist. */
    *upper = (unsigned int)high;
  }

#ifdef DD_DEBUG
  /* Make sure that all variables in group are contiguous. */
  assert(treenode->size >= *upper - *lower + 1);
#endif

  return;

} /* end of ddFindNodeHiLo */

/**Function********************************************************************

  Synopsis    [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the variables
  according to the number of keys in the subtables.  Returns the
  difference in number of keys between the two variables being
  compared.]

  SideEffects [None]

******************************************************************************/
static int ddUniqueCompareGroup(int *ptrX, int *ptrY) {
#if 0
    if (entry[*ptrY] == entry[*ptrX]) {
	return((*ptrX) - (*ptrY));
    }
#endif
  return (entry[*ptrY] - entry[*ptrX]);

} /* end of ddUniqueCompareGroup */

/**Function********************************************************************

  Synopsis    [Sifts from treenode->low to treenode->high.]

  Description [Sifts from treenode->low to treenode->high. If
  croupcheck == CUDD_GROUP_CHECK7, it checks for group creation at the
  end of the initial sifting. If a group is created, it is then sifted
  again. After sifting one variable, the group that contains it is
  dissolved.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddGroupSifting(DdManager *table, int lower, int upper,
                          DD_CHKFP checkFunction, int lazyFlag) {
  int *var;
  int i, j, x, xInit;
  int nvars;
  int classes;
  int result;
  int *sifted;
  int merged;
  int dissolve;
#ifdef DD_STATS
  unsigned previousSize;
#endif
  int xindex;

  nvars = table->size;

  /* Order variables to sift. */
  entry = NULL;
  sifted = NULL;
  var = ALLOC(int, nvars);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddGroupSiftingOutOfMem;
  }
  entry = ALLOC(int, nvars);
  if (entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddGroupSiftingOutOfMem;
  }
  sifted = ALLOC(int, nvars);
  if (sifted == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddGroupSiftingOutOfMem;
  }

  /* Here we consider only one representative for each group. */
  for (i = 0, classes = 0; i < nvars; i++) {
    sifted[i] = 0;
    x = table->perm[i];
    if ((unsigned)x >= table->subtables[x].next) {
      entry[i] = table->subtables[x].keys;
      var[classes] = i;
      classes++;
    }
  }

  qsort((void *)var, classes, sizeof(int), (DD_QSFP)ddUniqueCompareGroup);

  if (lazyFlag) {
    for (i = 0; i < nvars; i++) {
      ddResetVarHandled(table, i);
    }
  }

  /* Now sift. */
  for (i = 0; i < ddMin(table->siftMaxVar, classes); i++) {
    if (ddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (util_cpu_time() - table->startTime + table->reordTime >
        table->timeLimit) {
      table->autoDyn = 0; /* prevent further reordering */
      break;
    }
    xindex = var[i];
    if (sifted[xindex] == 1) /* variable already sifted as part of group */
      continue;
    x = table->perm[xindex]; /* find current level of this variable */

    if (x < lower || x > upper || table->subtables[x].bindVar == 1)
      continue;
#ifdef DD_STATS
    previousSize = table->keys - table->isolated;
#endif
#ifdef DD_DEBUG
    /* x is bottom of group */
    assert((unsigned)x >= table->subtables[x].next);
#endif
    if ((unsigned)x == table->subtables[x].next) {
      dissolve = 1;
      result =
          ddGroupSiftingAux(table, x, lower, upper, checkFunction, lazyFlag);
    } else {
      dissolve = 0;
      result = ddGroupSiftingAux(table, x, lower, upper, ddNoCheck, lazyFlag);
    }
    if (!result)
      goto ddGroupSiftingOutOfMem;

    /* check for aggregation */
    merged = 0;
    if (lazyFlag == 0 && table->groupcheck == CUDD_GROUP_CHECK7) {
      x = table->perm[xindex];                       /* find current level */
      if ((unsigned)x == table->subtables[x].next) { /* not part of a group */
        if (x != upper && sifted[table->invperm[x + 1]] == 0 &&
            (unsigned)x + 1 == table->subtables[x + 1].next) {
          if (ddSecDiffCheck(table, x, x + 1)) {
            merged = 1;
            ddCreateGroup(table, x, x + 1);
          }
        }
        if (x != lower && sifted[table->invperm[x - 1]] == 0 &&
            (unsigned)x - 1 == table->subtables[x - 1].next) {
          if (ddSecDiffCheck(table, x - 1, x)) {
            merged = 1;
            ddCreateGroup(table, x - 1, x);
          }
        }
      }
    }

    if (merged) { /* a group was created */
      /* move x to bottom of group */
      while ((unsigned)x < table->subtables[x].next)
        x = table->subtables[x].next;
      /* sift */
      result = ddGroupSiftingAux(table, x, lower, upper, ddNoCheck, lazyFlag);
      if (!result)
        goto ddGroupSiftingOutOfMem;
#ifdef DD_STATS
      if (table->keys < previousSize + table->isolated) {
        (void)fprintf(table->out, "_");
      } else if (table->keys > previousSize + table->isolated) {
        (void)fprintf(table->out, "^");
      } else {
        (void)fprintf(table->out, "*");
      }
      fflush(table->out);
    } else {
      if (table->keys < previousSize + table->isolated) {
        (void)fprintf(table->out, "-");
      } else if (table->keys > previousSize + table->isolated) {
        (void)fprintf(table->out, "+");
      } else {
        (void)fprintf(table->out, "=");
      }
      fflush(table->out);
#endif
    }

    /* Mark variables in the group just sifted. */
    x = table->perm[xindex];
    if ((unsigned)x != table->subtables[x].next) {
      xInit = x;
      do {
        j = table->invperm[x];
        sifted[j] = 1;
        x = table->subtables[x].next;
      } while (x != xInit);

      /* Dissolve the group if it was created. */
      if (lazyFlag == 0 && dissolve) {
        do {
          j = table->subtables[x].next;
          table->subtables[x].next = x;
          x = j;
        } while (x != xInit);
      }
    }

#ifdef DD_DEBUG
    if (pr > 0)
      (void)fprintf(table->out, "ddGroupSifting:");
#endif

    if (lazyFlag)
      ddSetVarHandled(table, xindex);
  } /* for */

  FREE(sifted);
  FREE(var);
  FREE(entry);

  return (1);

ddGroupSiftingOutOfMem:
  if (entry != NULL)
    FREE(entry);
  if (var != NULL)
    FREE(var);
  if (sifted != NULL)
    FREE(sifted);

  return (0);

} /* end of ddGroupSifting */

/**Function********************************************************************

  Synopsis    [Creates a group encompassing variables from x to y in the
  DD table.]

  Description [Creates a group encompassing variables from x to y in the
  DD table. In the current implementation it must be y == x+1.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static void ddCreateGroup(DdManager *table, int x, int y) {
  int gybot;

#ifdef DD_DEBUG
  assert(y == x + 1);
#endif

  /* Find bottom of second group. */
  gybot = y;
  while ((unsigned)gybot < table->subtables[gybot].next)
    gybot = table->subtables[gybot].next;

  /* Link groups. */
  table->subtables[x].next = y;
  table->subtables[gybot].next = x;

  return;

} /* ddCreateGroup */

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
static int ddGroupSiftingAux(DdManager *table, int x, int xLow, int xHigh,
                             DD_CHKFP checkFunction, int lazyFlag) {
  Move *move;
  Move *moves; /* list of moves */
  int initialSize;
  int result;
  int y;
  int topbot;

#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "ddGroupSiftingAux from %d to %d\n", xLow, xHigh);
  assert((unsigned)x >= table->subtables[x].next); /* x is bottom of group */
#endif

  initialSize = table->keys - table->isolated;
  moves = NULL;

  originalSize = initialSize; /* for lazy sifting */

  /* If we have a singleton, we check for aggregation in both
  ** directions before we sift.
  */
  if ((unsigned)x == table->subtables[x].next) {
    /* Will go down first, unless x == xHigh:
    ** Look for aggregation above x.
    */
    for (y = x; y > xLow; y--) {
      if (!checkFunction(table, y - 1, y))
        break;
      topbot = table->subtables[y - 1].next; /* find top of y-1's group */
      table->subtables[y - 1].next = y;
      table->subtables[x].next = topbot; /* x is bottom of group so its */
      /* next is top of y-1's group */
      y = topbot + 1; /* add 1 for y--; new y is top of group */
    }
    /* Will go up first unless x == xlow:
    ** Look for aggregation below x.
    */
    for (y = x; y < xHigh; y++) {
      if (!checkFunction(table, y, y + 1))
        break;
      /* find bottom of y+1's group */
      topbot = y + 1;
      while ((unsigned)topbot < table->subtables[topbot].next) {
        topbot = table->subtables[topbot].next;
      }
      table->subtables[topbot].next = table->subtables[y].next;
      table->subtables[y].next = y + 1;
      y = topbot - 1; /* subtract 1 for y++; new y is bottom of group */
    }
  }

  /* Now x may be in the middle of a group.
  ** Find bottom of x's group.
  */
  while ((unsigned)x < table->subtables[x].next)
    x = table->subtables[x].next;

  if (x == xLow) { /* Sift down */
#ifdef DD_DEBUG
    /* x must be a singleton */
    assert((unsigned)x == table->subtables[x].next);
#endif
    if (x == xHigh)
      return (1); /* just one variable */

    if (!ddGroupSiftingDown(table, x, xHigh, checkFunction, &moves))
      goto ddGroupSiftingAuxOutOfMem;
    /* at this point x == xHigh, unless early term */

    /* move backward and stop at best position */
    result = ddGroupSiftingBackward(table, moves, initialSize, DD_SIFT_DOWN,
                                    lazyFlag);
#ifdef DD_DEBUG
    assert(table->keys - table->isolated <= (unsigned)initialSize);
#endif
    if (!result)
      goto ddGroupSiftingAuxOutOfMem;

  } else if (cuddNextHigh(table, x) > xHigh) { /* Sift up */
#ifdef DD_DEBUG
    /* x is bottom of group */
    assert((unsigned)x >= table->subtables[x].next);
#endif
    /* Find top of x's group */
    x = table->subtables[x].next;

    if (!ddGroupSiftingUp(table, x, xLow, checkFunction, &moves))
      goto ddGroupSiftingAuxOutOfMem;
    /* at this point x == xLow, unless early term */

    /* move backward and stop at best position */
    result =
        ddGroupSiftingBackward(table, moves, initialSize, DD_SIFT_UP, lazyFlag);
#ifdef DD_DEBUG
    assert(table->keys - table->isolated <= (unsigned)initialSize);
#endif
    if (!result)
      goto ddGroupSiftingAuxOutOfMem;

  } else if (x - xLow > xHigh - x) { /* must go down first: shorter */
    if (!ddGroupSiftingDown(table, x, xHigh, checkFunction, &moves))
      goto ddGroupSiftingAuxOutOfMem;
    /* at this point x == xHigh, unless early term */

    /* Find top of group */
    if (moves) {
      x = moves->y;
    }
    while ((unsigned)x < table->subtables[x].next)
      x = table->subtables[x].next;
    x = table->subtables[x].next;
#ifdef DD_DEBUG
    /* x should be the top of a group */
    assert((unsigned)x <= table->subtables[x].next);
#endif

    if (!ddGroupSiftingUp(table, x, xLow, checkFunction, &moves))
      goto ddGroupSiftingAuxOutOfMem;

    /* move backward and stop at best position */
    result =
        ddGroupSiftingBackward(table, moves, initialSize, DD_SIFT_UP, lazyFlag);
#ifdef DD_DEBUG
    assert(table->keys - table->isolated <= (unsigned)initialSize);
#endif
    if (!result)
      goto ddGroupSiftingAuxOutOfMem;

  } else { /* moving up first: shorter */
    /* Find top of x's group */
    x = table->subtables[x].next;

    if (!ddGroupSiftingUp(table, x, xLow, checkFunction, &moves))
      goto ddGroupSiftingAuxOutOfMem;
    /* at this point x == xHigh, unless early term */

    if (moves) {
      x = moves->x;
    }
    while ((unsigned)x < table->subtables[x].next)
      x = table->subtables[x].next;
#ifdef DD_DEBUG
    /* x is bottom of a group */
    assert((unsigned)x >= table->subtables[x].next);
#endif

    if (!ddGroupSiftingDown(table, x, xHigh, checkFunction, &moves))
      goto ddGroupSiftingAuxOutOfMem;

    /* move backward and stop at best position */
    result = ddGroupSiftingBackward(table, moves, initialSize, DD_SIFT_DOWN,
                                    lazyFlag);
#ifdef DD_DEBUG
    assert(table->keys - table->isolated <= (unsigned)initialSize);
#endif
    if (!result)
      goto ddGroupSiftingAuxOutOfMem;
  }

  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }

  return (1);

ddGroupSiftingAuxOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }

  return (0);

} /* end of ddGroupSiftingAux */

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
static int ddGroupSiftingUp(DdManager *table, int y, int xLow,
                            DD_CHKFP checkFunction, Move **moves) {
  Move *move;
  int x;
  int size;
  int i;
  int gxtop, gybot;
  int limitSize;
  int xindex, yindex;
  int zindex;
  int z;
  int isolated;
  int L; /* lower bound on DD size */
#ifdef DD_DEBUG
  int checkL;
#endif

  yindex = table->invperm[y];

  /* Initialize the lower bound.
  ** The part of the DD below the bottom of y's group will not change.
  ** The part of the DD above y that does not interact with any
  ** variable of y's group will not change.
  ** The rest may vanish in the best case, except for
  ** the nodes at level xLow, which will not vanish, regardless.
  ** What we use here is not really a lower bound, because we ignore
  ** the interactions with all variables except y.
  */
  limitSize = L = table->keys - table->isolated;
  gybot = y;
  while ((unsigned)gybot < table->subtables[gybot].next)
    gybot = table->subtables[gybot].next;
  for (z = xLow + 1; z <= gybot; z++) {
    zindex = table->invperm[z];
    if (zindex == yindex || cuddTestInteract(table, zindex, yindex)) {
      isolated = table->vars[zindex]->ref == 1;
      L -= table->subtables[z].keys - isolated;
    }
  }

  x = cuddNextLow(table, y);
  while (x >= xLow && L <= limitSize) {
#ifdef DD_DEBUG
    gybot = y;
    while ((unsigned)gybot < table->subtables[gybot].next)
      gybot = table->subtables[gybot].next;
    checkL = table->keys - table->isolated;
    for (z = xLow + 1; z <= gybot; z++) {
      zindex = table->invperm[z];
      if (zindex == yindex || cuddTestInteract(table, zindex, yindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkL -= table->subtables[z].keys - isolated;
      }
    }
    if (pr > 0 && L != checkL) {
      (void)fprintf(table->out, "Inaccurate lower bound: L = %d checkL = %d\n",
                    L, checkL);
    }
#endif
    gxtop = table->subtables[x].next;
    if (checkFunction(table, x, y)) {
      /* Group found, attach groups */
      table->subtables[x].next = y;
      i = table->subtables[y].next;
      while (table->subtables[i].next != (unsigned)y)
        i = table->subtables[i].next;
      table->subtables[i].next = gxtop;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddGroupSiftingUpOutOfMem;
      move->x = x;
      move->y = y;
      move->flags = MTR_NEWNODE;
      move->size = table->keys - table->isolated;
      move->next = *moves;
      *moves = move;
    } else if (table->subtables[x].next == (unsigned)x &&
               table->subtables[y].next == (unsigned)y) {
      /* x and y are self groups */
      xindex = table->invperm[x];
      size = cuddSwapInPlace(table, x, y);
#ifdef DD_DEBUG
      assert(table->subtables[x].next == (unsigned)x);
      assert(table->subtables[y].next == (unsigned)y);
#endif
      if (size == 0)
        goto ddGroupSiftingUpOutOfMem;
      /* Update the lower bound. */
      if (cuddTestInteract(table, xindex, yindex)) {
        isolated = table->vars[xindex]->ref == 1;
        L += table->subtables[y].keys - isolated;
      }
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddGroupSiftingUpOutOfMem;
      move->x = x;
      move->y = y;
      move->flags = MTR_DEFAULT;
      move->size = size;
      move->next = *moves;
      *moves = move;

#ifdef DD_DEBUG
      if (pr > 0)
        (void)fprintf(table->out, "ddGroupSiftingUp (2 single groups):\n");
#endif
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;
    } else { /* Group move */
      size = ddGroupMove(table, x, y, moves);
      if (size == 0)
        goto ddGroupSiftingUpOutOfMem;
      /* Update the lower bound. */
      z = (*moves)->y;
      do {
        zindex = table->invperm[z];
        if (cuddTestInteract(table, zindex, yindex)) {
          isolated = table->vars[zindex]->ref == 1;
          L += table->subtables[z].keys - isolated;
        }
        z = table->subtables[z].next;
      } while (z != (int)(*moves)->y);
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;
    }
    y = gxtop;
    x = cuddNextLow(table, y);
  }

  return (1);

ddGroupSiftingUpOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }
  return (0);

} /* end of ddGroupSiftingUp */

/**Function********************************************************************

  Synopsis    [Sifts down a variable until it reaches position xHigh.]

  Description [Sifts down a variable until it reaches position xHigh.
  Assumes that x is the bottom of a group (or a singleton).  Records
  all the moves.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddGroupSiftingDown(DdManager *table, int x, int xHigh,
                              DD_CHKFP checkFunction, Move **moves) {
  Move *move;
  int y;
  int size;
  int limitSize;
  int gxtop, gybot;
  int R; /* upper bound on node decrease */
  int xindex, yindex;
  int isolated, allVars;
  int z;
  int zindex;
#ifdef DD_DEBUG
  int checkR;
#endif

  /* If the group consists of simple variables, there is no point in
  ** sifting it down. This check is redundant if the projection functions
  ** do not have external references, because the computation of the
  ** lower bound takes care of the problem.  It is necessary otherwise to
  ** prevent the sifting down of simple variables. */
  y = x;
  allVars = 1;
  do {
    if (table->subtables[y].keys != 1) {
      allVars = 0;
      break;
    }
    y = table->subtables[y].next;
  } while (table->subtables[y].next != (unsigned)x);
  if (allVars)
    return (1);

  /* Initialize R. */
  xindex = table->invperm[x];
  gxtop = table->subtables[x].next;
  limitSize = size = table->keys - table->isolated;
  R = 0;
  for (z = xHigh; z > gxtop; z--) {
    zindex = table->invperm[z];
    if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
      isolated = table->vars[zindex]->ref == 1;
      R += table->subtables[z].keys - isolated;
    }
  }

  y = cuddNextHigh(table, x);
  while (y <= xHigh && size - R < limitSize) {
#ifdef DD_DEBUG
    gxtop = table->subtables[x].next;
    checkR = 0;
    for (z = xHigh; z > gxtop; z--) {
      zindex = table->invperm[z];
      if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkR += table->subtables[z].keys - isolated;
      }
    }
    assert(R >= checkR);
#endif
    /* Find bottom of y group. */
    gybot = table->subtables[y].next;
    while (table->subtables[gybot].next != (unsigned)y)
      gybot = table->subtables[gybot].next;

    if (checkFunction(table, x, y)) {
      /* Group found: attach groups and record move. */
      gxtop = table->subtables[x].next;
      table->subtables[x].next = y;
      table->subtables[gybot].next = gxtop;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddGroupSiftingDownOutOfMem;
      move->x = x;
      move->y = y;
      move->flags = MTR_NEWNODE;
      move->size = table->keys - table->isolated;
      move->next = *moves;
      *moves = move;
    } else if (table->subtables[x].next == (unsigned)x &&
               table->subtables[y].next == (unsigned)y) {
      /* x and y are self groups */
      /* Update upper bound on node decrease. */
      yindex = table->invperm[y];
      if (cuddTestInteract(table, xindex, yindex)) {
        isolated = table->vars[yindex]->ref == 1;
        R -= table->subtables[y].keys - isolated;
      }
      size = cuddSwapInPlace(table, x, y);
#ifdef DD_DEBUG
      assert(table->subtables[x].next == (unsigned)x);
      assert(table->subtables[y].next == (unsigned)y);
#endif
      if (size == 0)
        goto ddGroupSiftingDownOutOfMem;

      /* Record move. */
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddGroupSiftingDownOutOfMem;
      move->x = x;
      move->y = y;
      move->flags = MTR_DEFAULT;
      move->size = size;
      move->next = *moves;
      *moves = move;

#ifdef DD_DEBUG
      if (pr > 0)
        (void)fprintf(table->out, "ddGroupSiftingDown (2 single groups):\n");
#endif
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;

      x = y;
      y = cuddNextHigh(table, x);
    } else { /* Group move */
      /* Update upper bound on node decrease: first phase. */
      gxtop = table->subtables[x].next;
      z = gxtop + 1;
      do {
        zindex = table->invperm[z];
        if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
          isolated = table->vars[zindex]->ref == 1;
          R -= table->subtables[z].keys - isolated;
        }
        z++;
      } while (z <= gybot);
      size = ddGroupMove(table, x, y, moves);
      if (size == 0)
        goto ddGroupSiftingDownOutOfMem;
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;

      /* Update upper bound on node decrease: second phase. */
      gxtop = table->subtables[gybot].next;
      for (z = gxtop + 1; z <= gybot; z++) {
        zindex = table->invperm[z];
        if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
          isolated = table->vars[zindex]->ref == 1;
          R += table->subtables[z].keys - isolated;
        }
      }
    }
    x = gybot;
    y = cuddNextHigh(table, x);
  }

  return (1);

ddGroupSiftingDownOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }

  return (0);

} /* end of ddGroupSiftingDown */

/**Function********************************************************************

  Synopsis    [Swaps two groups and records the move.]

  Description [Swaps two groups and records the move. Returns the
  number of keys in the DD table in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddGroupMove(DdManager *table, int x, int y, Move **moves) {
  Move *move;
  int size;
  int i, j, xtop, xbot, xsize, ytop, ybot, ysize, newxtop;
  int swapx, swapy;
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  int initialSize, bestSize;
#endif

#ifdef DD_DEBUG
  /* We assume that x < y */
  assert(x < y);
#endif
  /* Find top, bottom, and size for the two groups. */
  xbot = x;
  xtop = table->subtables[x].next;
  xsize = xbot - xtop + 1;
  ybot = y;
  while ((unsigned)ybot < table->subtables[ybot].next)
    ybot = table->subtables[ybot].next;
  ytop = y;
  ysize = ybot - ytop + 1;

#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  initialSize = bestSize = table->keys - table->isolated;
#endif
  /* Sift the variables of the second group up through the first group */
  for (i = 1; i <= ysize; i++) {
    for (j = 1; j <= xsize; j++) {
      size = cuddSwapInPlace(table, x, y);
      if (size == 0)
        goto ddGroupMoveOutOfMem;
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
      if (size < bestSize)
        bestSize = size;
#endif
      swapx = x;
      swapy = y;
      y = x;
      x = cuddNextLow(table, y);
    }
    y = ytop + i;
    x = cuddNextLow(table, y);
  }
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  if ((bestSize < initialSize) && (bestSize < size))
    (void)fprintf(
        table->out,
        "Missed local minimum: initialSize:%d  bestSize:%d  finalSize:%d\n",
        initialSize, bestSize, size);
#endif

  /* fix groups */
  y = xtop; /* ytop is now where xtop used to be */
  for (i = 0; i < ysize - 1; i++) {
    table->subtables[y].next = cuddNextHigh(table, y);
    y = cuddNextHigh(table, y);
  }
  table->subtables[y].next = xtop; /* y is bottom of its group, join */
  /* it to top of its group */
  x = cuddNextHigh(table, y);
  newxtop = x;
  for (i = 0; i < xsize - 1; i++) {
    table->subtables[x].next = cuddNextHigh(table, x);
    x = cuddNextHigh(table, x);
  }
  table->subtables[x].next = newxtop; /* x is bottom of its group, join */
                                      /* it to top of its group */
#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "ddGroupMove:\n");
#endif

  /* Store group move */
  move = (Move *)cuddDynamicAllocNode(table);
  if (move == NULL)
    goto ddGroupMoveOutOfMem;
  move->x = swapx;
  move->y = swapy;
  move->flags = MTR_DEFAULT;
  move->size = table->keys - table->isolated;
  move->next = *moves;
  *moves = move;

  return (table->keys - table->isolated);

ddGroupMoveOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }
  return (0);

} /* end of ddGroupMove */

/**Function********************************************************************

  Synopsis    [Undoes the swap two groups.]

  Description [Undoes the swap two groups.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddGroupMoveBackward(DdManager *table, int x, int y) {
  int size;
  int i, j, xtop, xbot, xsize, ytop, ybot, ysize, newxtop;

#ifdef DD_DEBUG
  /* We assume that x < y */
  assert(x < y);
#endif

  /* Find top, bottom, and size for the two groups. */
  xbot = x;
  xtop = table->subtables[x].next;
  xsize = xbot - xtop + 1;
  ybot = y;
  while ((unsigned)ybot < table->subtables[ybot].next)
    ybot = table->subtables[ybot].next;
  ytop = y;
  ysize = ybot - ytop + 1;

  /* Sift the variables of the second group up through the first group */
  for (i = 1; i <= ysize; i++) {
    for (j = 1; j <= xsize; j++) {
      size = cuddSwapInPlace(table, x, y);
      if (size == 0)
        return (0);
      y = x;
      x = cuddNextLow(table, y);
    }
    y = ytop + i;
    x = cuddNextLow(table, y);
  }

  /* fix groups */
  y = xtop;
  for (i = 0; i < ysize - 1; i++) {
    table->subtables[y].next = cuddNextHigh(table, y);
    y = cuddNextHigh(table, y);
  }
  table->subtables[y].next = xtop; /* y is bottom of its group, join */
  /* to its top */
  x = cuddNextHigh(table, y);
  newxtop = x;
  for (i = 0; i < xsize - 1; i++) {
    table->subtables[x].next = cuddNextHigh(table, x);
    x = cuddNextHigh(table, x);
  }
  table->subtables[x].next = newxtop; /* x is bottom of its group, join */
                                      /* to its top */
#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "ddGroupMoveBackward:\n");
#endif

  return (1);

} /* end of ddGroupMoveBackward */

/**Function********************************************************************

  Synopsis    [Determines the best position for a variables and returns
  it there.]

  Description [Determines the best position for a variables and returns
  it there.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddGroupSiftingBackward(DdManager *table, Move *moves, int size,
                                  int upFlag, int lazyFlag) {
  Move *move;
  int res;
  Move *end_move;
  int diff, tmp_diff;
  int index;
  unsigned int pairlev;

  if (lazyFlag) {
    end_move = NULL;

    /* Find the minimum size, and the earliest position at which it
    ** was achieved. */
    for (move = moves; move != NULL; move = move->next) {
      if (move->size < size) {
        size = move->size;
        end_move = move;
      } else if (move->size == size) {
        if (end_move == NULL)
          end_move = move;
      }
    }

    /* Find among the moves that give minimum size the one that
    ** minimizes the distance from the corresponding variable. */
    if (moves != NULL) {
      diff = Cudd_ReadSize(table) + 1;
      index =
          (upFlag == 1) ? table->invperm[moves->x] : table->invperm[moves->y];
      pairlev = (unsigned)table->perm[Cudd_bddReadPairIndex(table, index)];

      for (move = moves; move != NULL; move = move->next) {
        if (move->size == size) {
          if (upFlag == 1) {
            tmp_diff =
                (move->x > pairlev) ? move->x - pairlev : pairlev - move->x;
          } else {
            tmp_diff =
                (move->y > pairlev) ? move->y - pairlev : pairlev - move->y;
          }
          if (tmp_diff < diff) {
            diff = tmp_diff;
            end_move = move;
          }
        }
      }
    }
  } else {
    /* Find the minimum size. */
    for (move = moves; move != NULL; move = move->next) {
      if (move->size < size) {
        size = move->size;
      }
    }
  }

  /* In case of lazy sifting, end_move identifies the position at
  ** which we want to stop.  Otherwise, we stop as soon as we meet
  ** the minimum size. */
  for (move = moves; move != NULL; move = move->next) {
    if (lazyFlag) {
      if (move == end_move)
        return (1);
    } else {
      if (move->size == size)
        return (1);
    }
    if ((table->subtables[move->x].next == move->x) &&
        (table->subtables[move->y].next == move->y)) {
      res = cuddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
#ifdef DD_DEBUG
      if (pr > 0)
        (void)fprintf(table->out, "ddGroupSiftingBackward:\n");
      assert(table->subtables[move->x].next == move->x);
      assert(table->subtables[move->y].next == move->y);
#endif
    } else { /* Group move necessary */
      if (move->flags == MTR_NEWNODE) {
        ddDissolveGroup(table, (int)move->x, (int)move->y);
      } else {
        res = ddGroupMoveBackward(table, (int)move->x, (int)move->y);
        if (!res)
          return (0);
      }
    }
  }

  return (1);

} /* end of ddGroupSiftingBackward */

/**Function********************************************************************

  Synopsis    [Merges groups in the DD table.]

  Description [Creates a single group from low to high and adjusts the
  index field of the tree node.]

  SideEffects [None]

******************************************************************************/
static void ddMergeGroups(DdManager *table, MtrNode *treenode, int low,
                          int high) {
  int i;
  MtrNode *auxnode;
  int saveindex;
  int newindex;

  /* Merge all variables from low to high in one group, unless
  ** this is the topmost group. In such a case we do not merge lest
  ** we lose the symmetry information. */
  if (treenode != table->tree) {
    for (i = low; i < high; i++)
      table->subtables[i].next = i + 1;
    table->subtables[high].next = low;
  }

  /* Adjust the index fields of the tree nodes. If a node is the
  ** first child of its parent, then the parent may also need adjustment. */
  saveindex = treenode->index;
  newindex = table->invperm[low];
  auxnode = treenode;
  do {
    auxnode->index = newindex;
    if (auxnode->parent == NULL || (int)auxnode->parent->index != saveindex)
      break;
    auxnode = auxnode->parent;
  } while (1);
  return;

} /* end of ddMergeGroups */

/**Function********************************************************************

  Synopsis    [Dissolves a group in the DD table.]

  Description [x and y are variables in a group to be cut in two. The cut
  is to pass between x and y.]

  SideEffects [None]

******************************************************************************/
static void ddDissolveGroup(DdManager *table, int x, int y) {
  int topx;
  int boty;

  /* find top and bottom of the two groups */
  boty = y;
  while ((unsigned)boty < table->subtables[boty].next)
    boty = table->subtables[boty].next;

  topx = table->subtables[boty].next;

  table->subtables[boty].next = y;
  table->subtables[x].next = topx;

  return;

} /* end of ddDissolveGroup */

/**Function********************************************************************

  Synopsis    [Pretends to check two variables for aggregation.]

  Description [Pretends to check two variables for aggregation. Always
  returns 0.]

  SideEffects [None]

******************************************************************************/
static int ddNoCheck(DdManager *table, int x, int y) {
  return (0);

} /* end of ddNoCheck */

/**Function********************************************************************

  Synopsis    [Checks two variables for aggregation.]

  Description [Checks two variables for aggregation. The check is based
  on the second difference of the number of nodes as a function of the
  layer. If the second difference is lower than a given threshold
  (typically negative) then the two variables should be aggregated.
  Returns 1 if the two variables pass the test; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSecDiffCheck(DdManager *table, int x, int y) {
  double Nx, Nx_1;
  double Sx;
  double threshold;
  int xindex, yindex;

  if (x == 0)
    return (0);

#ifdef DD_STATS
  secdiffcalls++;
#endif
  Nx = (double)table->subtables[x].keys;
  Nx_1 = (double)table->subtables[x - 1].keys;
  Sx = (table->subtables[y].keys / Nx) - (Nx / Nx_1);

  threshold = table->recomb / 100.0;
  if (Sx < threshold) {
    xindex = table->invperm[x];
    yindex = table->invperm[y];
    if (cuddTestInteract(table, xindex, yindex)) {
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
      (void)fprintf(table->out, "Second difference for %d = %g Pos(%d)\n",
                    table->invperm[x], Sx, x);
#endif
#ifdef DD_STATS
      secdiff++;
#endif
      return (1);
    } else {
#ifdef DD_STATS
      secdiffmisfire++;
#endif
      return (0);
    }
  }
  return (0);

} /* end of ddSecDiffCheck */

/**Function********************************************************************

  Synopsis    [Checks for extended symmetry of x and y.]

  Description [Checks for extended symmetry of x and y. Returns 1 in
  case of extended symmetry; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddExtSymmCheck(DdManager *table, int x, int y) {
  DdNode *f, *f0, *f1, *f01, *f00, *f11, *f10;
  DdNode *one;
  unsigned comple;   /* f0 is complemented */
  int notproj;       /* f is not a projection function */
  int arccount;      /* number of arcs from layer x to layer y */
  int TotalRefCount; /* total reference count of layer y minus 1 */
  int counter;       /* number of nodes of layer x that are allowed */
  /* to violate extended symmetry conditions */
  int arccounter; /* number of arcs into layer y that are allowed */
  /* to come from layers other than x */
  int i;
  int xindex;
  int yindex;
  int res;
  int slots;
  DdNodePtr *list;
  DdNode *sentinel = &(table->sentinel);

  xindex = table->invperm[x];
  yindex = table->invperm[y];

  /* If the two variables do not interact, we do not want to merge them. */
  if (!cuddTestInteract(table, xindex, yindex))
    return (0);

#ifdef DD_DEBUG
  /* Checks that x and y do not contain just the projection functions.
  ** With the test on interaction, these test become redundant,
  ** because an isolated projection function does not interact with
  ** any other variable.
  */
  if (table->subtables[x].keys == 1) {
    assert(table->vars[xindex]->ref != 1);
  }
  if (table->subtables[y].keys == 1) {
    assert(table->vars[yindex]->ref != 1);
  }
#endif

#ifdef DD_STATS
  extsymmcalls++;
#endif

  arccount = 0;
  counter =
      (int)(table->subtables[x].keys * (table->symmviolation / 100.0) + 0.5);
  one = DD_ONE(table);

  slots = table->subtables[x].slots;
  list = table->subtables[x].nodelist;
  for (i = 0; i < slots; i++) {
    f = list[i];
    while (f != sentinel) {
      /* Find f1, f0, f11, f10, f01, f00. */
      f1 = cuddT(f);
      f0 = Cudd_Regular(cuddE(f));
      comple = Cudd_IsComplement(cuddE(f));
      notproj = f1 != one || f0 != one || f->ref != (DdHalfWord)1;
      if (f1->index == (unsigned)yindex) {
        arccount++;
        f11 = cuddT(f1);
        f10 = cuddE(f1);
      } else {
        if ((int)f0->index != yindex) {
          /* If f is an isolated projection function it is
          ** allowed to bypass layer y.
          */
          if (notproj) {
            if (counter == 0)
              return (0);
            counter--; /* f bypasses layer y */
          }
        }
        f11 = f10 = f1;
      }
      if ((int)f0->index == yindex) {
        arccount++;
        f01 = cuddT(f0);
        f00 = cuddE(f0);
      } else {
        f01 = f00 = f0;
      }
      if (comple) {
        f01 = Cudd_Not(f01);
        f00 = Cudd_Not(f00);
      }

      /* Unless we are looking at a projection function
      ** without external references except the one from the
      ** table, we insist that f01 == f10 or f11 == f00
      */
      if (notproj) {
        if (f01 != f10 && f11 != f00) {
          if (counter == 0)
            return (0);
          counter--;
        }
      }

      f = f->next;
    } /* while */
  }   /* for */

  /* Calculate the total reference counts of y */
  TotalRefCount = -1; /* -1 for projection function */
  slots = table->subtables[y].slots;
  list = table->subtables[y].nodelist;
  for (i = 0; i < slots; i++) {
    f = list[i];
    while (f != sentinel) {
      TotalRefCount += f->ref;
      f = f->next;
    }
  }

  arccounter =
      (int)(table->subtables[y].keys * (table->arcviolation / 100.0) + 0.5);
  res = arccount >= TotalRefCount - arccounter;

#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  if (res) {
    (void)fprintf(table->out,
                  "Found extended symmetry! x = %d\ty = %d\tPos(%d,%d)\n",
                  xindex, yindex, x, y);
  }
#endif

#ifdef DD_STATS
  if (res)
    extsymm++;
#endif
  return (res);

} /* end ddExtSymmCheck */

/**Function********************************************************************

  Synopsis    [Checks for grouping of x and y.]

  Description [Checks for grouping of x and y. Returns 1 in
  case of grouping; 0 otherwise. This function is used for lazy sifting.]

  SideEffects [None]

******************************************************************************/
static int ddVarGroupCheck(DdManager *table, int x, int y) {
  int xindex = table->invperm[x];
  int yindex = table->invperm[y];

  if (Cudd_bddIsVarToBeUngrouped(table, xindex))
    return (0);

  if (Cudd_bddReadPairIndex(table, xindex) == yindex) {
    if (ddIsVarHandled(table, xindex) || ddIsVarHandled(table, yindex)) {
      if (Cudd_bddIsVarToBeGrouped(table, xindex) ||
          Cudd_bddIsVarToBeGrouped(table, yindex)) {
        if (table->keys - table->isolated <= originalSize) {
          return (1);
        }
      }
    }
  }

  return (0);

} /* end of ddVarGroupCheck */

/**Function********************************************************************

  Synopsis    [Sets a variable to already handled.]

  Description [Sets a variable to already handled. This function is used
  for lazy sifting.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
static int ddSetVarHandled(DdManager *dd, int index) {
  if (index >= dd->size || index < 0)
    return (0);
  dd->subtables[dd->perm[index]].varHandled = 1;
  return (1);

} /* end of ddSetVarHandled */

/**Function********************************************************************

  Synopsis    [Resets a variable to be processed.]

  Description [Resets a variable to be processed. This function is used
  for lazy sifting.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
static int ddResetVarHandled(DdManager *dd, int index) {
  if (index >= dd->size || index < 0)
    return (0);
  dd->subtables[dd->perm[index]].varHandled = 0;
  return (1);

} /* end of ddResetVarHandled */

/**Function********************************************************************

  Synopsis    [Checks whether a variables is already handled.]

  Description [Checks whether a variables is already handled. This
  function is used for lazy sifting.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
static int ddIsVarHandled(DdManager *dd, int index) {
  if (index >= dd->size || index < 0)
    return (-1);
  return dd->subtables[dd->perm[index]].varHandled;

} /* end of ddIsVarHandled */

/**CFile***********************************************************************

  FileName    [cuddInit.c]

  PackageName [cudd]

  Synopsis    [Functions to initialize and shut down the DD manager.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_Init()
                <li> Cudd_Quit()
                </ul>
               Internal procedures included in this module:
                <ul>
                <li> cuddZddInitUniv()
                <li> cuddZddFreeUniv()
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
// static char rcsid[] DD_UNUSED = "$Id: cuddInit.c,v 1.34 2012/02/05 01:07:19
// fabio Exp $"; #endif

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

  Synopsis    [Creates a new DD manager.]

  Description [Creates a new DD manager, initializes the table, the
  basic constants and the projection functions. If maxMemory is 0,
  Cudd_Init decides suitable values for the maximum size of the cache
  and for the limit for fast unique table growth based on the available
  memory. Returns a pointer to the manager if successful; NULL
  otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Quit]

******************************************************************************/
DdManager *
Cudd_Init(unsigned int
              numVars /* initial number of BDD variables (i.e., subtables) */,
          unsigned int
              numVarsZ /* initial number of ZDD variables (i.e., subtables) */,
          unsigned int numSlots /* initial size of the unique tables */,
          unsigned int cacheSize /* initial size of the cache */,
          unsigned long maxMemory /* target maximum memory occupation */) {
  DdManager *unique;
  int i, result;
  DdNode *one, *zero;
  unsigned int maxCacheSize;
  unsigned int looseUpTo;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  if (maxMemory == 0) {
    maxMemory = getSoftDataLimit();
  }
  looseUpTo =
      (unsigned int)((maxMemory / sizeof(DdNode)) / DD_MAX_LOOSE_FRACTION);
  unique = cuddInitTable(numVars, numVarsZ, numSlots, looseUpTo);
  if (unique == NULL)
    return (NULL);
  unique->maxmem = (unsigned long)maxMemory / 10 * 9;
  maxCacheSize =
      (unsigned int)((maxMemory / sizeof(DdCache)) / DD_MAX_CACHE_FRACTION);
  result = cuddInitCache(unique, cacheSize, maxCacheSize);
  if (result == 0)
    return (NULL);

  saveHandler = MMoutOfMemory;
  MMoutOfMemory = Cudd_OutOfMem;
  unique->stash = ALLOC(char, (maxMemory / DD_STASH_FRACTION) + 4);
  MMoutOfMemory = saveHandler;
  if (unique->stash == NULL) {
    (void)fprintf(unique->err, "Unable to set aside memory\n");
  }

  /* Initialize constants. */
  unique->one = cuddUniqueConst(unique, 1.0);
  if (unique->one == NULL)
    return (0);
  cuddRef(unique->one);
  unique->zero = cuddUniqueConst(unique, 0.0);
  if (unique->zero == NULL)
    return (0);
  cuddRef(unique->zero);
#ifdef HAVE_IEEE_754
  if (DD_PLUS_INF_VAL != DD_PLUS_INF_VAL * 3 ||
      DD_PLUS_INF_VAL != DD_PLUS_INF_VAL / 3) {
    (void)fprintf(unique->err, "Warning: Crippled infinite values\n");
    (void)fprintf(unique->err, "Recompile without -DHAVE_IEEE_754\n");
  }
#endif
  unique->plusinfinity = cuddUniqueConst(unique, DD_PLUS_INF_VAL);
  if (unique->plusinfinity == NULL)
    return (0);
  cuddRef(unique->plusinfinity);
  unique->minusinfinity = cuddUniqueConst(unique, DD_MINUS_INF_VAL);
  if (unique->minusinfinity == NULL)
    return (0);
  cuddRef(unique->minusinfinity);
  unique->background = unique->zero;

  /* The logical zero is different from the CUDD_VALUE_TYPE zero! */
  one = unique->one;
  zero = Cudd_Not(one);
  /* Create the projection functions. */
  unique->vars = ALLOC(DdNodePtr, unique->maxSize);
  if (unique->vars == NULL) {
    unique->errorCode = CUDD_MEMORY_OUT;
    return (NULL);
  }
  for (i = 0; i < unique->size; i++) {
    unique->vars[i] = cuddUniqueInter(unique, i, one, zero);
    if (unique->vars[i] == NULL)
      return (0);
    cuddRef(unique->vars[i]);
  }

  if (unique->sizeZ)
    cuddZddInitUniv(unique);

  unique->memused += sizeof(DdNode *) * unique->maxSize;

  return (unique);

} /* end of Cudd_Init */

/**Function********************************************************************

  Synopsis    [Deletes resources associated with a DD manager.]

  Description [Deletes resources associated with a DD manager and
  resets the global statistical counters. (Otherwise, another manaqger
  subsequently created would inherit the stats of this one.)]

  SideEffects [None]

  SeeAlso     [Cudd_Init]

******************************************************************************/
void Cudd_Quit(DdManager *unique) {
  if (unique->stash != NULL)
    FREE(unique->stash);
  cuddFreeTable(unique);

} /* end of Cudd_Quit */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Initializes the ZDD universe.]

  Description [Initializes the ZDD universe. Returns 1 if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddZddFreeUniv]

******************************************************************************/
int cuddZddInitUniv(DdManager *zdd) {
  DdNode *p, *res;
  int i;

  zdd->univ = ALLOC(DdNodePtr, zdd->sizeZ);
  if (zdd->univ == NULL) {
    zdd->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }

  res = DD_ONE(zdd);
  cuddRef(res);
  for (i = zdd->sizeZ - 1; i >= 0; i--) {
    unsigned int index = zdd->invpermZ[i];
    p = res;
    res = cuddUniqueInterZdd(zdd, index, p, p);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(zdd, p);
      FREE(zdd->univ);
      return (0);
    }
    cuddRef(res);
    cuddDeref(p);
    zdd->univ[i] = res;
  }

#ifdef DD_VERBOSE
  cuddZddP(zdd, zdd->univ[0]);
#endif

  return (1);

} /* end of cuddZddInitUniv */

/**Function********************************************************************

  Synopsis    [Frees the ZDD universe.]

  Description [Frees the ZDD universe.]

  SideEffects [None]

  SeeAlso     [cuddZddInitUniv]

******************************************************************************/
void cuddZddFreeUniv(DdManager *zdd) {
  if (zdd->univ) {
    Cudd_RecursiveDerefZdd(zdd, zdd->univ[0]);
    FREE(zdd->univ);
  }

} /* end of cuddZddFreeUniv */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**CFile***********************************************************************

  FileName    [cuddInteract.c]

  PackageName [cudd]

  Synopsis    [Functions to manipulate the variable interaction matrix.]

  Description [Internal procedures included in this file:
        <ul>
        <li> cuddSetInteract()
        <li> cuddTestInteract()
        <li> cuddInitInteract()
        </ul>
  Static procedures included in this file:
        <ul>
        <li> ddSuppInteract()
        <li> ddClearLocal()
        <li> ddUpdateInteract()
        <li> ddClearGlobal()
        </ul>
  The interaction matrix tells whether two variables are
  both in the support of some function of the DD. The main use of the
  interaction matrix is in the in-place swapping. Indeed, if two
  variables do not interact, there is no arc connecting the two layers;
  therefore, the swap can be performed in constant time, without
  scanning the subtables. Another use of the interaction matrix is in
  the computation of the lower bounds for sifting. Finally, the
  interaction matrix can be used to speed up aggregation checks in
  symmetric and group sifting.<p>
  The computation of the interaction matrix is done with a series of
  depth-first searches. The searches start from those nodes that have
  only external references. The matrix is stored as a packed array of bits;
  since it is symmetric, only the upper triangle is kept in memory.
  As a final remark, we note that there may be variables that do
  interact, but that for a given variable order have no arc connecting
  their layers when they are adjacent.  For instance, in ite(a,b,c) with
  the order a<b<c, b and c interact, but are not connected.]

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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#if SIZEOF_LONG == 8
#define BPL 64
#define LOGBPL 6
#else
#define BPL 32
#define LOGBPL 5
#endif

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
// static char rcsid[] DD_UNUSED = "$Id: cuddInteract.c,v 1.14 2012/02/05
// 01:07:19 fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void ddSuppInteract(DdNode *f, char *support);
static void ddClearLocal(DdNode *f);
static void ddUpdateInteract(DdManager *table, char *support);
static void ddClearGlobal2(DdManager *table);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Set interaction matrix entries.]

  Description [Given a pair of variables 0 <= x < y < table->size,
  sets the corresponding bit of the interaction matrix to 1.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddSetInteract(DdManager *table, int x, int y) {
  int posn, word, bit;

#ifdef DD_DEBUG
  assert(x < y);
  assert(y < table->size);
  assert(x >= 0);
#endif

  posn = ((((table->size << 1) - x - 3) * x) >> 1) + y - 1;
  word = posn >> LOGBPL;
  bit = posn & (BPL - 1);
  table->interact[word] |= 1L << bit;

} /* end of cuddSetInteract */

/**Function********************************************************************

  Synopsis    [Test interaction matrix entries.]

  Description [Given a pair of variables 0 <= x < y < table->size,
  tests whether the corresponding bit of the interaction matrix is 1.
  Returns the value of the bit.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddTestInteract(DdManager *table, int x, int y) {
  int posn, word, bit, result;

  if (x > y) {
    int tmp = x;
    x = y;
    y = tmp;
  }
#ifdef DD_DEBUG
  assert(x < y);
  assert(y < table->size);
  assert(x >= 0);
#endif

  posn = ((((table->size << 1) - x - 3) * x) >> 1) + y - 1;
  word = posn >> LOGBPL;
  bit = posn & (BPL - 1);
  result = (table->interact[word] >> bit) & 1L;
  return (result);

} /* end of cuddTestInteract */

/**Function********************************************************************

  Synopsis    [Initializes the interaction matrix.]

  Description [Initializes the interaction matrix. The interaction
  matrix is implemented as a bit vector storing the upper triangle of
  the symmetric interaction matrix. The bit vector is kept in an array
  of long integers. The computation is based on a series of depth-first
  searches, one for each root of the DAG. Two flags are needed: The
  local visited flag uses the LSB of the then pointer. The global
  visited flag uses the LSB of the next pointer.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddInitInteract(DdManager *table) {
  int i, j;
  unsigned long words;
  long *interact;
  char *support;
  DdNode *f;
  DdNode *sentinel = &(table->sentinel);
  DdNodePtr *nodelist;
  int slots;
  unsigned long n = (unsigned long)table->size;

  words = ((n * (n - 1)) >> (1 + LOGBPL)) + 1;
  table->interact = interact = ALLOC(long, words);
  if (interact == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (i = 0; i < words; i++) {
    interact[i] = 0;
  }

  support = ALLOC(char, n);
  if (support == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    FREE(interact);
    return (0);
  }
  for (i = 0; i < n; i++) {
    support[i] = 0;
  }

  for (i = 0; i < n; i++) {
    nodelist = table->subtables[i].nodelist;
    slots = table->subtables[i].slots;
    for (j = 0; j < slots; j++) {
      f = nodelist[j];
      while (f != sentinel) {
        /* A node is a root of the DAG if it cannot be
        ** reached by nodes above it. If a node was never
        ** reached during the previous depth-first searches,
        ** then it is a root, and we start a new depth-first
        ** search from it.
        */
        if (!Cudd_IsComplement(f->next)) {
          ddSuppInteract(f, support);
          ddClearLocal(f);
          ddUpdateInteract(table, support);
        }
        f = Cudd_Regular(f->next);
      }
    }
  }
  ddClearGlobal2(table);

  FREE(support);
  return (1);

} /* end of cuddInitInteract */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Find the support of f.]

  Description [Performs a DFS from f. Uses the LSB of the then pointer
  as visited flag.]

  SideEffects [Accumulates in support the variables on which f depends.]

  SeeAlso     []

******************************************************************************/
static void ddSuppInteract(DdNode *f, char *support) {
  if (cuddIsConstant(f) || Cudd_IsComplement(cuddT(f))) {
    return;
  }

  support[f->index] = 1;
  ddSuppInteract(cuddT(f), support);
  ddSuppInteract(Cudd_Regular(cuddE(f)), support);
  /* mark as visited */
  cuddT(f) = Cudd_Complement(cuddT(f));
  f->next = Cudd_Complement(f->next);
  return;

} /* end of ddSuppInteract */

/**Function********************************************************************

  Synopsis    [Performs a DFS from f, clearing the LSB of the then pointers.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void ddClearLocal(DdNode *f) {
  if (cuddIsConstant(f) || !Cudd_IsComplement(cuddT(f))) {
    return;
  }
  /* clear visited flag */
  cuddT(f) = Cudd_Regular(cuddT(f));
  ddClearLocal(cuddT(f));
  ddClearLocal(Cudd_Regular(cuddE(f)));
  return;

} /* end of ddClearLocal */

/**Function********************************************************************

  Synopsis [Marks as interacting all pairs of variables that appear in
  support.]

  Description [If support[i] == support[j] == 1, sets the (i,j) entry
  of the interaction matrix to 1.]

  SideEffects [Clears support.]

  SeeAlso     []

******************************************************************************/
static void ddUpdateInteract(DdManager *table, char *support) {
  int i, j;
  int n = table->size;

  for (i = 0; i < n - 1; i++) {
    if (support[i] == 1) {
      support[i] = 0;
      for (j = i + 1; j < n; j++) {
        if (support[j] == 1) {
          cuddSetInteract(table, i, j);
        }
      }
    }
  }
  support[n - 1] = 0;

} /* end of ddUpdateInteract */

/**Function********************************************************************

  Synopsis    [Scans the DD and clears the LSB of the next pointers.]

  Description [The LSB of the next pointers are used as markers to tell
  whether a node was reached by at least one DFS. Once the interaction
  matrix is built, these flags are reset.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void ddClearGlobal2(DdManager *table) {
  int i, j;
  DdNode *f;
  DdNode *sentinel = &(table->sentinel);
  DdNodePtr *nodelist;
  int slots;

  for (i = 0; i < table->size; i++) {
    nodelist = table->subtables[i].nodelist;
    slots = table->subtables[i].slots;
    for (j = 0; j < slots; j++) {
      f = nodelist[j];
      while (f != sentinel) {
        f->next = Cudd_Regular(f->next);
        f = f->next;
      }
    }
  }

} /* end of ddClearGlobal */

/**CFile***********************************************************************

  FileName    [cuddLCache.c]

  PackageName [cudd]

  Synopsis    [Functions for local caches.]

  Description [Internal procedures included in this module:
                <ul>
                <li> cuddLocalCacheInit()
                <li> cuddLocalCacheQuit()
                <li> cuddLocalCacheInsert()
                <li> cuddLocalCacheLookup()
                <li> cuddLocalCacheClearDead()
                <li> cuddLocalCacheClearAll()
                <li> cuddLocalCacheProfile()
                <li> cuddHashTableInit()
                <li> cuddHashTableQuit()
                <li> cuddHashTableGenericQuit()
                <li> cuddHashTableInsert()
                <li> cuddHashTableLookup()
                <li> cuddHashTableGenericInsert()
                <li> cuddHashTableGenericLookup()
                <li> cuddHashTableInsert2()
                <li> cuddHashTableLookup2()
                <li> cuddHashTableInsert3()
                <li> cuddHashTableLookup3()
                </ul>
            Static procedures included in this module:
                <ul>
                <li> cuddLocalCacheResize()
                <li> ddLCHash()
                <li> cuddLocalCacheAddToList()
                <li> cuddLocalCacheRemoveFromList()
                <li> cuddHashTableResize()
                <li> cuddHashTableAlloc()
                </ul> ]

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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DD_MAX_HASHTABLE_DENSITY 2 /* tells when to resize a table */

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
// static char rcsid[] DD_UNUSED = "$Id: cuddLCache.c,v 1.27 2012/02/05 01:07:19
// fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**Macro***********************************************************************

  Synopsis    [Computes hash function for keys of one operand.]

  Description []

  SideEffects [None]

  SeeAlso     [ddLCHash3 ddLCHash]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddLCHash1(f, shift) (((unsigned)(ptruint)(f)*DD_P1) >> (shift))
#else
#define ddLCHash1(f, shift) (((unsigned)(f)*DD_P1) >> (shift))
#endif

/**Macro***********************************************************************

  Synopsis    [Computes hash function for keys of two operands.]

  Description []

  SideEffects [None]

  SeeAlso     [ddLCHash3 ddLCHash]

******************************************************************************/
#if SIZEOF_VOID_P == 8 && SIZEOF_INT == 4
#define ddLCHash2(f, g, shift)                                                 \
  ((((unsigned)(ptruint)(f)*DD_P1 + (unsigned)(ptruint)(g)) * DD_P2) >> (shift))
#else
#define ddLCHash2(f, g, shift)                                                 \
  ((((unsigned)(f)*DD_P1 + (unsigned)(g)) * DD_P2) >> (shift))
#endif

/**Macro***********************************************************************

  Synopsis    [Computes hash function for keys of three operands.]

  Description []

  SideEffects [None]

  SeeAlso     [ddLCHash2 ddLCHash]

******************************************************************************/
#define ddLCHash3(f, g, h, shift) ddCHash2(f, g, h, shift)

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void cuddLocalCacheResize(DdLocalCache *cache);
DD_INLINE static unsigned int ddLCHash(DdNodePtr *key, unsigned int keysize,
                                       int shift);
static void cuddLocalCacheAddToList(DdLocalCache *cache);
static void cuddLocalCacheRemoveFromList(DdLocalCache *cache);
static int cuddHashTableResize(DdHashTable *hash);
DD_INLINE static DdHashItem *cuddHashTableAlloc(DdHashTable *hash);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Clears the dead entries of the local caches of a manager.]

  Description [Clears the dead entries of the local caches of a manager.
  Used during garbage collection.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddLocalCacheClearDead(DdManager *manager) {
  DdLocalCache *cache = manager->localCaches;
  unsigned int keysize;
  unsigned int itemsize;
  unsigned int slots;
  DdLocalCacheItem *item;
  DdNodePtr *key;
  unsigned int i, j;

  while (cache != NULL) {
    keysize = cache->keysize;
    itemsize = cache->itemsize;
    slots = cache->slots;
    item = cache->item;
    for (i = 0; i < slots; i++) {
      if (item->value != NULL) {
        if (Cudd_Regular(item->value)->ref == 0) {
          item->value = NULL;
        } else {
          key = item->key;
          for (j = 0; j < keysize; j++) {
            if (Cudd_Regular(key[j])->ref == 0) {
              item->value = NULL;
              break;
            }
          }
        }
      }
      item = (DdLocalCacheItem *)((char *)item + itemsize);
    }
    cache = cache->next;
  }
  return;

} /* end of cuddLocalCacheClearDead */

/**Function********************************************************************

  Synopsis [Clears the local caches of a manager.]

  Description [Clears the local caches of a manager.
  Used before reordering.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddLocalCacheClearAll(DdManager *manager) {
  DdLocalCache *cache = manager->localCaches;

  while (cache != NULL) {
    memset(cache->item, 0, cache->slots * cache->itemsize);
    cache = cache->next;
  }
  return;

} /* end of cuddLocalCacheClearAll */

#ifdef DD_CACHE_PROFILE

#define DD_HYSTO_BINS 8

/**Function********************************************************************

  Synopsis    [Computes and prints a profile of a local cache usage.]

  Description [Computes and prints a profile of a local cache usage.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddLocalCacheProfile(DdLocalCache *cache) {
  double count, mean, meansq, stddev, expected;
  long max, min;
  int imax, imin;
  int i, retval, slots;
  long *hystogram;
  int nbins = DD_HYSTO_BINS;
  int bin;
  long thiscount;
  double totalcount;
  int nzeroes;
  DdLocalCacheItem *entry;
  FILE *fp = cache->manager->out;

  slots = cache->slots;

  meansq = mean = expected = 0.0;
  max = min = (long)cache->item[0].count;
  imax = imin = nzeroes = 0;
  totalcount = 0.0;

  hystogram = ALLOC(long, nbins);
  if (hystogram == NULL) {
    return (0);
  }
  for (i = 0; i < nbins; i++) {
    hystogram[i] = 0;
  }

  for (i = 0; i < slots; i++) {
    entry = (DdLocalCacheItem *)((char *)cache->item + i * cache->itemsize);
    thiscount = (long)entry->count;
    if (thiscount > max) {
      max = thiscount;
      imax = i;
    }
    if (thiscount < min) {
      min = thiscount;
      imin = i;
    }
    if (thiscount == 0) {
      nzeroes++;
    }
    count = (double)thiscount;
    mean += count;
    meansq += count * count;
    totalcount += count;
    expected += count * (double)i;
    bin = (i * nbins) / slots;
    hystogram[bin] += thiscount;
  }
  mean /= (double)slots;
  meansq /= (double)slots;
  stddev = sqrt(meansq - mean * mean);

  retval = fprintf(fp, "Cache stats: slots = %d average = %g ", slots, mean);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "standard deviation = %g\n", stddev);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "Cache max accesses = %ld for slot %d\n", max, imax);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "Cache min accesses = %ld for slot %d\n", min, imin);
  if (retval == EOF)
    return (0);
  retval = fprintf(fp, "Cache unused slots = %d\n", nzeroes);
  if (retval == EOF)
    return (0);

  if (totalcount) {
    expected /= totalcount;
    retval = fprintf(fp, "Cache access hystogram for %d bins", nbins);
    if (retval == EOF)
      return (0);
    retval = fprintf(fp, " (expected bin value = %g)\n# ", expected);
    if (retval == EOF)
      return (0);
    for (i = nbins - 1; i >= 0; i--) {
      retval = fprintf(fp, "%ld ", hystogram[i]);
      if (retval == EOF)
        return (0);
    }
    retval = fprintf(fp, "\n");
    if (retval == EOF)
      return (0);
  }

  FREE(hystogram);
  return (1);

} /* end of cuddLocalCacheProfile */
#endif

/**Function********************************************************************

  Synopsis    [Initializes a hash table.]

  Description [Initializes a hash table. Returns a pointer to the new
  table if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableQuit]

******************************************************************************/
DdHashTable *cuddHashTableInit(DdManager *manager, unsigned int keySize,
                               unsigned int initSize) {
  DdHashTable *hash;
  int logSize;

  hash = ALLOC(DdHashTable, 1);
  if (hash == NULL) {
    manager->errorCode = CUDD_MEMORY_OUT;
    return (NULL);
  }
  hash->keysize = keySize;
  hash->manager = manager;
  hash->memoryList = NULL;
  hash->nextFree = NULL;
  hash->itemsize =
      (keySize + 1) * sizeof(DdNode *) + sizeof(ptrint) + sizeof(DdHashItem *);
  /* We have to guarantee that the shift be < 32. */
  if (initSize < 2)
    initSize = 2;
  logSize = cuddComputeFloorLog2(initSize);
  hash->numBuckets = 1 << logSize;
  hash->shift = sizeof(int) * 8 - logSize;
  hash->bucket = ALLOC(DdHashItem *, hash->numBuckets);
  if (hash->bucket == NULL) {
    manager->errorCode = CUDD_MEMORY_OUT;
    FREE(hash);
    return (NULL);
  }
  memset(hash->bucket, 0, hash->numBuckets * sizeof(DdHashItem *));
  hash->size = 0;
  hash->maxsize = hash->numBuckets * DD_MAX_HASHTABLE_DENSITY;
  return (hash);

} /* end of cuddHashTableInit */

/**Function********************************************************************

  Synopsis    [Shuts down a hash table.]

  Description [Shuts down a hash table, dereferencing all the values.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInit]

******************************************************************************/
void cuddHashTableQuit(DdHashTable *hash) {
  unsigned int i;
  DdManager *dd = hash->manager;
  DdHashItem *bucket;
  DdHashItem **memlist, **nextmem;
  unsigned int numBuckets = hash->numBuckets;

  for (i = 0; i < numBuckets; i++) {
    bucket = hash->bucket[i];
    while (bucket != NULL) {
      Cudd_RecursiveDeref(dd, bucket->value);
      bucket = bucket->next;
    }
  }

  memlist = hash->memoryList;
  while (memlist != NULL) {
    nextmem = (DdHashItem **)memlist[0];
    FREE(memlist);
    memlist = nextmem;
  }

  FREE(hash->bucket);
  FREE(hash);

  return;

} /* end of cuddHashTableQuit */

/**Function********************************************************************

  Synopsis    [Shuts down a hash table.]

  Description [Shuts down a hash table, when the values are not DdNode
  pointers.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInit]

******************************************************************************/
void cuddHashTableGenericQuit(DdHashTable *hash) {
#ifdef __osf__
#pragma pointer_size save
#pragma pointer_size short
#endif
  DdHashItem **memlist, **nextmem;

  memlist = hash->memoryList;
  while (memlist != NULL) {
    nextmem = (DdHashItem **)memlist[0];
    FREE(memlist);
    memlist = nextmem;
  }

  FREE(hash->bucket);
  FREE(hash);
#ifdef __osf__
#pragma pointer_size restore
#endif

  return;

} /* end of cuddHashTableGenericQuit */

/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key has more than
  three pointers.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [[cuddHashTableInsert1 cuddHashTableInsert2 cuddHashTableInsert3
  cuddHashTableLookup]

******************************************************************************/
int cuddHashTableInsert(DdHashTable *hash, DdNodePtr *key, DdNode *value,
                        ptrint count) {
  int result;
  unsigned int posn;
  DdHashItem *item;
  unsigned int i;

#ifdef DD_DEBUG
  assert(hash->keysize > 3);
#endif

  if (hash->size > hash->maxsize) {
    result = cuddHashTableResize(hash);
    if (result == 0)
      return (0);
  }
  item = cuddHashTableAlloc(hash);
  if (item == NULL)
    return (0);
  hash->size++;
  item->value = value;
  cuddRef(value);
  item->count = count;
  for (i = 0; i < hash->keysize; i++) {
    item->key[i] = key[i];
  }
  posn = ddLCHash(key, hash->keysize, hash->shift);
  item->next = hash->bucket[posn];
  hash->bucket[posn] = item;

  return (1);

} /* end of cuddHashTableInsert */

/**Function********************************************************************

  Synopsis    [Looks up a key in a hash table.]

  Description [Looks up a key consisting of more than three pointers
  in a hash table.  Returns the value associated to the key if there
  is an entry for the given key in the table; NULL otherwise. If the
  entry is present, its reference counter is decremented if not
  saturated. If the counter reaches 0, the value of the entry is
  dereferenced, and the entry is returned to the free list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup1 cuddHashTableLookup2 cuddHashTableLookup3
  cuddHashTableInsert]

******************************************************************************/
DdNode *cuddHashTableLookup(DdHashTable *hash, DdNodePtr *key) {
  unsigned int posn;
  DdHashItem *item, *prev;
  unsigned int i, keysize;

#ifdef DD_DEBUG
  assert(hash->keysize > 3);
#endif

  posn = ddLCHash(key, hash->keysize, hash->shift);
  item = hash->bucket[posn];
  prev = NULL;

  keysize = hash->keysize;
  while (item != NULL) {
    DdNodePtr *key2 = item->key;
    int equal = 1;
    for (i = 0; i < keysize; i++) {
      if (key[i] != key2[i]) {
        equal = 0;
        break;
      }
    }
    if (equal) {
      DdNode *value = item->value;
      cuddSatDec(item->count);
      if (item->count == 0) {
        cuddDeref(value);
        if (prev == NULL) {
          hash->bucket[posn] = item->next;
        } else {
          prev->next = item->next;
        }
        item->next = hash->nextFree;
        hash->nextFree = item;
        hash->size--;
      }
      return (value);
    }
    prev = item;
    item = item->next;
  }
  return (NULL);

} /* end of cuddHashTableLookup */

/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is one pointer.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert cuddHashTableInsert2 cuddHashTableInsert3
  cuddHashTableLookup1]

******************************************************************************/
int cuddHashTableInsert1(DdHashTable *hash, DdNode *f, DdNode *value,
                         ptrint count) {
  int result;
  unsigned int posn;
  DdHashItem *item;

#ifdef DD_DEBUG
  assert(hash->keysize == 1);
#endif

  if (hash->size > hash->maxsize) {
    result = cuddHashTableResize(hash);
    if (result == 0)
      return (0);
  }
  item = cuddHashTableAlloc(hash);
  if (item == NULL)
    return (0);
  hash->size++;
  item->value = value;
  cuddRef(value);
  item->count = count;
  item->key[0] = f;
  posn = ddLCHash1(f, hash->shift);
  item->next = hash->bucket[posn];
  hash->bucket[posn] = item;

  return (1);

} /* end of cuddHashTableInsert1 */

/**Function********************************************************************

  Synopsis    [Looks up a key consisting of one pointer in a hash table.]

  Description [Looks up a key consisting of one pointer in a hash table.
  Returns the value associated to the key if there is an entry for the given
  key in the table; NULL otherwise. If the entry is present, its reference
  counter is decremented if not saturated. If the counter reaches 0, the
  value of the entry is dereferenced, and the entry is returned to the free
  list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup cuddHashTableLookup2 cuddHashTableLookup3
  cuddHashTableInsert1]

******************************************************************************/
DdNode *cuddHashTableLookup1(DdHashTable *hash, DdNode *f) {
  unsigned int posn;
  DdHashItem *item, *prev;

#ifdef DD_DEBUG
  assert(hash->keysize == 1);
#endif

  posn = ddLCHash1(f, hash->shift);
  item = hash->bucket[posn];
  prev = NULL;

  while (item != NULL) {
    DdNodePtr *key = item->key;
    if (f == key[0]) {
      DdNode *value = item->value;
      cuddSatDec(item->count);
      if (item->count == 0) {
        cuddDeref(value);
        if (prev == NULL) {
          hash->bucket[posn] = item->next;
        } else {
          prev->next = item->next;
        }
        item->next = hash->nextFree;
        hash->nextFree = item;
        hash->size--;
      }
      return (value);
    }
    prev = item;
    item = item->next;
  }
  return (NULL);

} /* end of cuddHashTableLookup1 */

/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is one
  pointer and the value is not a DdNode pointer.  Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert1 cuddHashTableGenericLookup]

******************************************************************************/
int cuddHashTableGenericInsert(DdHashTable *hash, DdNode *f, void *value) {
  int result;
  unsigned int posn;
  DdHashItem *item;

#ifdef DD_DEBUG
  assert(hash->keysize == 1);
#endif

  if (hash->size > hash->maxsize) {
    result = cuddHashTableResize(hash);
    if (result == 0)
      return (0);
  }
  item = cuddHashTableAlloc(hash);
  if (item == NULL)
    return (0);
  hash->size++;
  item->value = (DdNode *)value;
  item->count = 0;
  item->key[0] = f;
  posn = ddLCHash1(f, hash->shift);
  item->next = hash->bucket[posn];
  hash->bucket[posn] = item;

  return (1);

} /* end of cuddHashTableGenericInsert */

/**Function********************************************************************

  Synopsis    [Looks up a key consisting of one pointer in a hash table.]

  Description [Looks up a key consisting of one pointer in a hash
  table when the value is not a DdNode pointer.  Returns the value
  associated to the key if there is an entry for the given key in the
  table; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup1 cuddHashTableGenericInsert]

******************************************************************************/
void *cuddHashTableGenericLookup(DdHashTable *hash, DdNode *f) {
  unsigned int posn;
  DdHashItem *item;

#ifdef DD_DEBUG
  assert(hash->keysize == 1);
#endif

  posn = ddLCHash1(f, hash->shift);
  item = hash->bucket[posn];

  while (item != NULL) {
    if (f == item->key[0]) {
      return ((void *)item->value);
    }
    item = item->next;
  }
  return (NULL);

} /* end of cuddHashTableGenericLookup */

/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is
  composed of two pointers. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert cuddHashTableInsert1 cuddHashTableInsert3
  cuddHashTableLookup2]

******************************************************************************/
int cuddHashTableInsert2(DdHashTable *hash, DdNode *f, DdNode *g, DdNode *value,
                         ptrint count) {
  int result;
  unsigned int posn;
  DdHashItem *item;

#ifdef DD_DEBUG
  assert(hash->keysize == 2);
#endif

  if (hash->size > hash->maxsize) {
    result = cuddHashTableResize(hash);
    if (result == 0)
      return (0);
  }
  item = cuddHashTableAlloc(hash);
  if (item == NULL)
    return (0);
  hash->size++;
  item->value = value;
  cuddRef(value);
  item->count = count;
  item->key[0] = f;
  item->key[1] = g;
  posn = ddLCHash2(f, g, hash->shift);
  item->next = hash->bucket[posn];
  hash->bucket[posn] = item;

  return (1);

} /* end of cuddHashTableInsert2 */

/**Function********************************************************************

  Synopsis    [Looks up a key consisting of two pointers in a hash table.]

  Description [Looks up a key consisting of two pointer in a hash table.
  Returns the value associated to the key if there is an entry for the given
  key in the table; NULL otherwise. If the entry is present, its reference
  counter is decremented if not saturated. If the counter reaches 0, the
  value of the entry is dereferenced, and the entry is returned to the free
  list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup cuddHashTableLookup1 cuddHashTableLookup3
  cuddHashTableInsert2]

******************************************************************************/
DdNode *cuddHashTableLookup2(DdHashTable *hash, DdNode *f, DdNode *g) {
  unsigned int posn;
  DdHashItem *item, *prev;

#ifdef DD_DEBUG
  assert(hash->keysize == 2);
#endif

  posn = ddLCHash2(f, g, hash->shift);
  item = hash->bucket[posn];
  prev = NULL;

  while (item != NULL) {
    DdNodePtr *key = item->key;
    if ((f == key[0]) && (g == key[1])) {
      DdNode *value = item->value;
      cuddSatDec(item->count);
      if (item->count == 0) {
        cuddDeref(value);
        if (prev == NULL) {
          hash->bucket[posn] = item->next;
        } else {
          prev->next = item->next;
        }
        item->next = hash->nextFree;
        hash->nextFree = item;
        hash->size--;
      }
      return (value);
    }
    prev = item;
    item = item->next;
  }
  return (NULL);

} /* end of cuddHashTableLookup2 */

/**Function********************************************************************

  Synopsis    [Inserts an item in a hash table.]

  Description [Inserts an item in a hash table when the key is
  composed of three pointers. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert cuddHashTableInsert1 cuddHashTableInsert2
  cuddHashTableLookup3]

******************************************************************************/
int cuddHashTableInsert3(DdHashTable *hash, DdNode *f, DdNode *g, DdNode *h,
                         DdNode *value, ptrint count) {
  int result;
  unsigned int posn;
  DdHashItem *item;

#ifdef DD_DEBUG
  assert(hash->keysize == 3);
#endif

  if (hash->size > hash->maxsize) {
    result = cuddHashTableResize(hash);
    if (result == 0)
      return (0);
  }
  item = cuddHashTableAlloc(hash);
  if (item == NULL)
    return (0);
  hash->size++;
  item->value = value;
  cuddRef(value);
  item->count = count;
  item->key[0] = f;
  item->key[1] = g;
  item->key[2] = h;
  posn = ddLCHash3(f, g, h, hash->shift);
  item->next = hash->bucket[posn];
  hash->bucket[posn] = item;

  return (1);

} /* end of cuddHashTableInsert3 */

/**Function********************************************************************

  Synopsis    [Looks up a key consisting of three pointers in a hash table.]

  Description [Looks up a key consisting of three pointers in a hash table.
  Returns the value associated to the key if there is an entry for the given
  key in the table; NULL otherwise. If the entry is present, its reference
  counter is decremented if not saturated. If the counter reaches 0, the
  value of the entry is dereferenced, and the entry is returned to the free
  list.]

  SideEffects [None]

  SeeAlso     [cuddHashTableLookup cuddHashTableLookup1 cuddHashTableLookup2
  cuddHashTableInsert3]

******************************************************************************/
DdNode *cuddHashTableLookup3(DdHashTable *hash, DdNode *f, DdNode *g,
                             DdNode *h) {
  unsigned int posn;
  DdHashItem *item, *prev;

#ifdef DD_DEBUG
  assert(hash->keysize == 3);
#endif

  posn = ddLCHash3(f, g, h, hash->shift);
  item = hash->bucket[posn];
  prev = NULL;

  while (item != NULL) {
    DdNodePtr *key = item->key;
    if ((f == key[0]) && (g == key[1]) && (h == key[2])) {
      DdNode *value = item->value;
      cuddSatDec(item->count);
      if (item->count == 0) {
        cuddDeref(value);
        if (prev == NULL) {
          hash->bucket[posn] = item->next;
        } else {
          prev->next = item->next;
        }
        item->next = hash->nextFree;
        hash->nextFree = item;
        hash->size--;
      }
      return (value);
    }
    prev = item;
    item = item->next;
  }
  return (NULL);

} /* end of cuddHashTableLookup3 */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Resizes a local cache.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void cuddLocalCacheResize(DdLocalCache *cache) {
  DdLocalCacheItem *item, *olditem, *entry, *old;
  int i, shift;
  unsigned int posn;
  unsigned int slots, oldslots;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  olditem = cache->item;
  oldslots = cache->slots;
  slots = cache->slots = oldslots << 1;

#ifdef DD_VERBOSE
  (void)fprintf(cache->manager->err,
                "Resizing local cache from %d to %d entries\n", oldslots,
                slots);
  (void)fprintf(cache->manager->err,
                "\thits = %.0f\tlookups = %.0f\thit ratio = %5.3f\n",
                cache->hits, cache->lookUps, cache->hits / cache->lookUps);
#endif

  saveHandler = MMoutOfMemory;
  MMoutOfMemory = Cudd_OutOfMem;
  cache->item = item = (DdLocalCacheItem *)ALLOC(char, slots * cache->itemsize);
  MMoutOfMemory = saveHandler;
  /* If we fail to allocate the new table we just give up. */
  if (item == NULL) {
#ifdef DD_VERBOSE
    (void)fprintf(cache->manager->err, "Resizing failed. Giving up.\n");
#endif
    cache->slots = oldslots;
    cache->item = olditem;
    /* Do not try to resize again. */
    cache->maxslots = oldslots - 1;
    return;
  }
  shift = --(cache->shift);
  cache->manager->memused += (slots - oldslots) * cache->itemsize;

  /* Clear new cache. */
  memset(item, 0, slots * cache->itemsize);

  /* Copy from old cache to new one. */
  for (i = 0; (unsigned)i < oldslots; i++) {
    old = (DdLocalCacheItem *)((char *)olditem + i * cache->itemsize);
    if (old->value != NULL) {
      posn = ddLCHash(old->key, cache->keysize, shift);
      entry = (DdLocalCacheItem *)((char *)item + posn * cache->itemsize);
      memcpy(entry->key, old->key, cache->keysize * sizeof(DdNode *));
      entry->value = old->value;
    }
  }

  FREE(olditem);

  /* Reinitialize measurements so as to avoid division by 0 and
  ** immediate resizing.
  */
  cache->lookUps = (double)(int)(slots * cache->minHit + 1);
  cache->hits = 0;

} /* end of cuddLocalCacheResize */

/**Function********************************************************************

  Synopsis    [Computes the hash value for a local cache.]

  Description [Computes the hash value for a local cache. Returns the
  bucket index.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DD_INLINE
static unsigned int ddLCHash(DdNodePtr *key, unsigned int keysize, int shift) {
  unsigned int val = (unsigned int)(ptrint)key[0] * DD_P2;
  unsigned int i;

  for (i = 1; i < keysize; i++) {
    val = val * DD_P1 + (int)(ptrint)key[i];
  }

  return (val >> shift);

} /* end of ddLCHash */

/**Function********************************************************************

  Synopsis    [Inserts a local cache in the manager list.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void cuddLocalCacheAddToList(DdLocalCache *cache) {
  DdManager *manager = cache->manager;

  cache->next = manager->localCaches;
  manager->localCaches = cache;
  return;

} /* end of cuddLocalCacheAddToList */

/**Function********************************************************************

  Synopsis    [Removes a local cache from the manager list.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void cuddLocalCacheRemoveFromList(DdLocalCache *cache) {
  DdManager *manager = cache->manager;
  DdLocalCache **prevCache, *nextCache;

  prevCache = &(manager->localCaches);
  nextCache = manager->localCaches;

  while (nextCache != NULL) {
    if (nextCache == cache) {
      *prevCache = nextCache->next;
      return;
    }
    prevCache = &(nextCache->next);
    nextCache = nextCache->next;
  }
  return; /* should never get here */

} /* end of cuddLocalCacheRemoveFromList */

/**Function********************************************************************

  Synopsis    [Resizes a hash table.]

  Description [Resizes a hash table. Returns 1 if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddHashTableInsert]

******************************************************************************/
static int cuddHashTableResize(DdHashTable *hash) {
  int j;
  unsigned int posn;
  DdHashItem *item;
  DdHashItem *next;
  DdNode **key;
  int numBuckets;
  DdHashItem **buckets;
  DdHashItem **oldBuckets = hash->bucket;
  int shift;
  int oldNumBuckets = hash->numBuckets;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  /* Compute the new size of the table. */
  numBuckets = oldNumBuckets << 1;
  saveHandler = MMoutOfMemory;
  MMoutOfMemory = Cudd_OutOfMem;
  buckets = ALLOC(DdHashItem *, numBuckets);
  MMoutOfMemory = saveHandler;
  if (buckets == NULL) {
    hash->maxsize <<= 1;
    return (1);
  }

  hash->bucket = buckets;
  hash->numBuckets = numBuckets;
  shift = --(hash->shift);
  hash->maxsize <<= 1;
  memset(buckets, 0, numBuckets * sizeof(DdHashItem *));
  if (hash->keysize == 1) {
    for (j = 0; j < oldNumBuckets; j++) {
      item = oldBuckets[j];
      while (item != NULL) {
        next = item->next;
        key = item->key;
        posn = ddLCHash2(key[0], key[0], shift);
        item->next = buckets[posn];
        buckets[posn] = item;
        item = next;
      }
    }
  } else if (hash->keysize == 2) {
    for (j = 0; j < oldNumBuckets; j++) {
      item = oldBuckets[j];
      while (item != NULL) {
        next = item->next;
        key = item->key;
        posn = ddLCHash2(key[0], key[1], shift);
        item->next = buckets[posn];
        buckets[posn] = item;
        item = next;
      }
    }
  } else if (hash->keysize == 3) {
    for (j = 0; j < oldNumBuckets; j++) {
      item = oldBuckets[j];
      while (item != NULL) {
        next = item->next;
        key = item->key;
        posn = ddLCHash3(key[0], key[1], key[2], shift);
        item->next = buckets[posn];
        buckets[posn] = item;
        item = next;
      }
    }
  } else {
    for (j = 0; j < oldNumBuckets; j++) {
      item = oldBuckets[j];
      while (item != NULL) {
        next = item->next;
        posn = ddLCHash(item->key, hash->keysize, shift);
        item->next = buckets[posn];
        buckets[posn] = item;
        item = next;
      }
    }
  }
  FREE(oldBuckets);
  return (1);

} /* end of cuddHashTableResize */

/**Function********************************************************************

  Synopsis    [Fast storage allocation for items in a hash table.]

  Description [Fast storage allocation for items in a hash table. The
  first 4 bytes of a chunk contain a pointer to the next block; the
  rest contains DD_MEM_CHUNK spaces for hash items.  Returns a pointer to
  a new item if successful; NULL is memory is full.]

  SideEffects [None]

  SeeAlso     [cuddAllocNode cuddDynamicAllocNode]

******************************************************************************/
DD_INLINE
static DdHashItem *cuddHashTableAlloc(DdHashTable *hash) {
  int i;
  unsigned int itemsize = hash->itemsize;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;
  DdHashItem **mem, *thisOne, *next, *item;

  if (hash->nextFree == NULL) {
    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    mem = (DdHashItem **)ALLOC(char, (DD_MEM_CHUNK + 1) * itemsize);
    MMoutOfMemory = saveHandler;
    if (mem == NULL) {
      if (hash->manager->stash != NULL) {
        FREE(hash->manager->stash);
        hash->manager->stash = NULL;
        /* Inhibit resizing of tables. */
        hash->manager->maxCacheHard = hash->manager->cacheSlots - 1;
        hash->manager->cacheSlack = -(int)(hash->manager->cacheSlots + 1);
        for (i = 0; i < hash->manager->size; i++) {
          hash->manager->subtables[i].maxKeys <<= 2;
        }
        hash->manager->gcFrac = 0.2;
        hash->manager->minDead = (unsigned)(0.2 * (double)hash->manager->slots);
        mem = (DdHashItem **)ALLOC(char, (DD_MEM_CHUNK + 1) * itemsize);
      }
      if (mem == NULL) {
        (*MMoutOfMemory)((long)((DD_MEM_CHUNK + 1) * itemsize));
        hash->manager->errorCode = CUDD_MEMORY_OUT;
        return (NULL);
      }
    }

    mem[0] = (DdHashItem *)hash->memoryList;
    hash->memoryList = mem;

    thisOne = (DdHashItem *)((char *)mem + itemsize);
    hash->nextFree = thisOne;
    for (i = 1; i < DD_MEM_CHUNK; i++) {
      next = (DdHashItem *)((char *)thisOne + itemsize);
      thisOne->next = next;
      thisOne = next;
    }

    thisOne->next = NULL;
  }
  item = hash->nextFree;
  hash->nextFree = item->next;
  return (item);

} /* end of cuddHashTableAlloc */
/**CFile***********************************************************************

  FileName    [cuddLinear.c]

  PackageName [cudd]

  Synopsis    [Functions for DD reduction by linear transformations.]

  Description [ Internal procedures included in this module:
                <ul>
                <li> cuddLinearAndSifting()
                <li> cuddLinearInPlace()
                <li> cuddUpdateInteractionMatrix()
                <li> cuddInitLinear()
                <li> cuddResizeLinear()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> ddLinearUniqueCompare()
                <li> ddLinearAndSiftingAux()
                <li> ddLinearAndSiftingUp()
                <li> ddLinearAndSiftingDown()
                <li> ddLinearAndSiftingBackward()
                <li> ddUndoMoves()
                <li> cuddXorLinear()
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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define CUDD_SWAP_MOVE 0
#define CUDD_LINEAR_TRANSFORM_MOVE 1
#define CUDD_INVERSE_TRANSFORM_MOVE 2
#if SIZEOF_LONG == 8
#define BPL 64
#define LOGBPL 6
#else
#define BPL 32
#define LOGBPL 5
#endif

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
// static char rcsid[] DD_UNUSED = "$Id: cuddLinear.c,v 1.29 2012/02/05 01:07:19
// fabio Exp $"; #endif

static int *entry;

#ifdef DD_STATS
extern int ddTotalNumberSwapping;
extern int ddTotalNISwaps;
static int ddTotalNumberLinearTr;
#endif

#ifdef DD_DEBUG
static int zero = 0;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddLinearUniqueCompare(int *ptrX, int *ptrY);
static int ddLinearAndSiftingAux(DdManager *table, int x, int xLow, int xHigh);
static Move *ddLinearAndSiftingUp(DdManager *table, int y, int xLow,
                                  Move *prevMoves);
static Move *ddLinearAndSiftingDown(DdManager *table, int x, int xHigh,
                                    Move *prevMoves);
static int ddLinearAndSiftingBackward(DdManager *table, int size, Move *moves);
static Move *ddUndoMoves(DdManager *table, Move *moves);
static void cuddXorLinear(DdManager *table, int x, int y);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Prints the linear transform matrix.]

  Description [Prints the linear transform matrix. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
int Cudd_PrintLinear(DdManager *table) {
  int i, j, k;
  int retval;
  int nvars = table->linearSize;
  int wordsPerRow = ((nvars - 1) >> LOGBPL) + 1;
  long word;

  for (i = 0; i < nvars; i++) {
    for (j = 0; j < wordsPerRow; j++) {
      word = table->linear[i * wordsPerRow + j];
      for (k = 0; k < BPL; k++) {
        retval = fprintf(table->out, "%ld", word & 1);
        if (retval == 0)
          return (0);
        word >>= 1;
      }
    }
    retval = fprintf(table->out, "\n");
    if (retval == 0)
      return (0);
  }
  return (1);

} /* end of Cudd_PrintLinear */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [BDD reduction based on combination of sifting and linear
  transformations.]

  Description [BDD reduction based on combination of sifting and linear
  transformations.  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries
    in each unique table.
    <li> Sift the variable up and down, remembering each time the
    total size of the DD heap. At each position, linear transformation
    of the two adjacent variables is tried and is accepted if it reduces
    the size of the DD.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddLinearAndSifting(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int size;
  int x;
  int result;
#ifdef DD_STATS
  int previousSize;
#endif

#ifdef DD_STATS
  ddTotalNumberLinearTr = 0;
#endif

  size = table->size;

  var = NULL;
  entry = NULL;
  if (table->linear == NULL) {
    result = cuddInitLinear(table);
    if (result == 0)
      goto cuddLinearAndSiftingOutOfMem;
#if 0
        (void) fprintf(table->out,"\n");
	result = Cudd_PrintLinear(table);
	if (result == 0) goto cuddLinearAndSiftingOutOfMem;
#endif
  } else if (table->size != table->linearSize) {
    result = cuddResizeLinear(table);
    if (result == 0)
      goto cuddLinearAndSiftingOutOfMem;
#if 0
        (void) fprintf(table->out,"\n");
	result = Cudd_PrintLinear(table);
	if (result == 0) goto cuddLinearAndSiftingOutOfMem;
#endif
  }

  /* Find order in which to sift variables. */
  entry = ALLOC(int, size);
  if (entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddLinearAndSiftingOutOfMem;
  }
  var = ALLOC(int, size);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddLinearAndSiftingOutOfMem;
  }

  for (i = 0; i < size; i++) {
    x = table->perm[i];
    entry[i] = table->subtables[x].keys;
    var[i] = i;
  }

  qsort((void *)var, size, sizeof(int), (DD_QSFP)ddLinearUniqueCompare);

  /* Now sift. */
  for (i = 0; i < ddMin(table->siftMaxVar, size); i++) {
    x = table->perm[var[i]];
    if (x < lower || x > upper)
      continue;
#ifdef DD_STATS
    previousSize = table->keys - table->isolated;
#endif
    result = ddLinearAndSiftingAux(table, x, lower, upper);
    if (!result)
      goto cuddLinearAndSiftingOutOfMem;
#ifdef DD_STATS
    if (table->keys < (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "-");
    } else if (table->keys > (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "+"); /* should never happen */
      (void)fprintf(
          table->out,
          "\nSize increased from %d to %d while sifting variable %d\n",
          previousSize, table->keys - table->isolated, var[i]);
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
#ifdef DD_DEBUG
    (void)Cudd_DebugCheck(table);
#endif
  }

  FREE(var);
  FREE(entry);

#ifdef DD_STATS
  (void)fprintf(table->out, "\n#:L_LINSIFT %8d: linear trans.",
                ddTotalNumberLinearTr);
#endif

  return (1);

cuddLinearAndSiftingOutOfMem:

  if (entry != NULL)
    FREE(entry);
  if (var != NULL)
    FREE(var);

  return (0);

} /* end of cuddLinearAndSifting */

/**Function********************************************************************

  Synopsis    [Linearly combines two adjacent variables.]

  Description [Linearly combines two adjacent variables. Specifically,
  replaces the top variable with the exclusive nor of the two variables.
  It assumes that no dead nodes are present on entry to this
  procedure.  The procedure then guarantees that no dead nodes will be
  present when it terminates.  cuddLinearInPlace assumes that x &lt;
  y.  Returns the number of keys in the table if successful; 0
  otherwise.]

  SideEffects [The two subtables corrresponding to variables x and y are
  modified. The global counters of the unique table are also affected.]

  SeeAlso     [cuddSwapInPlace]

******************************************************************************/
int cuddLinearInPlace(DdManager *table, int x, int y) {
  DdNodePtr *xlist, *ylist;
  int xindex, yindex;
  int xslots, yslots;
  int xshift, yshift;
  int oldxkeys, oldykeys;
  int newxkeys, newykeys;
  int comple, newcomplement;
  int i;
  int posn;
  int isolated;
  DdNode *f, *f0, *f1, *f01, *f00, *f11, *f10, *newf1, *newf0;
  DdNode *g, *next, *last;
  DdNodePtr *previousP;
  DdNode *tmp;
  DdNode *sentinel = &(table->sentinel);
#ifdef DD_DEBUG
  int count, idcheck;
#endif

#ifdef DD_DEBUG
  assert(x < y);
  assert(cuddNextHigh(table, x) == y);
  assert(table->subtables[x].keys != 0);
  assert(table->subtables[y].keys != 0);
  assert(table->subtables[x].dead == 0);
  assert(table->subtables[y].dead == 0);
#endif

  xindex = table->invperm[x];
  yindex = table->invperm[y];

  if (cuddTestInteract(table, xindex, yindex)) {
#ifdef DD_STATS
    ddTotalNumberLinearTr++;
#endif
    /* Get parameters of x subtable. */
    xlist = table->subtables[x].nodelist;
    oldxkeys = table->subtables[x].keys;
    xslots = table->subtables[x].slots;
    xshift = table->subtables[x].shift;

    /* Get parameters of y subtable. */
    ylist = table->subtables[y].nodelist;
    oldykeys = table->subtables[y].keys;
    yslots = table->subtables[y].slots;
    yshift = table->subtables[y].shift;

    newxkeys = 0;
    newykeys = oldykeys;

    /* Check whether the two projection functions involved in this
    ** swap are isolated. At the end, we'll be able to tell how many
    ** isolated projection functions are there by checking only these
    ** two functions again. This is done to eliminate the isolated
    ** projection functions from the node count.
    */
    isolated =
        -((table->vars[xindex]->ref == 1) + (table->vars[yindex]->ref == 1));

    /* The nodes in the x layer are put in a chain.
    ** The chain is handled as a FIFO; g points to the beginning and
    ** last points to the end.
    */
    g = NULL;
#ifdef DD_DEBUG
    last = NULL;
#endif
    for (i = 0; i < xslots; i++) {
      f = xlist[i];
      if (f == sentinel)
        continue;
      xlist[i] = sentinel;
      if (g == NULL) {
        g = f;
      } else {
        last->next = f;
      }
      while ((next = f->next) != sentinel) {
        f = next;
      } /* while there are elements in the collision chain */
      last = f;
    } /* for each slot of the x subtable */
#ifdef DD_DEBUG
    /* last is always assigned in the for loop because there is at
    ** least one key */
    assert(last != NULL);
#endif
    last->next = NULL;

#ifdef DD_COUNT
    table->swapSteps += oldxkeys;
#endif
    /* Take care of the x nodes that must be re-expressed.
    ** They form a linked list pointed by g.
    */
    f = g;
    while (f != NULL) {
      next = f->next;
      /* Find f1, f0, f11, f10, f01, f00. */
      f1 = cuddT(f);
#ifdef DD_DEBUG
      assert(!(Cudd_IsComplement(f1)));
#endif
      if ((int)f1->index == yindex) {
        f11 = cuddT(f1);
        f10 = cuddE(f1);
      } else {
        f11 = f10 = f1;
      }
#ifdef DD_DEBUG
      assert(!(Cudd_IsComplement(f11)));
#endif
      f0 = cuddE(f);
      comple = Cudd_IsComplement(f0);
      f0 = Cudd_Regular(f0);
      if ((int)f0->index == yindex) {
        f01 = cuddT(f0);
        f00 = cuddE(f0);
      } else {
        f01 = f00 = f0;
      }
      if (comple) {
        f01 = Cudd_Not(f01);
        f00 = Cudd_Not(f00);
      }
      /* Decrease ref count of f1. */
      cuddSatDec(f1->ref);
      /* Create the new T child. */
      if (f11 == f00) {
        newf1 = f11;
        cuddSatInc(newf1->ref);
      } else {
        /* Check ylist for triple (yindex,f11,f00). */
        posn = ddHash(f11, f00, yshift);
        /* For each element newf1 in collision list ylist[posn]. */
        previousP = &(ylist[posn]);
        newf1 = *previousP;
        while (f11 < cuddT(newf1)) {
          previousP = &(newf1->next);
          newf1 = *previousP;
        }
        while (f11 == cuddT(newf1) && f00 < cuddE(newf1)) {
          previousP = &(newf1->next);
          newf1 = *previousP;
        }
        if (cuddT(newf1) == f11 && cuddE(newf1) == f00) {
          cuddSatInc(newf1->ref);
        } else { /* no match */
          newf1 = cuddDynamicAllocNode(table);
          if (newf1 == NULL)
            goto cuddLinearOutOfMem;
          newf1->index = yindex;
          newf1->ref = 1;
          cuddT(newf1) = f11;
          cuddE(newf1) = f00;
          /* Insert newf1 in the collision list ylist[posn];
          ** increase the ref counts of f11 and f00.
          */
          newykeys++;
          newf1->next = *previousP;
          *previousP = newf1;
          cuddSatInc(f11->ref);
          tmp = Cudd_Regular(f00);
          cuddSatInc(tmp->ref);
        }
      }
      cuddT(f) = newf1;
#ifdef DD_DEBUG
      assert(!(Cudd_IsComplement(newf1)));
#endif

      /* Do the same for f0, keeping complement dots into account. */
      /* decrease ref count of f0 */
      tmp = Cudd_Regular(f0);
      cuddSatDec(tmp->ref);
      /* create the new E child */
      if (f01 == f10) {
        newf0 = f01;
        tmp = Cudd_Regular(newf0);
        cuddSatInc(tmp->ref);
      } else {
        /* make sure f01 is regular */
        newcomplement = Cudd_IsComplement(f01);
        if (newcomplement) {
          f01 = Cudd_Not(f01);
          f10 = Cudd_Not(f10);
        }
        /* Check ylist for triple (yindex,f01,f10). */
        posn = ddHash(f01, f10, yshift);
        /* For each element newf0 in collision list ylist[posn]. */
        previousP = &(ylist[posn]);
        newf0 = *previousP;
        while (f01 < cuddT(newf0)) {
          previousP = &(newf0->next);
          newf0 = *previousP;
        }
        while (f01 == cuddT(newf0) && f10 < cuddE(newf0)) {
          previousP = &(newf0->next);
          newf0 = *previousP;
        }
        if (cuddT(newf0) == f01 && cuddE(newf0) == f10) {
          cuddSatInc(newf0->ref);
        } else { /* no match */
          newf0 = cuddDynamicAllocNode(table);
          if (newf0 == NULL)
            goto cuddLinearOutOfMem;
          newf0->index = yindex;
          newf0->ref = 1;
          cuddT(newf0) = f01;
          cuddE(newf0) = f10;
          /* Insert newf0 in the collision list ylist[posn];
          ** increase the ref counts of f01 and f10.
          */
          newykeys++;
          newf0->next = *previousP;
          *previousP = newf0;
          cuddSatInc(f01->ref);
          tmp = Cudd_Regular(f10);
          cuddSatInc(tmp->ref);
        }
        if (newcomplement) {
          newf0 = Cudd_Not(newf0);
        }
      }
      cuddE(f) = newf0;

      /* Re-insert the modified f in xlist.
      ** The modified f does not already exists in xlist.
      ** (Because of the uniqueness of the cofactors.)
      */
      posn = ddHash(newf1, newf0, xshift);
      newxkeys++;
      previousP = &(xlist[posn]);
      tmp = *previousP;
      while (newf1 < cuddT(tmp)) {
        previousP = &(tmp->next);
        tmp = *previousP;
      }
      while (newf1 == cuddT(tmp) && newf0 < cuddE(tmp)) {
        previousP = &(tmp->next);
        tmp = *previousP;
      }
      f->next = *previousP;
      *previousP = f;
      f = next;
    } /* while f != NULL */

    /* GC the y layer. */

    /* For each node f in ylist. */
    for (i = 0; i < yslots; i++) {
      previousP = &(ylist[i]);
      f = *previousP;
      while (f != sentinel) {
        next = f->next;
        if (f->ref == 0) {
          tmp = cuddT(f);
          cuddSatDec(tmp->ref);
          tmp = Cudd_Regular(cuddE(f));
          cuddSatDec(tmp->ref);
          cuddDeallocNode(table, f);
          newykeys--;
        } else {
          *previousP = f;
          previousP = &(f->next);
        }
        f = next;
      } /* while f */
      *previousP = sentinel;
    } /* for every collision list */

#ifdef DD_DEBUG
#if 0
	(void) fprintf(table->out,"Linearly combining %d and %d\n",x,y);
#endif
    count = 0;
    idcheck = 0;
    for (i = 0; i < yslots; i++) {
      f = ylist[i];
      while (f != sentinel) {
        count++;
        if (f->index != (DdHalfWord)yindex)
          idcheck++;
        f = f->next;
      }
    }
    if (count != newykeys) {
      fprintf(table->err,
              "Error in finding newykeys\toldykeys = %d\tnewykeys = %d\tactual "
              "= %d\n",
              oldykeys, newykeys, count);
    }
    if (idcheck != 0)
      fprintf(table->err, "Error in id's of ylist\twrong id's = %d\n", idcheck);
    count = 0;
    idcheck = 0;
    for (i = 0; i < xslots; i++) {
      f = xlist[i];
      while (f != sentinel) {
        count++;
        if (f->index != (DdHalfWord)xindex)
          idcheck++;
        f = f->next;
      }
    }
    if (count != newxkeys || newxkeys != oldxkeys) {
      fprintf(table->err,
              "Error in finding newxkeys\toldxkeys = %d \tnewxkeys = %d "
              "\tactual = %d\n",
              oldxkeys, newxkeys, count);
    }
    if (idcheck != 0)
      fprintf(table->err, "Error in id's of xlist\twrong id's = %d\n", idcheck);
#endif

    isolated +=
        (table->vars[xindex]->ref == 1) + (table->vars[yindex]->ref == 1);
    table->isolated += isolated;

    /* Set the appropriate fields in table. */
    table->subtables[y].keys = newykeys;

    /* Here we should update the linear combination table
    ** to record that x <- x EXNOR y. This is done by complementing
    ** the (x,y) entry of the table.
    */

    table->keys += newykeys - oldykeys;

    cuddXorLinear(table, xindex, yindex);
  }

#ifdef DD_DEBUG
  if (zero) {
    (void)Cudd_DebugCheck(table);
  }
#endif

  return (table->keys - table->isolated);

cuddLinearOutOfMem:
  (void)fprintf(table->err, "Error: cuddLinearInPlace out of memory\n");

  return (0);

} /* end of cuddLinearInPlace */

/**Function********************************************************************

  Synopsis    [Updates the interaction matrix.]

  Description []

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
void cuddUpdateInteractionMatrix(DdManager *table, int xindex, int yindex) {
  int i;
  for (i = 0; i < yindex; i++) {
    if (i != xindex && cuddTestInteract(table, i, yindex)) {
      if (i < xindex) {
        cuddSetInteract(table, i, xindex);
      } else {
        cuddSetInteract(table, xindex, i);
      }
    }
  }
  for (i = yindex + 1; i < table->size; i++) {
    if (i != xindex && cuddTestInteract(table, yindex, i)) {
      if (i < xindex) {
        cuddSetInteract(table, i, xindex);
      } else {
        cuddSetInteract(table, xindex, i);
      }
    }
  }

} /* end of cuddUpdateInteractionMatrix */

/**Function********************************************************************

  Synopsis    [Initializes the linear transform matrix.]

  Description [Initializes the linear transform matrix.  Returns 1 if
  successful; 0 otherwise.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
int cuddInitLinear(DdManager *table) {
  int words;
  int wordsPerRow;
  int nvars;
  int word;
  int bit;
  int i;
  long *linear;

  nvars = table->size;
  wordsPerRow = ((nvars - 1) >> LOGBPL) + 1;
  words = wordsPerRow * nvars;
  table->linear = linear = ALLOC(long, words);
  if (linear == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  table->memused += words * sizeof(long);
  table->linearSize = nvars;
  for (i = 0; i < words; i++)
    linear[i] = 0;
  for (i = 0; i < nvars; i++) {
    word = wordsPerRow * i + (i >> LOGBPL);
    bit = i & (BPL - 1);
    linear[word] = 1 << bit;
  }
  return (1);

} /* end of cuddInitLinear */

/**Function********************************************************************

  Synopsis    [Resizes the linear transform matrix.]

  Description [Resizes the linear transform matrix.  Returns 1 if
  successful; 0 otherwise.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
int cuddResizeLinear(DdManager *table) {
  int words, oldWords;
  int wordsPerRow, oldWordsPerRow;
  int nvars, oldNvars;
  int word, oldWord;
  int bit;
  int i, j;
  long *linear, *oldLinear;

  oldNvars = table->linearSize;
  oldWordsPerRow = ((oldNvars - 1) >> LOGBPL) + 1;
  oldWords = oldWordsPerRow * oldNvars;
  oldLinear = table->linear;

  nvars = table->size;
  wordsPerRow = ((nvars - 1) >> LOGBPL) + 1;
  words = wordsPerRow * nvars;
  table->linear = linear = ALLOC(long, words);
  if (linear == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  table->memused += (words - oldWords) * sizeof(long);
  for (i = 0; i < words; i++)
    linear[i] = 0;

  /* Copy old matrix. */
  for (i = 0; i < oldNvars; i++) {
    for (j = 0; j < oldWordsPerRow; j++) {
      oldWord = oldWordsPerRow * i + j;
      word = wordsPerRow * i + j;
      linear[word] = oldLinear[oldWord];
    }
  }
  FREE(oldLinear);

  /* Add elements to the diagonal. */
  for (i = oldNvars; i < nvars; i++) {
    word = wordsPerRow * i + (i >> LOGBPL);
    bit = i & (BPL - 1);
    linear[word] = 1 << bit;
  }
  table->linearSize = nvars;

  return (1);

} /* end of cuddResizeLinear */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the
  variables according to the number of keys in the subtables.
  Returns the difference in number of keys between the two
  variables being compared.]

  SideEffects [None]

******************************************************************************/
static int ddLinearUniqueCompare(int *ptrX, int *ptrY) {
#if 0
    if (entry[*ptrY] == entry[*ptrX]) {
	return((*ptrX) - (*ptrY));
    }
#endif
  return (entry[*ptrY] - entry[*ptrX]);

} /* end of ddLinearUniqueCompare */

/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. At each step a linear transformation is tried, and, if it
  decreases the size of the DD, it is accepted. Finds the best position
  and does the required changes.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddLinearAndSiftingAux(DdManager *table, int x, int xLow, int xHigh) {

  Move *move;
  Move *moveUp;   /* list of up moves */
  Move *moveDown; /* list of down moves */
  int initialSize;
  int result;

  initialSize = table->keys - table->isolated;

  moveDown = NULL;
  moveUp = NULL;

  if (x == xLow) {
    moveDown = ddLinearAndSiftingDown(table, x, xHigh, NULL);
    /* At this point x --> xHigh unless bounding occurred. */
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto ddLinearAndSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddLinearAndSiftingBackward(table, initialSize, moveDown);
    if (!result)
      goto ddLinearAndSiftingAuxOutOfMem;

  } else if (x == xHigh) {
    moveUp = ddLinearAndSiftingUp(table, x, xLow, NULL);
    /* At this point x --> xLow unless bounding occurred. */
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto ddLinearAndSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddLinearAndSiftingBackward(table, initialSize, moveUp);
    if (!result)
      goto ddLinearAndSiftingAuxOutOfMem;

  } else if ((x - xLow) > (xHigh - x)) { /* must go down first: shorter */
    moveDown = ddLinearAndSiftingDown(table, x, xHigh, NULL);
    /* At this point x --> xHigh unless bounding occurred. */
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto ddLinearAndSiftingAuxOutOfMem;
    moveUp = ddUndoMoves(table, moveDown);
#ifdef DD_DEBUG
    assert(moveUp == NULL || moveUp->x == x);
#endif
    moveUp = ddLinearAndSiftingUp(table, x, xLow, moveUp);
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto ddLinearAndSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddLinearAndSiftingBackward(table, initialSize, moveUp);
    if (!result)
      goto ddLinearAndSiftingAuxOutOfMem;

  } else { /* must go up first: shorter */
    moveUp = ddLinearAndSiftingUp(table, x, xLow, NULL);
    /* At this point x --> xLow unless bounding occurred. */
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto ddLinearAndSiftingAuxOutOfMem;
    moveDown = ddUndoMoves(table, moveUp);
#ifdef DD_DEBUG
    assert(moveDown == NULL || moveDown->y == x);
#endif
    moveDown = ddLinearAndSiftingDown(table, x, xHigh, moveDown);
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto ddLinearAndSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddLinearAndSiftingBackward(table, initialSize, moveDown);
    if (!result)
      goto ddLinearAndSiftingAuxOutOfMem;
  }

  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (1);

ddLinearAndSiftingAuxOutOfMem:
  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (0);

} /* end of ddLinearAndSiftingAux */

/**Function********************************************************************

  Synopsis    [Sifts a variable up and applies linear transformations.]

  Description [Sifts a variable up and applies linear transformations.
  Moves y up until either it reaches the bound (xLow) or the size of
  the DD heap increases too much.  Returns the set of moves in case of
  success; NULL if memory is full.]

  SideEffects [None]

******************************************************************************/
static Move *ddLinearAndSiftingUp(DdManager *table, int y, int xLow,
                                  Move *prevMoves) {
  Move *moves;
  Move *move;
  int x;
  int size, newsize;
  int limitSize;
  int xindex, yindex;
  int isolated;
  int L; /* lower bound on DD size */
#ifdef DD_DEBUG
  int checkL;
  int z;
  int zindex;
#endif

  moves = prevMoves;
  yindex = table->invperm[y];

  /* Initialize the lower bound.
  ** The part of the DD below y will not change.
  ** The part of the DD above y that does not interact with y will not
  ** change. The rest may vanish in the best case, except for
  ** the nodes at level xLow, which will not vanish, regardless.
  */
  limitSize = L = table->keys - table->isolated;
  for (x = xLow + 1; x < y; x++) {
    xindex = table->invperm[x];
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[xindex]->ref == 1;
      L -= table->subtables[x].keys - isolated;
    }
  }
  isolated = table->vars[yindex]->ref == 1;
  L -= table->subtables[y].keys - isolated;

  x = cuddNextLow(table, y);
  while (x >= xLow && L <= limitSize) {
    xindex = table->invperm[x];
#ifdef DD_DEBUG
    checkL = table->keys - table->isolated;
    for (z = xLow + 1; z < y; z++) {
      zindex = table->invperm[z];
      if (cuddTestInteract(table, zindex, yindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkL -= table->subtables[z].keys - isolated;
      }
    }
    isolated = table->vars[yindex]->ref == 1;
    checkL -= table->subtables[y].keys - isolated;
    if (L != checkL) {
      (void)fprintf(table->out, "checkL(%d) != L(%d)\n", checkL, L);
    }
#endif
    size = cuddSwapInPlace(table, x, y);
    if (size == 0)
      goto ddLinearAndSiftingUpOutOfMem;
    newsize = cuddLinearInPlace(table, x, y);
    if (newsize == 0)
      goto ddLinearAndSiftingUpOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddLinearAndSiftingUpOutOfMem;
    move->x = x;
    move->y = y;
    move->next = moves;
    moves = move;
    move->flags = CUDD_SWAP_MOVE;
    if (newsize >= size) {
      /* Undo transformation. The transformation we apply is
      ** its own inverse. Hence, we just apply the transformation
      ** again.
      */
      newsize = cuddLinearInPlace(table, x, y);
      if (newsize == 0)
        goto ddLinearAndSiftingUpOutOfMem;
#ifdef DD_DEBUG
      if (newsize != size) {
        (void)fprintf(
            table->out,
            "Change in size after identity transformation! From %d to %d\n",
            size, newsize);
      }
#endif
    } else if (cuddTestInteract(table, xindex, yindex)) {
      size = newsize;
      move->flags = CUDD_LINEAR_TRANSFORM_MOVE;
      cuddUpdateInteractionMatrix(table, xindex, yindex);
    }
    move->size = size;
    /* Update the lower bound. */
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[xindex]->ref == 1;
      L += table->subtables[y].keys - isolated;
    }
    if ((double)size > (double)limitSize * table->maxGrowth)
      break;
    if (size < limitSize)
      limitSize = size;
    y = x;
    x = cuddNextLow(table, y);
  }
  return (moves);

ddLinearAndSiftingUpOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of ddLinearAndSiftingUp */

/**Function********************************************************************

  Synopsis    [Sifts a variable down and applies linear transformations.]

  Description [Sifts a variable down and applies linear
  transformations. Moves x down until either it reaches the bound
  (xHigh) or the size of the DD heap increases too much. Returns the
  set of moves in case of success; NULL if memory is full.]

  SideEffects [None]

******************************************************************************/
static Move *ddLinearAndSiftingDown(DdManager *table, int x, int xHigh,
                                    Move *prevMoves) {
  Move *moves;
  Move *move;
  int y;
  int size, newsize;
  int R; /* upper bound on node decrease */
  int limitSize;
  int xindex, yindex;
  int isolated;
#ifdef DD_DEBUG
  int checkR;
  int z;
  int zindex;
#endif

  moves = prevMoves;
  /* Initialize R */
  xindex = table->invperm[x];
  limitSize = size = table->keys - table->isolated;
  R = 0;
  for (y = xHigh; y > x; y--) {
    yindex = table->invperm[y];
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[yindex]->ref == 1;
      R += table->subtables[y].keys - isolated;
    }
  }

  y = cuddNextHigh(table, x);
  while (y <= xHigh && size - R < limitSize) {
#ifdef DD_DEBUG
    checkR = 0;
    for (z = xHigh; z > x; z--) {
      zindex = table->invperm[z];
      if (cuddTestInteract(table, xindex, zindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkR += table->subtables[z].keys - isolated;
      }
    }
    if (R != checkR) {
      (void)fprintf(table->out, "checkR(%d) != R(%d)\n", checkR, R);
    }
#endif
    /* Update upper bound on node decrease. */
    yindex = table->invperm[y];
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[yindex]->ref == 1;
      R -= table->subtables[y].keys - isolated;
    }
    size = cuddSwapInPlace(table, x, y);
    if (size == 0)
      goto ddLinearAndSiftingDownOutOfMem;
    newsize = cuddLinearInPlace(table, x, y);
    if (newsize == 0)
      goto ddLinearAndSiftingDownOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddLinearAndSiftingDownOutOfMem;
    move->x = x;
    move->y = y;
    move->next = moves;
    moves = move;
    move->flags = CUDD_SWAP_MOVE;
    if (newsize >= size) {
      /* Undo transformation. The transformation we apply is
      ** its own inverse. Hence, we just apply the transformation
      ** again.
      */
      newsize = cuddLinearInPlace(table, x, y);
      if (newsize == 0)
        goto ddLinearAndSiftingDownOutOfMem;
      if (newsize != size) {
        (void)fprintf(
            table->out,
            "Change in size after identity transformation! From %d to %d\n",
            size, newsize);
      }
    } else if (cuddTestInteract(table, xindex, yindex)) {
      size = newsize;
      move->flags = CUDD_LINEAR_TRANSFORM_MOVE;
      cuddUpdateInteractionMatrix(table, xindex, yindex);
    }
    move->size = size;
    if ((double)size > (double)limitSize * table->maxGrowth)
      break;
    if (size < limitSize)
      limitSize = size;
    x = y;
    y = cuddNextHigh(table, x);
  }
  return (moves);

ddLinearAndSiftingDownOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of ddLinearAndSiftingDown */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the DD heap to the order
  giving the minimum size.]

  Description [Given a set of moves, returns the DD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddLinearAndSiftingBackward(DdManager *table, int size, Move *moves) {
  Move *move;
  int res;

  for (move = moves; move != NULL; move = move->next) {
    if (move->size < size) {
      size = move->size;
    }
  }

  for (move = moves; move != NULL; move = move->next) {
    if (move->size == size)
      return (1);
    if (move->flags == CUDD_LINEAR_TRANSFORM_MOVE) {
      res = cuddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
    }
    res = cuddSwapInPlace(table, (int)move->x, (int)move->y);
    if (!res)
      return (0);
    if (move->flags == CUDD_INVERSE_TRANSFORM_MOVE) {
      res = cuddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
    }
  }

  return (1);

} /* end of ddLinearAndSiftingBackward */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the DD heap to the order
  in effect before the moves.]

  Description [Given a set of moves, returns the DD heap to the
  order in effect before the moves.  Returns 1 in case of success;
  0 otherwise.]

  SideEffects [None]

******************************************************************************/
static Move *ddUndoMoves(DdManager *table, Move *moves) {
  Move *invmoves = NULL;
  Move *move;
  Move *invmove;
  int size;

  for (move = moves; move != NULL; move = move->next) {
    invmove = (Move *)cuddDynamicAllocNode(table);
    if (invmove == NULL)
      goto ddUndoMovesOutOfMem;
    invmove->x = move->x;
    invmove->y = move->y;
    invmove->next = invmoves;
    invmoves = invmove;
    if (move->flags == CUDD_SWAP_MOVE) {
      invmove->flags = CUDD_SWAP_MOVE;
      size = cuddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto ddUndoMovesOutOfMem;
    } else if (move->flags == CUDD_LINEAR_TRANSFORM_MOVE) {
      invmove->flags = CUDD_INVERSE_TRANSFORM_MOVE;
      size = cuddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto ddUndoMovesOutOfMem;
      size = cuddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto ddUndoMovesOutOfMem;
    } else { /* must be CUDD_INVERSE_TRANSFORM_MOVE */
#ifdef DD_DEBUG
      (void)fprintf(table->err, "Unforseen event in ddUndoMoves!\n");
#endif
      invmove->flags = CUDD_LINEAR_TRANSFORM_MOVE;
      size = cuddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto ddUndoMovesOutOfMem;
      size = cuddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto ddUndoMovesOutOfMem;
    }
    invmove->size = size;
  }

  return (invmoves);

ddUndoMovesOutOfMem:
  while (invmoves != NULL) {
    move = invmoves->next;
    cuddDeallocMove(table, invmoves);
    invmoves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of ddUndoMoves */

/**Function********************************************************************

  Synopsis    [XORs two rows of the linear transform matrix.]

  Description [XORs two rows of the linear transform matrix and replaces
  the first row with the result.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
static void cuddXorLinear(DdManager *table, int x, int y) {
  int i;
  int nvars = table->size;
  int wordsPerRow = ((nvars - 1) >> LOGBPL) + 1;
  int xstart = wordsPerRow * x;
  int ystart = wordsPerRow * y;
  long *linear = table->linear;

  for (i = 0; i < wordsPerRow; i++) {
    linear[xstart + i] ^= linear[ystart + i];
  }

} /* end of cuddXorLinear */
/**CFile***********************************************************************

  FileName    [cuddRef.c]

  PackageName [cudd]

  Synopsis    [Functions that manipulate the reference counts.]

  Description [External procedures included in this module:
                    <ul>
                    <li> Cudd_Ref()
                    <li> Cudd_RecursiveDeref()
                    <li> Cudd_IterDerefBdd()
                    <li> Cudd_DelayedDerefBdd()
                    <li> Cudd_RecursiveDerefZdd()
                    <li> Cudd_Deref()
                    <li> Cudd_CheckZeroRef()
                    </ul>
               Internal procedures included in this module:
                    <ul>
                    <li> cuddReclaim()
                    <li> cuddReclaimZdd()
                    <li> cuddClearDeathRow()
                    <li> cuddShrinkDeathRow()
                    <li> cuddIsInDeathRow()
                    <li> cuddTimesInDeathRow()
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
// static char rcsid[] DD_UNUSED = "$Id: cuddRef.c,v 1.29 2012/02/05 01:07:19
// fabio Exp $"; #endif

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

  Synopsis [Increases the reference count of a node, if it is not
  saturated.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_RecursiveDeref Cudd_Deref]

******************************************************************************/
void Cudd_Ref(DdNode *n) {

  n = Cudd_Regular(n);

  cuddSatInc(n->ref);

} /* end of Cudd_Ref */

/**Function********************************************************************

  Synopsis    [Decreases the reference count of node n.]

  Description [Decreases the reference count of node n. If n dies,
  recursively decreases the reference counts of its children.  It is
  used to dispose of a DD that is no longer needed.]

  SideEffects [None]

  SeeAlso     [Cudd_Deref Cudd_Ref Cudd_RecursiveDerefZdd]

******************************************************************************/
void Cudd_RecursiveDeref(DdManager *table, DdNode *n) {
  DdNode *N;
  int ord;
  DdNodePtr *stack = table->stack;
  int SP = 1;

  unsigned int live = table->keys - table->dead;
  if (live > table->peakLiveNodes) {
    table->peakLiveNodes = live;
  }

  N = Cudd_Regular(n);

  do {
#ifdef DD_DEBUG
    assert(N->ref != 0);
#endif

    if (N->ref == 1) {
      N->ref = 0;
      table->dead++;
#ifdef DD_STATS
      table->nodesDropped++;
#endif
      if (cuddIsConstant(N)) {
        table->constants.dead++;
        N = stack[--SP];
      } else {
        ord = table->perm[N->index];
        stack[SP++] = Cudd_Regular(cuddE(N));
        table->subtables[ord].dead++;
        N = cuddT(N);
      }
    } else {
      cuddSatDec(N->ref);
      N = stack[--SP];
    }
  } while (SP != 0);

} /* end of Cudd_RecursiveDeref */

/**Function********************************************************************

  Synopsis    [Decreases the reference count of BDD node n.]

  Description [Decreases the reference count of node n. If n dies,
  recursively decreases the reference counts of its children.  It is
  used to dispose of a BDD that is no longer needed. It is more
  efficient than Cudd_RecursiveDeref, but it cannot be used on
  ADDs. The greater efficiency comes from being able to assume that no
  constant node will ever die as a result of a call to this
  procedure.]

  SideEffects [None]

  SeeAlso     [Cudd_RecursiveDeref Cudd_DelayedDerefBdd]

******************************************************************************/
void Cudd_IterDerefBdd(DdManager *table, DdNode *n) {
  DdNode *N;
  int ord;
  DdNodePtr *stack = table->stack;
  int SP = 1;

  unsigned int live = table->keys - table->dead;
  if (live > table->peakLiveNodes) {
    table->peakLiveNodes = live;
  }

  N = Cudd_Regular(n);

  do {
#ifdef DD_DEBUG
    assert(N->ref != 0);
#endif

    if (N->ref == 1) {
      N->ref = 0;
      table->dead++;
#ifdef DD_STATS
      table->nodesDropped++;
#endif
      ord = table->perm[N->index];
      stack[SP++] = Cudd_Regular(cuddE(N));
      table->subtables[ord].dead++;
      N = cuddT(N);
    } else {
      cuddSatDec(N->ref);
      N = stack[--SP];
    }
  } while (SP != 0);

} /* end of Cudd_IterDerefBdd */

/**Function********************************************************************

  Synopsis    [Decreases the reference count of ZDD node n.]

  Description [Decreases the reference count of ZDD node n. If n dies,
  recursively decreases the reference counts of its children.  It is
  used to dispose of a ZDD that is no longer needed.]

  SideEffects [None]

  SeeAlso     [Cudd_Deref Cudd_Ref Cudd_RecursiveDeref]

******************************************************************************/
void Cudd_RecursiveDerefZdd(DdManager *table, DdNode *n) {
  DdNode *N;
  int ord;
  DdNodePtr *stack = table->stack;
  int SP = 1;

  N = n;

  do {
#ifdef DD_DEBUG
    assert(N->ref != 0);
#endif

    cuddSatDec(N->ref);

    if (N->ref == 0) {
      table->deadZ++;
#ifdef DD_STATS
      table->nodesDropped++;
#endif
#ifdef DD_DEBUG
      assert(!cuddIsConstant(N));
#endif
      ord = table->permZ[N->index];
      stack[SP++] = cuddE(N);
      table->subtableZ[ord].dead++;
      N = cuddT(N);
    } else {
      N = stack[--SP];
    }
  } while (SP != 0);

} /* end of Cudd_RecursiveDerefZdd */

/**Function********************************************************************

  Synopsis    [Decreases the reference count of node.]

  Description [Decreases the reference count of node. It is primarily
  used in recursive procedures to decrease the ref count of a result
  node before returning it. This accomplishes the goal of removing the
  protection applied by a previous Cudd_Ref.]

  SideEffects [None]

  SeeAlso     [Cudd_RecursiveDeref Cudd_RecursiveDerefZdd Cudd_Ref]

******************************************************************************/
void Cudd_Deref(DdNode *node) {
  node = Cudd_Regular(node);
  cuddSatDec(node->ref);

} /* end of Cudd_Deref */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Brings children of a dead node back.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddReclaimZdd]

******************************************************************************/
void cuddReclaim(DdManager *table, DdNode *n) {
  DdNode *N;
  int ord;
  DdNodePtr *stack = table->stack;
  int SP = 1;
  double initialDead = table->dead;

  N = Cudd_Regular(n);

#ifdef DD_DEBUG
  assert(N->ref == 0);
#endif

  do {
    if (N->ref == 0) {
      N->ref = 1;
      table->dead--;
      if (cuddIsConstant(N)) {
        table->constants.dead--;
        N = stack[--SP];
      } else {
        ord = table->perm[N->index];
        stack[SP++] = Cudd_Regular(cuddE(N));
        table->subtables[ord].dead--;
        N = cuddT(N);
      }
    } else {
      cuddSatInc(N->ref);
      N = stack[--SP];
    }
  } while (SP != 0);

  N = Cudd_Regular(n);
  cuddSatDec(N->ref);
  table->reclaimed += initialDead - table->dead;

} /* end of cuddReclaim */

/**Function********************************************************************

  Synopsis    [Brings children of a dead ZDD node back.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddReclaim]

******************************************************************************/
void cuddReclaimZdd(DdManager *table, DdNode *n) {
  DdNode *N;
  int ord;
  DdNodePtr *stack = table->stack;
  int SP = 1;

  N = n;

#ifdef DD_DEBUG
  assert(N->ref == 0);
#endif

  do {
    cuddSatInc(N->ref);

    if (N->ref == 1) {
      table->deadZ--;
      table->reclaimed++;
#ifdef DD_DEBUG
      assert(!cuddIsConstant(N));
#endif
      ord = table->permZ[N->index];
      stack[SP++] = cuddE(N);
      table->subtableZ[ord].dead--;
      N = cuddT(N);
    } else {
      N = stack[--SP];
    }
  } while (SP != 0);

  cuddSatDec(n->ref);

} /* end of cuddReclaimZdd */

/**Function********************************************************************

  Synopsis    [Shrinks the death row.]

  Description [Shrinks the death row by a factor of four.]

  SideEffects [None]

  SeeAlso     [cuddClearDeathRow]

******************************************************************************/
void cuddShrinkDeathRow(DdManager *table) {
#ifndef DD_NO_DEATH_ROW
  int i;

  if (table->deathRowDepth > 3) {
    for (i = table->deathRowDepth / 4; i < table->deathRowDepth; i++) {
      if (table->deathRow[i] == NULL)
        break;
      Cudd_IterDerefBdd(table, table->deathRow[i]);
      table->deathRow[i] = NULL;
    }
    table->deathRowDepth /= 4;
    table->deadMask = table->deathRowDepth - 1;
    if ((unsigned)table->nextDead > table->deadMask) {
      table->nextDead = 0;
    }
    table->deathRow = REALLOC(DdNodePtr, table->deathRow, table->deathRowDepth);
  }
#endif

} /* end of cuddShrinkDeathRow */

/**Function********************************************************************

  Synopsis    [Clears the death row.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_DelayedDerefBdd Cudd_IterDerefBdd Cudd_CheckZeroRef
  cuddGarbageCollect]

******************************************************************************/
void cuddClearDeathRow(DdManager *table) {
#ifndef DD_NO_DEATH_ROW
  int i;

  for (i = 0; i < table->deathRowDepth; i++) {
    if (table->deathRow[i] == NULL)
      break;
    Cudd_IterDerefBdd(table, table->deathRow[i]);
    table->deathRow[i] = NULL;
  }
#ifdef DD_DEBUG
  for (; i < table->deathRowDepth; i++) {
    assert(table->deathRow[i] == NULL);
  }
#endif
  table->nextDead = 0;
#endif

} /* end of cuddClearDeathRow */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
/**CFile***********************************************************************

  FileName    [cuddReorder.c]

  PackageName [cudd]

  Synopsis    [Functions for dynamic variable reordering.]

  Description [External procedures included in this file:
                <ul>
                <li> Cudd_ReduceHeap()
                <li> Cudd_ShuffleHeap()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddDynamicAllocNode()
                <li> cuddSifting()
                <li> cuddSwapping()
                <li> cuddNextHigh()
                <li> cuddNextLow()
                <li> cuddSwapInPlace()
                <li> cuddBddAlignToZdd()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> ddUniqueCompare()
                <li> ddSwapAny()
                <li> ddSiftingAux()
                <li> ddSiftingUp()
                <li> ddSiftingDown()
                <li> ddSiftingBackward()
                <li> ddReorderPreprocess()
                <li> ddReorderPostprocess()
                <li> ddShuffle()
                <li> ddSiftUp()
                <li> bddFixTree()
                </ul>]

  Author      [Shipra Panda, Bernard Plessier, Fabio Somenzi]

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

#define DD_MAX_SUBTABLE_SPARSITY 8

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
// static char rcsid[] DD_UNUSED = "$Id: cuddReorder.c,v 1.71 2012/02/05
// 01:07:19 fabio Exp $"; #endif

static int *entry;

int ddTotalNumberSwapping;
#ifdef DD_STATS
int ddTotalNISwaps;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddUniqueCompare(int *ptrX, int *ptrY);
static Move *ddSwapAny(DdManager *table, int x, int y);
static int ddSiftingAux(DdManager *table, int x, int xLow, int xHigh);
static Move *ddSiftingUp(DdManager *table, int y, int xLow);
static Move *ddSiftingDown(DdManager *table, int x, int xHigh);
static int ddSiftingBackward(DdManager *table, int size, Move *moves);
static int ddReorderPreprocess(DdManager *table);
static int ddReorderPostprocess(DdManager *table);
static int ddShuffle2(DdManager *table, int *permutation);
static int ddSiftUp2(DdManager *table, int x, int xLow);
static void bddFixTree(DdManager *table, MtrNode *treenode);
static int ddUpdateMtrTree(DdManager *table, MtrNode *treenode, int *perm,
                           int *invperm);
static int ddCheckPermuation(DdManager *table, MtrNode *treenode, int *perm,
                             int *invperm);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Main dynamic reordering routine.]

  Description [Main dynamic reordering routine.
  Calls one of the possible reordering procedures:
  <ul>
  <li>Swapping
  <li>Sifting
  <li>Symmetric Sifting
  <li>Group Sifting
  <li>Window Permutation
  <li>Simulated Annealing
  <li>Genetic Algorithm
  <li>Dynamic Programming (exact)
  </ul>

  For sifting, symmetric sifting, group sifting, and window
  permutation it is possible to request reordering to convergence.<p>

  The core of all methods is the reordering procedure
  cuddSwapInPlace() which swaps two adjacent variables and is based
  on Rudell's paper.
  Returns 1 in case of success; 0 otherwise. In the case of symmetric
  sifting (with and without convergence) returns 1 plus the number of
  symmetric variables, in case of success.]

  SideEffects [Changes the variable order for all diagrams and clears
  the cache.]

******************************************************************************/
int Cudd_ReduceHeap(
    DdManager *table /* DD manager */,
    Cudd_ReorderingType heuristic /* method used for reordering */,
    int minsize /* bound below which no reordering occurs */) {
  DdHook *hook;
  int result;
  unsigned int nextDyn;
#ifdef DD_STATS
  unsigned int initialSize;
  unsigned int finalSize;
#endif
  unsigned long localTime;

  /* Don't reorder if there are too many dead nodes. */
  if (table->keys - table->dead < (unsigned)minsize)
    return (1);

  if (heuristic == CUDD_REORDER_SAME) {
    heuristic = table->autoMethod;
  }
  if (heuristic == CUDD_REORDER_NONE) {
    return (1);
  }

  /* This call to Cudd_ReduceHeap does initiate reordering. Therefore
  ** we count it.
  */
  table->reorderings++;

  localTime = util_cpu_time();

  /* Run the hook functions. */
  hook = table->preReorderingHook;
  while (hook != NULL) {
    int res = (hook->f)(table, "BDD", (void *)heuristic);
    if (res == 0)
      return (0);
    hook = hook->next;
  }

  if (!ddReorderPreprocess(table))
    return (0);
  ddTotalNumberSwapping = 0;

  if (table->keys > table->peakLiveNodes) {
    table->peakLiveNodes = table->keys;
  }
#ifdef DD_STATS
  initialSize = table->keys - table->isolated;
  ddTotalNISwaps = 0;

  switch (heuristic) {
  case CUDD_REORDER_RANDOM:
  case CUDD_REORDER_RANDOM_PIVOT:
    (void)fprintf(table->out, "#:I_RANDOM  ");
    break;
  case CUDD_REORDER_SIFT:
  case CUDD_REORDER_SIFT_CONVERGE:
  case CUDD_REORDER_SYMM_SIFT:
  case CUDD_REORDER_SYMM_SIFT_CONV:
  case CUDD_REORDER_GROUP_SIFT:
  case CUDD_REORDER_GROUP_SIFT_CONV:
    (void)fprintf(table->out, "#:I_SIFTING ");
    break;
  case CUDD_REORDER_WINDOW2:
  case CUDD_REORDER_WINDOW3:
  case CUDD_REORDER_WINDOW4:
  case CUDD_REORDER_WINDOW2_CONV:
  case CUDD_REORDER_WINDOW3_CONV:
  case CUDD_REORDER_WINDOW4_CONV:
    (void)fprintf(table->out, "#:I_WINDOW  ");
    break;
  case CUDD_REORDER_ANNEALING:
    (void)fprintf(table->out, "#:I_ANNEAL  ");
    break;
  case CUDD_REORDER_GENETIC:
    (void)fprintf(table->out, "#:I_GENETIC ");
    break;
  case CUDD_REORDER_LINEAR:
  case CUDD_REORDER_LINEAR_CONVERGE:
    (void)fprintf(table->out, "#:I_LINSIFT ");
    break;
  case CUDD_REORDER_EXACT:
    (void)fprintf(table->out, "#:I_EXACT   ");
    break;
  default:
    return (0);
  }
  (void)fprintf(table->out, "%8d: initial size", initialSize);
#endif

  /* See if we should use alternate threshold for maximum growth. */
  if (table->reordCycle && table->reorderings % table->reordCycle == 0) {
    double saveGrowth = table->maxGrowth;
    table->maxGrowth = table->maxGrowthAlt;
    result = cuddTreeSifting(table, heuristic);
    table->maxGrowth = saveGrowth;
  } else {
    result = cuddTreeSifting(table, heuristic);
  }

#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
  finalSize = table->keys - table->isolated;
  (void)fprintf(table->out, "#:F_REORDER %8d: final size\n", finalSize);
  (void)fprintf(table->out, "#:T_REORDER %8g: total time (sec)\n",
                ((double)(util_cpu_time() - localTime) / 1000.0));
  (void)fprintf(table->out, "#:N_REORDER %8d: total swaps\n",
                ddTotalNumberSwapping);
  (void)fprintf(table->out, "#:M_REORDER %8d: NI swaps\n", ddTotalNISwaps);
#endif

  if (result == 0)
    return (0);

  if (!ddReorderPostprocess(table))
    return (0);

  if (table->realign) {
    if (!cuddZddAlignToBdd(table))
      return (0);
  }

  nextDyn = (table->keys - table->constants.keys + 1) * DD_DYN_RATIO +
            table->constants.keys;
  if (table->reorderings < 20 || nextDyn > table->nextDyn)
    table->nextDyn = nextDyn;
  else
    table->nextDyn += 20;
  if (table->randomizeOrder != 0) {
    table->nextDyn += Cudd_Random() & table->randomizeOrder;
  }
  table->reordered = 1;

  /* Run hook functions. */
  hook = table->postReorderingHook;
  while (hook != NULL) {
    int res = (hook->f)(table, "BDD", (void *)localTime);
    if (res == 0)
      return (0);
    hook = hook->next;
  }
  /* Update cumulative reordering time. */
  table->reordTime += util_cpu_time() - localTime;

  return (result);

} /* end of Cudd_ReduceHeap */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Dynamically allocates a Node.]

  Description [Dynamically allocates a Node. This procedure is similar
  to cuddAllocNode in Cudd_Table.c, but it does not attempt garbage
  collection, because during reordering there are no dead nodes.
  Returns a pointer to a new node if successful; NULL is memory is
  full.]

  SideEffects [None]

  SeeAlso     [cuddAllocNode]

******************************************************************************/
DdNode *cuddDynamicAllocNode(DdManager *table) {
  int i;
  DdNodePtr *mem;
  DdNode *list, *node;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  if (table->nextFree == NULL) { /* free list is empty */
    /* Try to allocate a new block. */
    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    mem = (DdNodePtr *)ALLOC(DdNode, DD_MEM_CHUNK + 1);
    MMoutOfMemory = saveHandler;
    if (mem == NULL && table->stash != NULL) {
      FREE(table->stash);
      table->stash = NULL;
      /* Inhibit resizing of tables. */
      table->maxCacheHard = table->cacheSlots - 1;
      table->cacheSlack = -(int)(table->cacheSlots + 1);
      for (i = 0; i < table->size; i++) {
        table->subtables[i].maxKeys <<= 2;
      }
      mem = (DdNodePtr *)ALLOC(DdNode, DD_MEM_CHUNK + 1);
    }
    if (mem == NULL) {
      /* Out of luck. Call the default handler to do
      ** whatever it specifies for a failed malloc.  If this
      ** handler returns, then set error code, print
      ** warning, and return. */
      (*MMoutOfMemory)(sizeof(DdNode) * (DD_MEM_CHUNK + 1));
      table->errorCode = CUDD_MEMORY_OUT;
#ifdef DD_VERBOSE
      (void)fprintf(table->err, "cuddDynamicAllocNode: out of memory");
      (void)fprintf(table->err, "Memory in use = %lu\n", table->memused);
#endif
      return (NULL);
    } else { /* successful allocation; slice memory */
      unsigned long offset;
      table->memused += (DD_MEM_CHUNK + 1) * sizeof(DdNode);
      mem[0] = (DdNode *)table->memoryList;
      table->memoryList = mem;

      /* Here we rely on the fact that the size of a DdNode is a
      ** power of 2 and a multiple of the size of a pointer.
      ** If we align one node, all the others will be aligned
      ** as well. */
      offset = (unsigned long)mem & (sizeof(DdNode) - 1);
      mem += (sizeof(DdNode) - offset) / sizeof(DdNodePtr);
#ifdef DD_DEBUG
      assert(((unsigned long)mem & (sizeof(DdNode) - 1)) == 0);
#endif
      list = (DdNode *)mem;

      i = 1;
      do {
        list[i - 1].ref = 0;
        list[i - 1].next = &list[i];
      } while (++i < DD_MEM_CHUNK);

      list[DD_MEM_CHUNK - 1].ref = 0;
      list[DD_MEM_CHUNK - 1].next = NULL;

      table->nextFree = &list[0];
    }
  } /* if free list empty */

  node = table->nextFree;
  table->nextFree = node->next;
  return (node);

} /* end of cuddDynamicAllocNode */

/**Function********************************************************************

  Synopsis    [Implementation of Rudell's sifting algorithm.]

  Description [Implementation of Rudell's sifting algorithm.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries
    in each unique table.
    <li> Sift the variable up and down, remembering each time the
    total size of the DD heap.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddSifting(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int size;
  int x;
  int result;
#ifdef DD_STATS
  int previousSize;
#endif

  size = table->size;

  /* Find order in which to sift variables. */
  var = NULL;
  entry = ALLOC(int, size);
  if (entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddSiftingOutOfMem;
  }
  var = ALLOC(int, size);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddSiftingOutOfMem;
  }

  for (i = 0; i < size; i++) {
    x = table->perm[i];
    entry[i] = table->subtables[x].keys;
    var[i] = i;
  }

  qsort((void *)var, size, sizeof(int), (DD_QSFP)ddUniqueCompare);

  /* Now sift. */
  for (i = 0; i < ddMin(table->siftMaxVar, size); i++) {
    if (ddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (util_cpu_time() - table->startTime + table->reordTime >
        table->timeLimit) {
      table->autoDyn = 0; /* prevent further reordering */
      break;
    }
    x = table->perm[var[i]];

    if (x < lower || x > upper || table->subtables[x].bindVar == 1)
      continue;
#ifdef DD_STATS
    previousSize = table->keys - table->isolated;
#endif
    result = ddSiftingAux(table, x, lower, upper);
    if (!result)
      goto cuddSiftingOutOfMem;
#ifdef DD_STATS
    if (table->keys < (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "-");
    } else if (table->keys > (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "+"); /* should never happen */
      (void)fprintf(
          table->err,
          "\nSize increased from %d to %d while sifting variable %d\n",
          previousSize, table->keys - table->isolated, var[i]);
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

  FREE(var);
  FREE(entry);

  return (1);

cuddSiftingOutOfMem:

  if (entry != NULL)
    FREE(entry);
  if (var != NULL)
    FREE(var);

  return (0);

} /* end of cuddSifting */

/**Function********************************************************************

  Synopsis    [Reorders variables by a sequence of (non-adjacent) swaps.]

  Description [Implementation of Plessier's algorithm that reorders
  variables by a sequence of (non-adjacent) swaps.
    <ol>
    <li> Select two variables (RANDOM or HEURISTIC).
    <li> Permute these variables.
    <li> If the nodes have decreased accept the permutation.
    <li> Otherwise reconstruct the original heap.
    <li> Loop.
    </ol>
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddSwapping(DdManager *table, int lower, int upper,
                 Cudd_ReorderingType heuristic) {
  int i, j;
  int max, keys;
  int nvars;
  int x, y;
  int iterate;
  int previousSize;
  Move *moves, *move;
  int pivot;
  int modulo;
  int result;

#ifdef DD_DEBUG
  /* Sanity check */
  assert(lower >= 0 && upper < table->size && lower <= upper);
#endif

  nvars = upper - lower + 1;
  iterate = nvars;

  for (i = 0; i < iterate; i++) {
    if (ddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (heuristic == CUDD_REORDER_RANDOM_PIVOT) {
      max = -1;
      for (j = lower; j <= upper; j++) {
        if ((keys = table->subtables[j].keys) > max) {
          max = keys;
          pivot = j;
        }
      }

      modulo = upper - pivot;
      if (modulo == 0) {
        y = pivot;
      } else {
        y = pivot + 1 + ((int)Cudd_Random() % modulo);
      }

      modulo = pivot - lower - 1;
      if (modulo < 1) {
        x = lower;
      } else {
        do {
          x = (int)Cudd_Random() % modulo;
        } while (x == y);
      }
    } else {
      x = ((int)Cudd_Random() % nvars) + lower;
      do {
        y = ((int)Cudd_Random() % nvars) + lower;
      } while (x == y);
    }
    previousSize = table->keys - table->isolated;
    moves = ddSwapAny(table, x, y);
    if (moves == NULL)
      goto cuddSwappingOutOfMem;
    result = ddSiftingBackward(table, previousSize, moves);
    if (!result)
      goto cuddSwappingOutOfMem;
    while (moves != NULL) {
      move = moves->next;
      cuddDeallocMove(table, moves);
      moves = move;
    }
#ifdef DD_STATS
    if (table->keys < (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "-");
    } else if (table->keys > (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "+"); /* should never happen */
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
#if 0
        (void) fprintf(table->out,"#:t_SWAPPING %8d: tmp size\n",
		       table->keys - table->isolated);
#endif
  }

  return (1);

cuddSwappingOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }

  return (0);

} /* end of cuddSwapping */

/**Function********************************************************************

  Synopsis    [Finds the next subtable with a larger index.]

  Description [Finds the next subtable with a larger index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     [cuddNextLow]

******************************************************************************/
int cuddNextHigh(DdManager *table, int x) {
  return (x + 1);

} /* end of cuddNextHigh */

/**Function********************************************************************

  Synopsis    [Finds the next subtable with a smaller index.]

  Description [Finds the next subtable with a smaller index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     [cuddNextHigh]

******************************************************************************/
int cuddNextLow(DdManager *table, int x) {
  return (x - 1);

} /* end of cuddNextLow */

/**Function********************************************************************

  Synopsis    [Swaps two adjacent variables.]

  Description [Swaps two adjacent variables. It assumes that no dead
  nodes are present on entry to this procedure.  The procedure then
  guarantees that no dead nodes will be present when it terminates.
  cuddSwapInPlace assumes that x &lt; y.  Returns the number of keys in
  the table if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddSwapInPlace(DdManager *table, int x, int y) {
  DdNodePtr *xlist, *ylist;
  int xindex, yindex;
  int xslots, yslots;
  int xshift, yshift;
  int oldxkeys, oldykeys;
  int newxkeys, newykeys;
  int comple, newcomplement;
  int i;
  Cudd_VariableType varType;
  Cudd_LazyGroupType groupType;
  int posn;
  int isolated;
  DdNode *f, *f0, *f1, *f01, *f00, *f11, *f10, *newf1, *newf0;
  DdNode *g, *next;
  DdNodePtr *previousP;
  DdNode *tmp;
  DdNode *sentinel = &(table->sentinel);
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

#ifdef DD_DEBUG
  int count, idcheck;
#endif

#ifdef DD_DEBUG
  assert(x < y);
  assert(cuddNextHigh(table, x) == y);
  assert(table->subtables[x].keys != 0);
  assert(table->subtables[y].keys != 0);
  assert(table->subtables[x].dead == 0);
  assert(table->subtables[y].dead == 0);
#endif

  ddTotalNumberSwapping++;

  /* Get parameters of x subtable. */
  xindex = table->invperm[x];
  xlist = table->subtables[x].nodelist;
  oldxkeys = table->subtables[x].keys;
  xslots = table->subtables[x].slots;
  xshift = table->subtables[x].shift;

  /* Get parameters of y subtable. */
  yindex = table->invperm[y];
  ylist = table->subtables[y].nodelist;
  oldykeys = table->subtables[y].keys;
  yslots = table->subtables[y].slots;
  yshift = table->subtables[y].shift;

  if (!cuddTestInteract(table, xindex, yindex)) {
#ifdef DD_STATS
    ddTotalNISwaps++;
#endif
    newxkeys = oldxkeys;
    newykeys = oldykeys;
  } else {
    newxkeys = 0;
    newykeys = oldykeys;

    /* Check whether the two projection functions involved in this
    ** swap are isolated. At the end, we'll be able to tell how many
    ** isolated projection functions are there by checking only these
    ** two functions again. This is done to eliminate the isolated
    ** projection functions from the node count.
    */
    isolated =
        -((table->vars[xindex]->ref == 1) + (table->vars[yindex]->ref == 1));

    /* The nodes in the x layer that do not depend on
    ** y will stay there; the others are put in a chain.
    ** The chain is handled as a LIFO; g points to the beginning.
    */
    g = NULL;
    if ((oldxkeys >= xslots || (unsigned)xslots == table->initSlots) &&
        oldxkeys <= DD_MAX_SUBTABLE_DENSITY * xslots) {
      for (i = 0; i < xslots; i++) {
        previousP = &(xlist[i]);
        f = *previousP;
        while (f != sentinel) {
          next = f->next;
          f1 = cuddT(f);
          f0 = cuddE(f);
          if (f1->index != (DdHalfWord)yindex &&
              Cudd_Regular(f0)->index != (DdHalfWord)yindex) {
            /* stays */
            newxkeys++;
            *previousP = f;
            previousP = &(f->next);
          } else {
            f->index = yindex;
            f->next = g;
            g = f;
          }
          f = next;
        } /* while there are elements in the collision chain */
        *previousP = sentinel;
      }      /* for each slot of the x subtable */
    } else { /* resize xlist */
      DdNode *h = NULL;
      DdNodePtr *newxlist;
      unsigned int newxslots;
      int newxshift;
      /* Empty current xlist. Nodes that stay go to list h;
      ** nodes that move go to list g. */
      for (i = 0; i < xslots; i++) {
        f = xlist[i];
        while (f != sentinel) {
          next = f->next;
          f1 = cuddT(f);
          f0 = cuddE(f);
          if (f1->index != (DdHalfWord)yindex &&
              Cudd_Regular(f0)->index != (DdHalfWord)yindex) {
            /* stays */
            f->next = h;
            h = f;
            newxkeys++;
          } else {
            f->index = yindex;
            f->next = g;
            g = f;
          }
          f = next;
        } /* while there are elements in the collision chain */
      }   /* for each slot of the x subtable */
      /* Decide size of new subtable. */
      newxshift = xshift;
      newxslots = xslots;
      while ((unsigned)oldxkeys > DD_MAX_SUBTABLE_DENSITY * newxslots) {
        newxshift--;
        newxslots <<= 1;
      }
      while ((unsigned)oldxkeys < newxslots && newxslots > table->initSlots) {
        newxshift++;
        newxslots >>= 1;
      }
      /* Try to allocate new table. Be ready to back off. */
      saveHandler = MMoutOfMemory;
      MMoutOfMemory = Cudd_OutOfMem;
      newxlist = ALLOC(DdNodePtr, newxslots);
      MMoutOfMemory = saveHandler;
      if (newxlist == NULL) {
        (void)fprintf(table->err,
                      "Unable to resize subtable %d for lack of memory\n", i);
        newxlist = xlist;
        newxslots = xslots;
        newxshift = xshift;
      } else {
        table->slots += ((int)newxslots - xslots);
        table->minDead = (unsigned)(table->gcFrac * (double)table->slots);
        table->cacheSlack =
            (int)ddMin(table->maxCacheHard,
                       DD_MAX_CACHE_TO_SLOTS_RATIO * table->slots) -
            2 * (int)table->cacheSlots;
        table->memused += ((int)newxslots - xslots) * sizeof(DdNodePtr);
        FREE(xlist);
        xslots = newxslots;
        xshift = newxshift;
        xlist = newxlist;
      }
      /* Initialize new subtable. */
      for (i = 0; i < xslots; i++) {
        xlist[i] = sentinel;
      }
      /* Move nodes that were parked in list h to their new home. */
      f = h;
      while (f != NULL) {
        next = f->next;
        f1 = cuddT(f);
        f0 = cuddE(f);
        /* Check xlist for pair (f11,f01). */
        posn = ddHash(f1, f0, xshift);
        /* For each element tmp in collision list xlist[posn]. */
        previousP = &(xlist[posn]);
        tmp = *previousP;
        while (f1 < cuddT(tmp)) {
          previousP = &(tmp->next);
          tmp = *previousP;
        }
        while (f1 == cuddT(tmp) && f0 < cuddE(tmp)) {
          previousP = &(tmp->next);
          tmp = *previousP;
        }
        f->next = *previousP;
        *previousP = f;
        f = next;
      }
    }

#ifdef DD_COUNT
    table->swapSteps += oldxkeys - newxkeys;
#endif
    /* Take care of the x nodes that must be re-expressed.
    ** They form a linked list pointed by g. Their index has been
    ** already changed to yindex.
    */
    f = g;
    while (f != NULL) {
      next = f->next;
      /* Find f1, f0, f11, f10, f01, f00. */
      f1 = cuddT(f);
#ifdef DD_DEBUG
      assert(!(Cudd_IsComplement(f1)));
#endif
      if ((int)f1->index == yindex) {
        f11 = cuddT(f1);
        f10 = cuddE(f1);
      } else {
        f11 = f10 = f1;
      }
#ifdef DD_DEBUG
      assert(!(Cudd_IsComplement(f11)));
#endif
      f0 = cuddE(f);
      comple = Cudd_IsComplement(f0);
      f0 = Cudd_Regular(f0);
      if ((int)f0->index == yindex) {
        f01 = cuddT(f0);
        f00 = cuddE(f0);
      } else {
        f01 = f00 = f0;
      }
      if (comple) {
        f01 = Cudd_Not(f01);
        f00 = Cudd_Not(f00);
      }
      /* Decrease ref count of f1. */
      cuddSatDec(f1->ref);
      /* Create the new T child. */
      if (f11 == f01) {
        newf1 = f11;
        cuddSatInc(newf1->ref);
      } else {
        /* Check xlist for triple (xindex,f11,f01). */
        posn = ddHash(f11, f01, xshift);
        /* For each element newf1 in collision list xlist[posn]. */
        previousP = &(xlist[posn]);
        newf1 = *previousP;
        while (f11 < cuddT(newf1)) {
          previousP = &(newf1->next);
          newf1 = *previousP;
        }
        while (f11 == cuddT(newf1) && f01 < cuddE(newf1)) {
          previousP = &(newf1->next);
          newf1 = *previousP;
        }
        if (cuddT(newf1) == f11 && cuddE(newf1) == f01) {
          cuddSatInc(newf1->ref);
        } else { /* no match */
          newf1 = cuddDynamicAllocNode(table);
          if (newf1 == NULL)
            goto cuddSwapOutOfMem;
          newf1->index = xindex;
          newf1->ref = 1;
          cuddT(newf1) = f11;
          cuddE(newf1) = f01;
          /* Insert newf1 in the collision list xlist[posn];
          ** increase the ref counts of f11 and f01.
          */
          newxkeys++;
          newf1->next = *previousP;
          *previousP = newf1;
          cuddSatInc(f11->ref);
          tmp = Cudd_Regular(f01);
          cuddSatInc(tmp->ref);
        }
      }
      cuddT(f) = newf1;
#ifdef DD_DEBUG
      assert(!(Cudd_IsComplement(newf1)));
#endif

      /* Do the same for f0, keeping complement dots into account. */
      /* Decrease ref count of f0. */
      tmp = Cudd_Regular(f0);
      cuddSatDec(tmp->ref);
      /* Create the new E child. */
      if (f10 == f00) {
        newf0 = f00;
        tmp = Cudd_Regular(newf0);
        cuddSatInc(tmp->ref);
      } else {
        /* make sure f10 is regular */
        newcomplement = Cudd_IsComplement(f10);
        if (newcomplement) {
          f10 = Cudd_Not(f10);
          f00 = Cudd_Not(f00);
        }
        /* Check xlist for triple (xindex,f10,f00). */
        posn = ddHash(f10, f00, xshift);
        /* For each element newf0 in collision list xlist[posn]. */
        previousP = &(xlist[posn]);
        newf0 = *previousP;
        while (f10 < cuddT(newf0)) {
          previousP = &(newf0->next);
          newf0 = *previousP;
        }
        while (f10 == cuddT(newf0) && f00 < cuddE(newf0)) {
          previousP = &(newf0->next);
          newf0 = *previousP;
        }
        if (cuddT(newf0) == f10 && cuddE(newf0) == f00) {
          cuddSatInc(newf0->ref);
        } else { /* no match */
          newf0 = cuddDynamicAllocNode(table);
          if (newf0 == NULL)
            goto cuddSwapOutOfMem;
          newf0->index = xindex;
          newf0->ref = 1;
          cuddT(newf0) = f10;
          cuddE(newf0) = f00;
          /* Insert newf0 in the collision list xlist[posn];
          ** increase the ref counts of f10 and f00.
          */
          newxkeys++;
          newf0->next = *previousP;
          *previousP = newf0;
          cuddSatInc(f10->ref);
          tmp = Cudd_Regular(f00);
          cuddSatInc(tmp->ref);
        }
        if (newcomplement) {
          newf0 = Cudd_Not(newf0);
        }
      }
      cuddE(f) = newf0;

      /* Insert the modified f in ylist.
      ** The modified f does not already exists in ylist.
      ** (Because of the uniqueness of the cofactors.)
      */
      posn = ddHash(newf1, newf0, yshift);
      newykeys++;
      previousP = &(ylist[posn]);
      tmp = *previousP;
      while (newf1 < cuddT(tmp)) {
        previousP = &(tmp->next);
        tmp = *previousP;
      }
      while (newf1 == cuddT(tmp) && newf0 < cuddE(tmp)) {
        previousP = &(tmp->next);
        tmp = *previousP;
      }
      f->next = *previousP;
      *previousP = f;
      f = next;
    } /* while f != NULL */

    /* GC the y layer. */

    /* For each node f in ylist. */
    for (i = 0; i < yslots; i++) {
      previousP = &(ylist[i]);
      f = *previousP;
      while (f != sentinel) {
        next = f->next;
        if (f->ref == 0) {
          tmp = cuddT(f);
          cuddSatDec(tmp->ref);
          tmp = Cudd_Regular(cuddE(f));
          cuddSatDec(tmp->ref);
          cuddDeallocNode(table, f);
          newykeys--;
        } else {
          *previousP = f;
          previousP = &(f->next);
        }
        f = next;
      } /* while f */
      *previousP = sentinel;
    } /* for i */

#ifdef DD_DEBUG
#if 0
	(void) fprintf(table->out,"Swapping %d and %d\n",x,y);
#endif
    count = 0;
    idcheck = 0;
    for (i = 0; i < yslots; i++) {
      f = ylist[i];
      while (f != sentinel) {
        count++;
        if (f->index != (DdHalfWord)yindex)
          idcheck++;
        f = f->next;
      }
    }
    if (count != newykeys) {
      (void)fprintf(table->out,
                    "Error in finding newykeys\toldykeys = %d\tnewykeys = "
                    "%d\tactual = %d\n",
                    oldykeys, newykeys, count);
    }
    if (idcheck != 0)
      (void)fprintf(table->out, "Error in id's of ylist\twrong id's = %d\n",
                    idcheck);
    count = 0;
    idcheck = 0;
    for (i = 0; i < xslots; i++) {
      f = xlist[i];
      while (f != sentinel) {
        count++;
        if (f->index != (DdHalfWord)xindex)
          idcheck++;
        f = f->next;
      }
    }
    if (count != newxkeys) {
      (void)fprintf(table->out,
                    "Error in finding newxkeys\toldxkeys = %d \tnewxkeys = %d "
                    "\tactual = %d\n",
                    oldxkeys, newxkeys, count);
    }
    if (idcheck != 0)
      (void)fprintf(table->out, "Error in id's of xlist\twrong id's = %d\n",
                    idcheck);
#endif

    isolated +=
        (table->vars[xindex]->ref == 1) + (table->vars[yindex]->ref == 1);
    table->isolated += isolated;
  }

  /* Set the appropriate fields in table. */
  table->subtables[x].nodelist = ylist;
  table->subtables[x].slots = yslots;
  table->subtables[x].shift = yshift;
  table->subtables[x].keys = newykeys;
  table->subtables[x].maxKeys = yslots * DD_MAX_SUBTABLE_DENSITY;
  i = table->subtables[x].bindVar;
  table->subtables[x].bindVar = table->subtables[y].bindVar;
  table->subtables[y].bindVar = i;
  /* Adjust filds for lazy sifting. */
  varType = table->subtables[x].varType;
  table->subtables[x].varType = table->subtables[y].varType;
  table->subtables[y].varType = varType;
  i = table->subtables[x].pairIndex;
  table->subtables[x].pairIndex = table->subtables[y].pairIndex;
  table->subtables[y].pairIndex = i;
  i = table->subtables[x].varHandled;
  table->subtables[x].varHandled = table->subtables[y].varHandled;
  table->subtables[y].varHandled = i;
  groupType = table->subtables[x].varToBeGrouped;
  table->subtables[x].varToBeGrouped = table->subtables[y].varToBeGrouped;
  table->subtables[y].varToBeGrouped = groupType;

  table->subtables[y].nodelist = xlist;
  table->subtables[y].slots = xslots;
  table->subtables[y].shift = xshift;
  table->subtables[y].keys = newxkeys;
  table->subtables[y].maxKeys = xslots * DD_MAX_SUBTABLE_DENSITY;

  table->perm[xindex] = y;
  table->perm[yindex] = x;
  table->invperm[x] = yindex;
  table->invperm[y] = xindex;

  table->keys += newxkeys + newykeys - oldxkeys - oldykeys;

  return (table->keys - table->isolated);

cuddSwapOutOfMem:
  (void)fprintf(table->err, "Error: cuddSwapInPlace out of memory\n");

  return (0);

} /* end of cuddSwapInPlace */

/**Function********************************************************************

  Synopsis    [Reorders BDD variables according to the order of the ZDD
  variables.]

  Description [Reorders BDD variables according to the order of the
  ZDD variables. This function can be called at the end of ZDD
  reordering to insure that the order of the BDD variables is
  consistent with the order of the ZDD variables. The number of ZDD
  variables must be a multiple of the number of BDD variables. Let
  <code>M</code> be the ratio of the two numbers. cuddBddAlignToZdd
  then considers the ZDD variables from <code>M*i</code> to
  <code>(M+1)*i-1</code> as corresponding to BDD variable
  <code>i</code>.  This function should be normally called from
  Cudd_zddReduceHeap, which clears the cache.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [Changes the BDD variable order for all diagrams and performs
  garbage collection of the BDD unique table.]

  SeeAlso [Cudd_ShuffleHeap Cudd_zddReduceHeap]

******************************************************************************/
int cuddBddAlignToZdd(DdManager *table /* DD manager */) {
  int *invperm; /* permutation array */
  int M;        /* ratio of ZDD variables to BDD variables */
  int i;        /* loop index */
  int result;   /* return value */

  /* We assume that a ratio of 0 is OK. */
  if (table->size == 0)
    return (1);

  M = table->sizeZ / table->size;
  /* Check whether the number of ZDD variables is a multiple of the
  ** number of BDD variables.
  */
  if (M * table->size != table->sizeZ)
    return (0);
  /* Create and initialize the inverse permutation array. */
  invperm = ALLOC(int, table->size);
  if (invperm == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (i = 0; i < table->sizeZ; i += M) {
    int indexZ = table->invpermZ[i];
    int index = indexZ / M;
    invperm[i / M] = index;
  }
  /* Eliminate dead nodes. Do not scan the cache again, because we
  ** assume that Cudd_zddReduceHeap has already cleared it.
  */
  cuddGarbageCollect(table, 0);

  /* Initialize number of isolated projection functions. */
  table->isolated = 0;
  for (i = 0; i < table->size; i++) {
    if (table->vars[i]->ref == 1)
      table->isolated++;
  }

  /* Initialize the interaction matrix. */
  result = cuddInitInteract(table);
  if (result == 0)
    return (0);

  result = ddShuffle2(table, invperm);
  FREE(invperm);
  /* Free interaction matrix. */
  FREE(table->interact);
  /* Fix the BDD variable group tree. */
  bddFixTree(table, table->tree);
  return (result);

} /* end of cuddBddAlignToZdd */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the
  variables according to the number of keys in the subtables.
  Returns the difference in number of keys between the two
  variables being compared.]

  SideEffects [None]

******************************************************************************/
static int ddUniqueCompare(int *ptrX, int *ptrY) {
#if 0
    if (entry[*ptrY] == entry[*ptrX]) {
	return((*ptrX) - (*ptrY));
    }
#endif
  return (entry[*ptrY] - entry[*ptrX]);

} /* end of ddUniqueCompare */

/**Function********************************************************************

  Synopsis    [Swaps any two variables.]

  Description [Swaps any two variables. Returns the set of moves.]

  SideEffects [None]

******************************************************************************/
static Move *ddSwapAny(DdManager *table, int x, int y) {
  Move *move, *moves;
  int xRef, yRef;
  int xNext, yNext;
  int size;
  int limitSize;
  int tmp;

  if (x > y) {
    tmp = x;
    x = y;
    y = tmp;
  }

  xRef = x;
  yRef = y;

  xNext = cuddNextHigh(table, x);
  yNext = cuddNextLow(table, y);
  moves = NULL;
  limitSize = table->keys - table->isolated;

  for (;;) {
    if (xNext == yNext) {
      size = cuddSwapInPlace(table, x, xNext);
      if (size == 0)
        goto ddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSwapAnyOutOfMem;
      move->x = x;
      move->y = xNext;
      move->size = size;
      move->next = moves;
      moves = move;

      size = cuddSwapInPlace(table, yNext, y);
      if (size == 0)
        goto ddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSwapAnyOutOfMem;
      move->x = yNext;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;

      size = cuddSwapInPlace(table, x, xNext);
      if (size == 0)
        goto ddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSwapAnyOutOfMem;
      move->x = x;
      move->y = xNext;
      move->size = size;
      move->next = moves;
      moves = move;

      tmp = x;
      x = y;
      y = tmp;

    } else if (x == yNext) {

      size = cuddSwapInPlace(table, x, xNext);
      if (size == 0)
        goto ddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSwapAnyOutOfMem;
      move->x = x;
      move->y = xNext;
      move->size = size;
      move->next = moves;
      moves = move;

      tmp = x;
      x = y;
      y = tmp;

    } else {
      size = cuddSwapInPlace(table, x, xNext);
      if (size == 0)
        goto ddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSwapAnyOutOfMem;
      move->x = x;
      move->y = xNext;
      move->size = size;
      move->next = moves;
      moves = move;

      size = cuddSwapInPlace(table, yNext, y);
      if (size == 0)
        goto ddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSwapAnyOutOfMem;
      move->x = yNext;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;

      x = xNext;
      y = yNext;
    }

    xNext = cuddNextHigh(table, x);
    yNext = cuddNextLow(table, y);
    if (xNext > yRef)
      break;

    if ((double)size > table->maxGrowth * (double)limitSize)
      break;
    if (size < limitSize)
      limitSize = size;
  }
  if (yNext >= xRef) {
    size = cuddSwapInPlace(table, yNext, y);
    if (size == 0)
      goto ddSwapAnyOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddSwapAnyOutOfMem;
    move->x = yNext;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;
  }

  return (moves);

ddSwapAnyOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (NULL);

} /* end of ddSwapAny */

/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSiftingAux(DdManager *table, int x, int xLow, int xHigh) {

  Move *move;
  Move *moveUp;   /* list of up moves */
  Move *moveDown; /* list of down moves */
  int initialSize;
  int result;

  initialSize = table->keys - table->isolated;

  moveDown = NULL;
  moveUp = NULL;

  if (x == xLow) {
    moveDown = ddSiftingDown(table, x, xHigh);
    /* At this point x --> xHigh unless bounding occurred. */
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto ddSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddSiftingBackward(table, initialSize, moveDown);
    if (!result)
      goto ddSiftingAuxOutOfMem;

  } else if (x == xHigh) {
    moveUp = ddSiftingUp(table, x, xLow);
    /* At this point x --> xLow unless bounding occurred. */
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto ddSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddSiftingBackward(table, initialSize, moveUp);
    if (!result)
      goto ddSiftingAuxOutOfMem;

  } else if ((x - xLow) > (xHigh - x)) { /* must go down first: shorter */
    moveDown = ddSiftingDown(table, x, xHigh);
    /* At this point x --> xHigh unless bounding occurred. */
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto ddSiftingAuxOutOfMem;
    if (moveDown != NULL) {
      x = moveDown->y;
    }
    moveUp = ddSiftingUp(table, x, xLow);
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto ddSiftingAuxOutOfMem;
    /* Move backward and stop at best position */
    result = ddSiftingBackward(table, initialSize, moveUp);
    if (!result)
      goto ddSiftingAuxOutOfMem;

  } else { /* must go up first: shorter */
    moveUp = ddSiftingUp(table, x, xLow);
    /* At this point x --> xLow unless bounding occurred. */
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto ddSiftingAuxOutOfMem;
    if (moveUp != NULL) {
      x = moveUp->x;
    }
    moveDown = ddSiftingDown(table, x, xHigh);
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto ddSiftingAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = ddSiftingBackward(table, initialSize, moveDown);
    if (!result)
      goto ddSiftingAuxOutOfMem;
  }

  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (1);

ddSiftingAuxOutOfMem:
  if (moveDown != (Move *)CUDD_OUT_OF_MEM) {
    while (moveDown != NULL) {
      move = moveDown->next;
      cuddDeallocMove(table, moveDown);
      moveDown = move;
    }
  }
  if (moveUp != (Move *)CUDD_OUT_OF_MEM) {
    while (moveUp != NULL) {
      move = moveUp->next;
      cuddDeallocMove(table, moveUp);
      moveUp = move;
    }
  }

  return (0);

} /* end of ddSiftingAux */

/**Function********************************************************************

  Synopsis    [Sifts a variable up.]

  Description [Sifts a variable up. Moves y up until either it reaches
  the bound (xLow) or the size of the DD heap increases too much.
  Returns the set of moves in case of success; NULL if memory is full.]

  SideEffects [None]

******************************************************************************/
static Move *ddSiftingUp(DdManager *table, int y, int xLow) {
  Move *moves;
  Move *move;
  int x;
  int size;
  int limitSize;
  int xindex, yindex;
  int isolated;
  int L; /* lower bound on DD size */
#ifdef DD_DEBUG
  int checkL;
  int z;
  int zindex;
#endif

  moves = NULL;
  yindex = table->invperm[y];

  /* Initialize the lower bound.
  ** The part of the DD below y will not change.
  ** The part of the DD above y that does not interact with y will not
  ** change. The rest may vanish in the best case, except for
  ** the nodes at level xLow, which will not vanish, regardless.
  */
  limitSize = L = table->keys - table->isolated;
  for (x = xLow + 1; x < y; x++) {
    xindex = table->invperm[x];
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[xindex]->ref == 1;
      L -= table->subtables[x].keys - isolated;
    }
  }
  isolated = table->vars[yindex]->ref == 1;
  L -= table->subtables[y].keys - isolated;

  x = cuddNextLow(table, y);
  while (x >= xLow && L <= limitSize) {
    xindex = table->invperm[x];
#ifdef DD_DEBUG
    checkL = table->keys - table->isolated;
    for (z = xLow + 1; z < y; z++) {
      zindex = table->invperm[z];
      if (cuddTestInteract(table, zindex, yindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkL -= table->subtables[z].keys - isolated;
      }
    }
    isolated = table->vars[yindex]->ref == 1;
    checkL -= table->subtables[y].keys - isolated;
    assert(L == checkL);
#endif
    size = cuddSwapInPlace(table, x, y);
    if (size == 0)
      goto ddSiftingUpOutOfMem;
    /* Update the lower bound. */
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[xindex]->ref == 1;
      L += table->subtables[y].keys - isolated;
    }
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddSiftingUpOutOfMem;
    move->x = x;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;
    if ((double)size > (double)limitSize * table->maxGrowth)
      break;
    if (size < limitSize)
      limitSize = size;
    y = x;
    x = cuddNextLow(table, y);
  }
  return (moves);

ddSiftingUpOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of ddSiftingUp */

/**Function********************************************************************

  Synopsis    [Sifts a variable down.]

  Description [Sifts a variable down. Moves x down until either it
  reaches the bound (xHigh) or the size of the DD heap increases too
  much. Returns the set of moves in case of success; NULL if memory is
  full.]

  SideEffects [None]

******************************************************************************/
static Move *ddSiftingDown(DdManager *table, int x, int xHigh) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int R; /* upper bound on node decrease */
  int limitSize;
  int xindex, yindex;
  int isolated;
#ifdef DD_DEBUG
  int checkR;
  int z;
  int zindex;
#endif

  moves = NULL;
  /* Initialize R */
  xindex = table->invperm[x];
  limitSize = size = table->keys - table->isolated;
  R = 0;
  for (y = xHigh; y > x; y--) {
    yindex = table->invperm[y];
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[yindex]->ref == 1;
      R += table->subtables[y].keys - isolated;
    }
  }

  y = cuddNextHigh(table, x);
  while (y <= xHigh && size - R < limitSize) {
#ifdef DD_DEBUG
    checkR = 0;
    for (z = xHigh; z > x; z--) {
      zindex = table->invperm[z];
      if (cuddTestInteract(table, xindex, zindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkR += table->subtables[z].keys - isolated;
      }
    }
    assert(R == checkR);
#endif
    /* Update upper bound on node decrease. */
    yindex = table->invperm[y];
    if (cuddTestInteract(table, xindex, yindex)) {
      isolated = table->vars[yindex]->ref == 1;
      R -= table->subtables[y].keys - isolated;
    }
    size = cuddSwapInPlace(table, x, y);
    if (size == 0)
      goto ddSiftingDownOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto ddSiftingDownOutOfMem;
    move->x = x;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;
    if ((double)size > (double)limitSize * table->maxGrowth)
      break;
    if (size < limitSize)
      limitSize = size;
    x = y;
    y = cuddNextHigh(table, x);
  }
  return (moves);

ddSiftingDownOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of ddSiftingDown */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the DD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the DD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSiftingBackward(DdManager *table, int size, Move *moves) {
  Move *move;
  int res;

  for (move = moves; move != NULL; move = move->next) {
    if (move->size < size) {
      size = move->size;
    }
  }

  for (move = moves; move != NULL; move = move->next) {
    if (move->size == size)
      return (1);
    res = cuddSwapInPlace(table, (int)move->x, (int)move->y);
    if (!res)
      return (0);
  }

  return (1);

} /* end of ddSiftingBackward */

/**Function********************************************************************

  Synopsis    [Prepares the DD heap for dynamic reordering.]

  Description [Prepares the DD heap for dynamic reordering. Does
  garbage collection, to guarantee that there are no dead nodes;
  clears the cache, which is invalidated by dynamic reordering; initializes
  the number of isolated projection functions; and initializes the
  interaction matrix.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddReorderPreprocess(DdManager *table) {
  int i;
  int res;

  /* Clear the cache. */
  cuddCacheFlush(table);
  cuddLocalCacheClearAll(table);

  /* Eliminate dead nodes. Do not scan the cache again. */
  cuddGarbageCollect(table, 0);

  /* Initialize number of isolated projection functions. */
  table->isolated = 0;
  for (i = 0; i < table->size; i++) {
    if (table->vars[i]->ref == 1)
      table->isolated++;
  }

  /* Initialize the interaction matrix. */
  res = cuddInitInteract(table);
  if (res == 0)
    return (0);

  return (1);

} /* end of ddReorderPreprocess */

/**Function********************************************************************

  Synopsis    [Cleans up at the end of reordering.]

  Description []

  SideEffects [None]

******************************************************************************/
static int ddReorderPostprocess(DdManager *table) {

#ifdef DD_VERBOSE
  (void)fflush(table->out);
#endif

  /* Free interaction matrix. */
  FREE(table->interact);

  return (1);

} /* end of ddReorderPostprocess */

/**Function********************************************************************

  Synopsis    [Reorders variables according to a given permutation.]

  Description [Reorders variables according to a given permutation.
  The i-th permutation array contains the index of the variable that
  should be brought to the i-th level. ddShuffle assumes that no
  dead nodes are present and that the interaction matrix is properly
  initialized.  The reordering is achieved by a series of upward sifts.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso []

******************************************************************************/
static int ddShuffle2(DdManager *table, int *permutation) {
  int index;
  int level;
  int position;
  int numvars;
  int result;
#ifdef DD_STATS
  unsigned long localTime;
  int initialSize;
  int finalSize;
  int previousSize;
#endif

  ddTotalNumberSwapping = 0;
#ifdef DD_STATS
  localTime = util_cpu_time();
  initialSize = table->keys - table->isolated;
  (void)fprintf(table->out, "#:I_SHUFFLE %8d: initial size\n", initialSize);
  ddTotalNISwaps = 0;
#endif

  numvars = table->size;

  for (level = 0; level < numvars; level++) {
    index = permutation[level];
    position = table->perm[index];
#ifdef DD_STATS
    previousSize = table->keys - table->isolated;
#endif
    result = ddSiftUp2(table, position, level);
    if (!result)
      return (0);
#ifdef DD_STATS
    if (table->keys < (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "-");
    } else if (table->keys > (unsigned)previousSize + table->isolated) {
      (void)fprintf(table->out, "+"); /* should never happen */
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
  finalSize = table->keys - table->isolated;
  (void)fprintf(table->out, "#:F_SHUFFLE %8d: final size\n", finalSize);
  (void)fprintf(table->out, "#:T_SHUFFLE %8g: total time (sec)\n",
                ((double)(util_cpu_time() - localTime) / 1000.0));
  (void)fprintf(table->out, "#:N_SHUFFLE %8d: total swaps\n",
                ddTotalNumberSwapping);
  (void)fprintf(table->out, "#:M_SHUFFLE %8d: NI swaps\n", ddTotalNISwaps);
#endif

  return (1);

} /* end of ddShuffle */

/**Function********************************************************************

  Synopsis    [Moves one variable up.]

  Description [Takes a variable from position x and sifts it up to
  position xLow;  xLow should be less than or equal to x.
  Returns 1 if successful; 0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int ddSiftUp2(DdManager *table, int x, int xLow) {
  int y;
  int size;

  y = cuddNextLow(table, x);
  while (y >= xLow) {
    size = cuddSwapInPlace(table, y, x);
    if (size == 0) {
      return (0);
    }
    x = y;
    y = cuddNextLow(table, x);
  }
  return (1);

} /* end of ddSiftUp */

/**Function********************************************************************

  Synopsis    [Fixes the BDD variable group tree after a shuffle.]

  Description [Fixes the BDD variable group tree after a
  shuffle. Assumes that the order of the variables in a terminal node
  has not been changed.]

  SideEffects [Changes the BDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static void bddFixTree(DdManager *table, MtrNode *treenode) {
  if (treenode == NULL)
    return;
  treenode->low = ((int)treenode->index < table->size)
                      ? table->perm[treenode->index]
                      : treenode->index;
  if (treenode->child != NULL) {
    bddFixTree(table, treenode->child);
  }
  if (treenode->younger != NULL)
    bddFixTree(table, treenode->younger);
  if (treenode->parent != NULL && treenode->low < treenode->parent->low) {
    treenode->parent->low = treenode->low;
    treenode->parent->index = treenode->index;
  }
  return;

} /* end of bddFixTree */

/**Function********************************************************************

  Synopsis    [Updates the BDD variable group tree before a shuffle.]

  Description [Updates the BDD variable group tree before a shuffle.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Changes the BDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static int ddUpdateMtrTree(DdManager *table, MtrNode *treenode, int *perm,
                           int *invperm) {
  unsigned int i, size;
  int index, level, minLevel, maxLevel, minIndex;

  if (treenode == NULL)
    return (1);

  minLevel = CUDD_MAXINDEX;
  maxLevel = 0;
  minIndex = -1;
  /* i : level */
  for (i = treenode->low; i < treenode->low + treenode->size; i++) {
    index = table->invperm[i];
    level = perm[index];
    if (level < minLevel) {
      minLevel = level;
      minIndex = index;
    }
    if (level > maxLevel)
      maxLevel = level;
  }
  size = maxLevel - minLevel + 1;
  if (minIndex == -1)
    return (0);
  if (size == treenode->size) {
    treenode->low = minLevel;
    treenode->index = minIndex;
  } else {
    return (0);
  }

  if (treenode->child != NULL) {
    if (!ddUpdateMtrTree(table, treenode->child, perm, invperm))
      return (0);
  }
  if (treenode->younger != NULL) {
    if (!ddUpdateMtrTree(table, treenode->younger, perm, invperm))
      return (0);
  }
  return (1);
}

/**Function********************************************************************

  Synopsis    [Checks the BDD variable group tree before a shuffle.]

  Description [Checks the BDD variable group tree before a shuffle.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [Changes the BDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static int ddCheckPermuation(DdManager *table, MtrNode *treenode, int *perm,
                             int *invperm) {
  unsigned int i, size;
  int index, level, minLevel, maxLevel;

  if (treenode == NULL)
    return (1);

  minLevel = table->size;
  maxLevel = 0;
  /* i : level */
  for (i = treenode->low; i < treenode->low + treenode->size; i++) {
    index = table->invperm[i];
    level = perm[index];
    if (level < minLevel)
      minLevel = level;
    if (level > maxLevel)
      maxLevel = level;
  }
  size = maxLevel - minLevel + 1;
  if (size != treenode->size)
    return (0);

  if (treenode->child != NULL) {
    if (!ddCheckPermuation(table, treenode->child, perm, invperm))
      return (0);
  }
  if (treenode->younger != NULL) {
    if (!ddCheckPermuation(table, treenode->younger, perm, invperm))
      return (0);
  }
  return (1);
}
/**CFile***********************************************************************

  FileName    [cuddSat.c]

  PackageName [cudd]

  Synopsis    [Functions for the solution of satisfiability related problems.]

  Description [External procedures included in this file:
                <ul>
                <li> Cudd_Eval()
                <li> Cudd_ShortestPath()
                <li> Cudd_LargestCube()
                <li> Cudd_ShortestLength()
                <li> Cudd_Decreasing()
                <li> Cudd_Increasing()
                <li> Cudd_EquivDC()
                <li> Cudd_bddLeqUnless()
                <li> Cudd_EqualSupNorm()
                <li> Cudd_bddMakePrime()
                <li> Cudd_bddMaximallyExpand()
                <li> Cudd_bddLargestPrimeUnate()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddBddMakePrime()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> freePathPair()
                <li> getShortest()
                <li> getPath()
                <li> getLargest()
                <li> getCube()
                <li> ddBddMaximallyExpand()
                <li> ddShortestPathUnate()
                </ul>]

  Author      [Seh-Woong Jeong, Fabio Somenzi]

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

#define DD_BIGGY 100000000

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

typedef struct cuddPathPair {
  int pos;
  int neg;
} cuddPathPair;

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

//#ifndef lint
// static char rcsid[] DD_UNUSED = "$Id: cuddSat.c,v 1.39 2012/02/05 01:07:19
// fabio Exp $"; #endif

static DdNode *one, *zero;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#define WEIGHT(weight, col) ((weight) == NULL ? 1 : weight[col])

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static enum st_retval freePathPair(char *key, char *value, char *arg);
static cuddPathPair getShortest(DdNode *root, int *cost, int *support,
                                st_table *visited);

static cuddPathPair getLargest(DdNode *root, st_table *visited);
static DdNode *getCube(DdManager *manager, st_table *visited, DdNode *f,
                       int cost);
static DdNode *ddBddMaximallyExpand(DdManager *dd, DdNode *lb, DdNode *ub,
                                    DdNode *f);
static int ddBddShortestPathUnate(DdManager *dd, DdNode *f, int *phases,
                                  st_table *table);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Finds a largest cube in a DD.]

  Description [Finds a largest cube in a DD. f is the DD we want to
  get the largest cube for. The problem is translated into the one of
  finding a shortest path in f, when both THEN and ELSE arcs are assumed to
  have unit length. This yields a largest cube in the disjoint cover
  corresponding to the DD. Therefore, it is not necessarily the largest
  implicant of f.  Returns the largest cube as a BDD.]

  SideEffects [The number of literals of the cube is returned in the location
  pointed by length if it is non-null.]

  SeeAlso     [Cudd_ShortestPath]

******************************************************************************/
DdNode *Cudd_LargestCube(DdManager *manager, DdNode *f, int *length) {
  register DdNode *F;
  st_table *visited;
  DdNode *sol;
  cuddPathPair *rootPair;
  int complement, cost;

  one = DD_ONE(manager);
  zero = DD_ZERO(manager);

  if (f == Cudd_Not(one) || f == zero) {
    if (length != NULL) {
      *length = DD_BIGGY;
    }
    return (Cudd_Not(one));
  }
  /* From this point on, a path exists. */

  do {
    manager->reordered = 0;

    /* Initialize visited table. */
    visited = st_init_table(st_ptrcmp, st_ptrhash);

    /* Now get the length of the shortest path(s) from f to 1. */
    (void)getLargest(f, visited);

    complement = Cudd_IsComplement(f);

    F = Cudd_Regular(f);

    if (!st_lookup(visited, F, &rootPair))
      return (NULL);

    if (complement) {
      cost = rootPair->neg;
    } else {
      cost = rootPair->pos;
    }

    /* Recover an actual shortest path. */
    sol = getCube(manager, visited, f, cost);

    st_foreach(visited, freePathPair, NULL);
    st_free_table(visited);

  } while (manager->reordered == 1);

  if (length != NULL) {
    *length = cost;
  }
  return (sol);

} /* end of Cudd_LargestCube */

/**Function********************************************************************

  Synopsis    [Determines whether a BDD is negative unate in a
  variable.]

  Description [Determines whether the function represented by BDD f is
  negative unate (monotonic decreasing) in variable i. Returns the
  constant one is f is unate and the (logical) constant zero if it is not.
  This function does not generate any new nodes.]

  SideEffects [None]

  SeeAlso     [Cudd_Increasing]

******************************************************************************/
DdNode *Cudd_Decreasing(DdManager *dd, DdNode *f, int i) {
  unsigned int topf, level;
  DdNode *F, *fv, *fvn, *res;
  DD_CTFP cacheOp;

  statLine(dd);
#ifdef DD_DEBUG
  assert(0 <= i && i < dd->size);
#endif

  F = Cudd_Regular(f);
  topf = cuddI(dd, F->index);

  /* Check terminal case. If topf > i, f does not depend on var.
  ** Therefore, f is unate in i.
  */
  level = (unsigned)dd->perm[i];
  if (topf > level) {
    return (DD_ONE(dd));
  }

  /* From now on, f is not constant. */

  /* Check cache. */
  cacheOp = (DD_CTFP)Cudd_Decreasing;
  res = cuddCacheLookup2(dd, cacheOp, f, dd->vars[i]);
  if (res != NULL) {
    return (res);
  }

  /* Compute cofactors. */
  fv = cuddT(F);
  fvn = cuddE(F);
  if (F != f) {
    fv = Cudd_Not(fv);
    fvn = Cudd_Not(fvn);
  }

  if (topf == (unsigned)level) {
    /* Special case: if fv is regular, fv(1,...,1) = 1;
    ** If in addition fvn is complemented, fvn(1,...,1) = 0.
    ** But then f(1,1,...,1) > f(0,1,...,1). Hence f is not
    ** monotonic decreasing in i.
    */
    if (!Cudd_IsComplement(fv) && Cudd_IsComplement(fvn)) {
      return (Cudd_Not(DD_ONE(dd)));
    }
    res = Cudd_bddLeq(dd, fv, fvn) ? DD_ONE(dd) : Cudd_Not(DD_ONE(dd));
  } else {
    res = Cudd_Decreasing(dd, fv, i);
    if (res == DD_ONE(dd)) {
      res = Cudd_Decreasing(dd, fvn, i);
    }
  }

  cuddCacheInsert2(dd, cacheOp, f, dd->vars[i], res);
  return (res);

} /* end of Cudd_Decreasing */

/**Function********************************************************************

  Synopsis    [Tells whether F and G are identical wherever D is 0.]

  Description [Tells whether F and G are identical wherever D is 0.  F
  and G are either two ADDs or two BDDs.  D is either a 0-1 ADD or a
  BDD.  The function returns 1 if F and G are equivalent, and 0
  otherwise.  No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_bddLeqUnless]

******************************************************************************/
int Cudd_EquivDC(DdManager *dd, DdNode *F, DdNode *G, DdNode *D) {
  DdNode *tmp, *One, *Gr, *Dr;
  DdNode *Fv, *Fvn, *Gv, *Gvn, *Dv, *Dvn;
  int res;
  unsigned int flevel, glevel, dlevel, top;

  One = DD_ONE(dd);

  statLine(dd);
  /* Check terminal cases. */
  if (D == One || F == G)
    return (1);
  if (D == Cudd_Not(One) || D == DD_ZERO(dd) || F == Cudd_Not(G))
    return (0);

  /* From now on, D is non-constant. */

  /* Normalize call to increase cache efficiency. */
  if (F > G) {
    tmp = F;
    F = G;
    G = tmp;
  }
  if (Cudd_IsComplement(F)) {
    F = Cudd_Not(F);
    G = Cudd_Not(G);
  }

  /* From now on, F is regular. */

  /* Check cache. */
  tmp = cuddCacheLookup(dd, DD_EQUIV_DC_TAG, F, G, D);
  if (tmp != NULL)
    return (tmp == One);

  /* Find splitting variable. */
  flevel = cuddI(dd, F->index);
  Gr = Cudd_Regular(G);
  glevel = cuddI(dd, Gr->index);
  top = ddMin(flevel, glevel);
  Dr = Cudd_Regular(D);
  dlevel = dd->perm[Dr->index];
  top = ddMin(top, dlevel);

  /* Compute cofactors. */
  if (top == flevel) {
    Fv = cuddT(F);
    Fvn = cuddE(F);
  } else {
    Fv = Fvn = F;
  }
  if (top == glevel) {
    Gv = cuddT(Gr);
    Gvn = cuddE(Gr);
    if (G != Gr) {
      Gv = Cudd_Not(Gv);
      Gvn = Cudd_Not(Gvn);
    }
  } else {
    Gv = Gvn = G;
  }
  if (top == dlevel) {
    Dv = cuddT(Dr);
    Dvn = cuddE(Dr);
    if (D != Dr) {
      Dv = Cudd_Not(Dv);
      Dvn = Cudd_Not(Dvn);
    }
  } else {
    Dv = Dvn = D;
  }

  /* Solve recursively. */
  res = Cudd_EquivDC(dd, Fv, Gv, Dv);
  if (res != 0) {
    res = Cudd_EquivDC(dd, Fvn, Gvn, Dvn);
  }
  cuddCacheInsert(dd, DD_EQUIV_DC_TAG, F, G, D, (res) ? One : Cudd_Not(One));

  return (res);

} /* end of Cudd_EquivDC */

/**Function********************************************************************

  Synopsis    [Tells whether f is less than of equal to G unless D is 1.]

  Description [Tells whether f is less than of equal to G unless D is
  1.  f, g, and D are BDDs.  The function returns 1 if f is less than
  of equal to G, and 0 otherwise.  No new nodes are created.]

  SideEffects [None]

  SeeAlso     [Cudd_EquivDC Cudd_bddLeq Cudd_bddIteConstant]

******************************************************************************/
int Cudd_bddLeqUnless(DdManager *dd, DdNode *f, DdNode *g, DdNode *D) {
  DdNode *tmp, *One, *F, *G;
  DdNode *Ft, *Fe, *Gt, *Ge, *Dt, *De;
  int res;
  unsigned int flevel, glevel, dlevel, top;

  statLine(dd);

  One = DD_ONE(dd);

  /* Check terminal cases. */
  if (f == g || g == One || f == Cudd_Not(One) || D == One || D == f ||
      D == Cudd_Not(g))
    return (1);
  /* Check for two-operand cases. */
  if (D == Cudd_Not(One) || D == g || D == Cudd_Not(f))
    return (Cudd_bddLeq(dd, f, g));
  if (g == Cudd_Not(One) || g == Cudd_Not(f))
    return (Cudd_bddLeq(dd, f, D));
  if (f == One)
    return (Cudd_bddLeq(dd, Cudd_Not(g), D));

  /* From now on, f, g, and D are non-constant, distinct, and
  ** non-complementary. */

  /* Normalize call to increase cache efficiency.  We rely on the
  ** fact that f <= g unless D is equivalent to not(g) <= not(f)
  ** unless D and to f <= D unless g.  We make sure that D is
  ** regular, and that at most one of f and g is complemented.  We also
  ** ensure that when two operands can be swapped, the one with the
  ** lowest address comes first. */

  if (Cudd_IsComplement(D)) {
    if (Cudd_IsComplement(g)) {
      /* Special case: if f is regular and g is complemented,
      ** f(1,...,1) = 1 > 0 = g(1,...,1).  If D(1,...,1) = 0, return 0.
      */
      if (!Cudd_IsComplement(f))
        return (0);
      /* !g <= D unless !f  or  !D <= g unless !f */
      tmp = D;
      D = Cudd_Not(f);
      if (g < tmp) {
        f = Cudd_Not(g);
        g = tmp;
      } else {
        f = Cudd_Not(tmp);
      }
    } else {
      if (Cudd_IsComplement(f)) {
        /* !D <= !f unless g  or  !D <= g unless !f */
        tmp = f;
        f = Cudd_Not(D);
        if (tmp < g) {
          D = g;
          g = Cudd_Not(tmp);
        } else {
          D = Cudd_Not(tmp);
        }
      } else {
        /* f <= D unless g  or  !D <= !f unless g */
        tmp = D;
        D = g;
        if (tmp < f) {
          g = Cudd_Not(f);
          f = Cudd_Not(tmp);
        } else {
          g = tmp;
        }
      }
    }
  } else {
    if (Cudd_IsComplement(g)) {
      if (Cudd_IsComplement(f)) {
        /* !g <= !f unless D  or  !g <= D unless !f */
        tmp = f;
        f = Cudd_Not(g);
        if (D < tmp) {
          g = D;
          D = Cudd_Not(tmp);
        } else {
          g = Cudd_Not(tmp);
        }
      } else {
        /* f <= g unless D  or  !g <= !f unless D */
        if (g < f) {
          tmp = g;
          g = Cudd_Not(f);
          f = Cudd_Not(tmp);
        }
      }
    } else {
      /* f <= g unless D  or  f <= D unless g */
      if (D < g) {
        tmp = D;
        D = g;
        g = tmp;
      }
    }
  }

  /* From now on, D is regular. */

  /* Check cache. */
  tmp = cuddCacheLookup(dd, DD_BDD_LEQ_UNLESS_TAG, f, g, D);
  if (tmp != NULL)
    return (tmp == One);

  /* Find splitting variable. */
  F = Cudd_Regular(f);
  flevel = dd->perm[F->index];
  G = Cudd_Regular(g);
  glevel = dd->perm[G->index];
  top = ddMin(flevel, glevel);
  dlevel = dd->perm[D->index];
  top = ddMin(top, dlevel);

  /* Compute cofactors. */
  if (top == flevel) {
    Ft = cuddT(F);
    Fe = cuddE(F);
    if (F != f) {
      Ft = Cudd_Not(Ft);
      Fe = Cudd_Not(Fe);
    }
  } else {
    Ft = Fe = f;
  }
  if (top == glevel) {
    Gt = cuddT(G);
    Ge = cuddE(G);
    if (G != g) {
      Gt = Cudd_Not(Gt);
      Ge = Cudd_Not(Ge);
    }
  } else {
    Gt = Ge = g;
  }
  if (top == dlevel) {
    Dt = cuddT(D);
    De = cuddE(D);
  } else {
    Dt = De = D;
  }

  /* Solve recursively. */
  res = Cudd_bddLeqUnless(dd, Ft, Gt, Dt);
  if (res != 0) {
    res = Cudd_bddLeqUnless(dd, Fe, Ge, De);
  }
  cuddCacheInsert(dd, DD_BDD_LEQ_UNLESS_TAG, f, g, D, Cudd_NotCond(One, !res));

  return (res);

} /* end of Cudd_bddLeqUnless */

/**Function********************************************************************

  Synopsis    [Compares two ADDs for equality within tolerance.]

  Description [Compares two ADDs for equality within tolerance. Two
  ADDs are reported to be equal if the maximum difference between them
  (the sup norm of their difference) is less than or equal to the
  tolerance parameter. Returns 1 if the two ADDs are equal (within
  tolerance); 0 otherwise. If parameter <code>pr</code> is positive
  the first failure is reported to the standard output.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int Cudd_EqualSupNorm(
    DdManager *dd /* manager */, DdNode *f /* first ADD */,
    DdNode *g /* second ADD */,
    CUDD_VALUE_TYPE tolerance /* maximum allowed difference */,
    int pr /* verbosity level */) {
  DdNode *fv, *fvn, *gv, *gvn, *r;
  unsigned int topf, topg;

  statLine(dd);
  /* Check terminal cases. */
  if (f == g)
    return (1);
  if (Cudd_IsConstant(f) && Cudd_IsConstant(g)) {
    if (ddEqualVal(cuddV(f), cuddV(g), tolerance)) {
      return (1);
    } else {
      if (pr > 0) {
        (void)fprintf(dd->out, "Offending nodes:\n");
        (void)fprintf(dd->out, "f: address = %p\t value = %40.30f\n", (void *)f,
                      cuddV(f));
        (void)fprintf(dd->out, "g: address = %p\t value = %40.30f\n", (void *)g,
                      cuddV(g));
      }
      return (0);
    }
  }

  /* We only insert the result in the cache if the comparison is
  ** successful. Therefore, if we hit we return 1. */
  r = cuddCacheLookup2(dd, (DD_CTFP)Cudd_EqualSupNorm, f, g);
  if (r != NULL) {
    return (1);
  }

  /* Compute the cofactors and solve the recursive subproblems. */
  topf = cuddI(dd, f->index);
  topg = cuddI(dd, g->index);

  if (topf <= topg) {
    fv = cuddT(f);
    fvn = cuddE(f);
  } else {
    fv = fvn = f;
  }
  if (topg <= topf) {
    gv = cuddT(g);
    gvn = cuddE(g);
  } else {
    gv = gvn = g;
  }

  if (!Cudd_EqualSupNorm(dd, fv, gv, tolerance, pr))
    return (0);
  if (!Cudd_EqualSupNorm(dd, fvn, gvn, tolerance, pr))
    return (0);

  cuddCacheInsert2(dd, (DD_CTFP)Cudd_EqualSupNorm, f, g, DD_ONE(dd));

  return (1);

} /* end of Cudd_EqualSupNorm */

/**Function********************************************************************

  Synopsis    [Expands cube to a prime implicant of f.]

  Description [Expands cube to a prime implicant of f. Returns the prime
  if successful; NULL otherwise.  In particular, NULL is returned if cube
  is not a real cube or is not an implicant of f.]

  SideEffects [None]

  SeeAlso     [Cudd_bddMaximallyExpand]

******************************************************************************/
DdNode *Cudd_bddMakePrime(
    DdManager *dd /* manager */, DdNode *cube /* cube to be expanded */,
    DdNode *f /* function of which the cube is to be made a prime */) {
  DdNode *res;

  if (!Cudd_bddLeq(dd, cube, f))
    return (NULL);

  do {
    dd->reordered = 0;
    res = cuddBddMakePrime(dd, cube, f);
  } while (dd->reordered == 1);
  return (res);

} /* end of Cudd_bddMakePrime */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddMakePrime.]

  Description [Performs the recursive step of Cudd_bddMakePrime.
  Returns the prime if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddBddMakePrime(
    DdManager *dd /* manager */, DdNode *cube /* cube to be expanded */,
    DdNode *f /* function of which the cube is to be made a prime */) {
  DdNode *scan;
  DdNode *t, *e;
  DdNode *res = cube;
  DdNode *zero = Cudd_Not(DD_ONE(dd));

  Cudd_Ref(res);
  scan = cube;
  while (!Cudd_IsConstant(scan)) {
    DdNode *reg = Cudd_Regular(scan);
    DdNode *var = dd->vars[reg->index];
    DdNode *expanded = Cudd_bddExistAbstract(dd, res, var);
    if (expanded == NULL) {
      Cudd_RecursiveDeref(dd, res);
      return (NULL);
    }
    Cudd_Ref(expanded);
    if (Cudd_bddLeq(dd, expanded, f)) {
      Cudd_RecursiveDeref(dd, res);
      res = expanded;
    } else {
      Cudd_RecursiveDeref(dd, expanded);
    }
    cuddGetBranches(scan, &t, &e);
    if (t == zero) {
      scan = e;
    } else if (e == zero) {
      scan = t;
    } else {
      Cudd_RecursiveDeref(dd, res);
      return (NULL); /* cube is not a cube */
    }
  }

  if (scan == DD_ONE(dd)) {
    Cudd_Deref(res);
    return (res);
  } else {
    Cudd_RecursiveDeref(dd, res);
    return (NULL);
  }

} /* end of cuddBddMakePrime */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Frees the entries of the visited symbol table.]

  Description [Frees the entries of the visited symbol table. Returns
  ST_CONTINUE.]

  SideEffects [None]

******************************************************************************/
static enum st_retval freePathPair(char *key, char *value, char *arg) {
  cuddPathPair *pair;

  pair = (cuddPathPair *)value;
  FREE(pair);
  return (ST_CONTINUE);

} /* end of freePathPair */

/**Function********************************************************************

  Synopsis    [Finds the length of the shortest path(s) in a DD.]

  Description [Finds the length of the shortest path(s) in a DD.
  Uses a local symbol table to store the lengths for each
  node. Only the lengths for the regular nodes are entered in the table,
  because those for the complement nodes are simply obtained by swapping
  the two lenghts.
  Returns a pair of lengths: the length of the shortest path to 1;
  and the length of the shortest path to 0. This is done so as to take
  complement arcs into account.]

  SideEffects [Accumulates the support of the DD in support.]

  SeeAlso     []

******************************************************************************/
static cuddPathPair getShortest(DdNode *root, int *cost, int *support,
                                st_table *visited) {
  cuddPathPair *my_pair, res_pair, pair_T, pair_E;
  DdNode *my_root, *T, *E;
  int weight;

  my_root = Cudd_Regular(root);

  if (st_lookup(visited, my_root, &my_pair)) {
    if (Cudd_IsComplement(root)) {
      res_pair.pos = my_pair->neg;
      res_pair.neg = my_pair->pos;
    } else {
      res_pair.pos = my_pair->pos;
      res_pair.neg = my_pair->neg;
    }
    return (res_pair);
  }

  /* In the case of a BDD the following test is equivalent to
  ** testing whether the BDD is the constant 1. This formulation,
  ** however, works for ADDs as well, by assuming the usual
  ** dichotomy of 0 and != 0.
  */
  if (cuddIsConstant(my_root)) {
    if (my_root != zero) {
      res_pair.pos = 0;
      res_pair.neg = DD_BIGGY;
    } else {
      res_pair.pos = DD_BIGGY;
      res_pair.neg = 0;
    }
  } else {
    T = cuddT(my_root);
    E = cuddE(my_root);

    pair_T = getShortest(T, cost, support, visited);
    pair_E = getShortest(E, cost, support, visited);
    weight = WEIGHT(cost, my_root->index);
    res_pair.pos = ddMin(pair_T.pos + weight, pair_E.pos);
    res_pair.neg = ddMin(pair_T.neg + weight, pair_E.neg);

    /* Update support. */
    if (support != NULL) {
      support[my_root->index] = 1;
    }
  }

  my_pair = ALLOC(cuddPathPair, 1);
  if (my_pair == NULL) {
    if (Cudd_IsComplement(root)) {
      int tmp = res_pair.pos;
      res_pair.pos = res_pair.neg;
      res_pair.neg = tmp;
    }
    return (res_pair);
  }
  my_pair->pos = res_pair.pos;
  my_pair->neg = res_pair.neg;

  st_insert(visited, (char *)my_root, (char *)my_pair);
  if (Cudd_IsComplement(root)) {
    res_pair.pos = my_pair->neg;
    res_pair.neg = my_pair->pos;
  } else {
    res_pair.pos = my_pair->pos;
    res_pair.neg = my_pair->neg;
  }
  return (res_pair);

} /* end of getShortest */

/* end of getPath */

/**Function********************************************************************

  Synopsis    [Finds the size of the largest cube(s) in a DD.]

  Description [Finds the size of the largest cube(s) in a DD.
  This problem is translated into finding the shortest paths from a node
  when both THEN and ELSE arcs have unit lengths.
  Uses a local symbol table to store the lengths for each
  node. Only the lengths for the regular nodes are entered in the table,
  because those for the complement nodes are simply obtained by swapping
  the two lenghts.
  Returns a pair of lengths: the length of the shortest path to 1;
  and the length of the shortest path to 0. This is done so as to take
  complement arcs into account.]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/
static cuddPathPair getLargest(DdNode *root, st_table *visited) {
  cuddPathPair *my_pair, res_pair, pair_T, pair_E;
  DdNode *my_root, *T, *E;

  my_root = Cudd_Regular(root);

  if (st_lookup(visited, my_root, &my_pair)) {
    if (Cudd_IsComplement(root)) {
      res_pair.pos = my_pair->neg;
      res_pair.neg = my_pair->pos;
    } else {
      res_pair.pos = my_pair->pos;
      res_pair.neg = my_pair->neg;
    }
    return (res_pair);
  }

  /* In the case of a BDD the following test is equivalent to
  ** testing whether the BDD is the constant 1. This formulation,
  ** however, works for ADDs as well, by assuming the usual
  ** dichotomy of 0 and != 0.
  */
  if (cuddIsConstant(my_root)) {
    if (my_root != zero) {
      res_pair.pos = 0;
      res_pair.neg = DD_BIGGY;
    } else {
      res_pair.pos = DD_BIGGY;
      res_pair.neg = 0;
    }
  } else {
    T = cuddT(my_root);
    E = cuddE(my_root);

    pair_T = getLargest(T, visited);
    pair_E = getLargest(E, visited);
    res_pair.pos = ddMin(pair_T.pos, pair_E.pos) + 1;
    res_pair.neg = ddMin(pair_T.neg, pair_E.neg) + 1;
  }

  my_pair = ALLOC(cuddPathPair, 1);
  if (my_pair == NULL) { /* simply do not cache this result */
    if (Cudd_IsComplement(root)) {
      int tmp = res_pair.pos;
      res_pair.pos = res_pair.neg;
      res_pair.neg = tmp;
    }
    return (res_pair);
  }
  my_pair->pos = res_pair.pos;
  my_pair->neg = res_pair.neg;

  /* Caching may fail without affecting correctness. */
  st_insert(visited, (char *)my_root, (char *)my_pair);
  if (Cudd_IsComplement(root)) {
    res_pair.pos = my_pair->neg;
    res_pair.neg = my_pair->pos;
  } else {
    res_pair.pos = my_pair->pos;
    res_pair.neg = my_pair->neg;
  }
  return (res_pair);

} /* end of getLargest */

/**Function********************************************************************

  Synopsis    [Build a BDD for a largest cube of f.]

  Description [Build a BDD for a largest cube of f.
  Given the minimum length from the root, and the minimum
  lengths for each node (in visited), apply triangulation at each node.
  Of the two children of each node on a shortest path, at least one is
  on a shortest path. In case of ties the procedure chooses the THEN
  children.
  Returns a pointer to the cube BDD representing the path if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *getCube(DdManager *manager, st_table *visited, DdNode *f,
                       int cost) {
  DdNode *sol, *tmp;
  DdNode *my_dd, *T, *E;
  cuddPathPair *T_pair, *E_pair;
  int Tcost, Ecost;
  int complement;

  my_dd = Cudd_Regular(f);
  complement = Cudd_IsComplement(f);

  sol = one;
  cuddRef(sol);

  while (!cuddIsConstant(my_dd)) {
    Tcost = cost - 1;
    Ecost = cost - 1;

    T = cuddT(my_dd);
    E = cuddE(my_dd);

    if (complement) {
      T = Cudd_Not(T);
      E = Cudd_Not(E);
    }

    if (!st_lookup(visited, Cudd_Regular(T), &T_pair))
      return (NULL);
    if ((Cudd_IsComplement(T) && T_pair->neg == Tcost) ||
        (!Cudd_IsComplement(T) && T_pair->pos == Tcost)) {
      tmp = cuddBddAndRecur(manager, manager->vars[my_dd->index], sol);
      if (tmp == NULL) {
        Cudd_RecursiveDeref(manager, sol);
        return (NULL);
      }
      cuddRef(tmp);
      Cudd_RecursiveDeref(manager, sol);
      sol = tmp;

      complement = Cudd_IsComplement(T);
      my_dd = Cudd_Regular(T);
      cost = Tcost;
      continue;
    }
    if (!st_lookup(visited, Cudd_Regular(E), &E_pair))
      return (NULL);
    if ((Cudd_IsComplement(E) && E_pair->neg == Ecost) ||
        (!Cudd_IsComplement(E) && E_pair->pos == Ecost)) {
      tmp =
          cuddBddAndRecur(manager, Cudd_Not(manager->vars[my_dd->index]), sol);
      if (tmp == NULL) {
        Cudd_RecursiveDeref(manager, sol);
        return (NULL);
      }
      cuddRef(tmp);
      Cudd_RecursiveDeref(manager, sol);
      sol = tmp;
      complement = Cudd_IsComplement(E);
      my_dd = Cudd_Regular(E);
      cost = Ecost;
      continue;
    }
    (void)fprintf(manager->err, "We shouldn't be here!\n");
    manager->errorCode = CUDD_INTERNAL_ERROR;
    return (NULL);
  }

  cuddDeref(sol);
  return (sol);

} /* end of getCube */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddMaximallyExpand.]

  Description [Performs the recursive step of Cudd_bddMaximallyExpand.
  Returns set of primes or zero BDD if successful; NULL otherwise.  On entry
  to this function, ub and lb should be different from the zero BDD.  The
  function then maintains this invariant.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *
ddBddMaximallyExpand(DdManager *dd /* manager */,
                     DdNode *lb /* cube to be expanded */,
                     DdNode *ub /* upper bound cube */,
                     DdNode *f /* function against which to expand */) {
  DdNode *one, *zero, *lbv, *lbvn, *lbnx, *ubv, *ubvn, *fv, *fvn, *res;
  DdNode *F, *UB, *LB, *t, *e;
  unsigned int top, toplb, topub, topf, index;

  statLine(dd);
  /* Terminal cases. */
  one = DD_ONE(dd);
  zero = Cudd_Not(one);
  assert(ub != zero && lb != zero);
  /** There are three major terminal cases in theory:
   **   ub -> f     : return ub
   **   lb == f     : return lb
   **   not(lb -> f): return zero
   ** Only the second case can be checked exactly in constant time.
   ** For the others, we check for sufficient conditions.
   */
  if (ub == f || f == one)
    return (ub);
  if (lb == f)
    return (lb);
  if (f == zero || ub == Cudd_Not(f) || lb == one || lb == Cudd_Not(f))
    return (zero);
  if (!Cudd_IsComplement(lb) && Cudd_IsComplement(f))
    return (zero);

  /* Here lb and f are not constant. */

  /* Check cache.  Since lb and ub are cubes, their local reference counts
  ** are always 1.  Hence, we only check the reference count of f.
  */
  F = Cudd_Regular(f);
  if (F->ref != 1) {
    DdNode *tmp = cuddCacheLookup(dd, DD_BDD_MAX_EXP_TAG, lb, ub, f);
    if (tmp != NULL) {
      return (tmp);
    }
  }

  /* Compute cofactors.  For lb we use the non-zero one in
  ** both branches of the recursion.
  */
  LB = Cudd_Regular(lb);
  UB = Cudd_Regular(ub);
  topf = dd->perm[F->index];
  toplb = dd->perm[LB->index];
  topub = (ub == one) ? CUDD_CONST_INDEX : dd->perm[UB->index];
  assert(toplb <= topub);
  top = ddMin(topf, toplb);
  if (toplb == top) {
    index = LB->index;
    lbv = cuddT(LB);
    lbvn = cuddE(LB);
    if (lb != LB) {
      lbv = Cudd_Not(lbv);
      lbvn = Cudd_Not(lbvn);
    }
    if (lbv == zero) {
      lbnx = lbvn;
    } else {
      lbnx = lbv;
    }
  } else {
    index = F->index;
    lbnx = lbv = lbvn = lb;
  }
  if (topub == top) {
    ubv = cuddT(UB);
    ubvn = cuddE(UB);
    if (ub != UB) {
      ubv = Cudd_Not(ubv);
      ubvn = Cudd_Not(ubvn);
    }
  } else {
    ubv = ubvn = ub;
  }
  if (topf == top) {
    fv = cuddT(F);
    fvn = cuddE(F);
    if (f != F) {
      fv = Cudd_Not(fv);
      fvn = Cudd_Not(fvn);
    }
  } else {
    fv = fvn = f;
  }

  /* Recursive calls. */
  if (ubv != zero) {
    t = ddBddMaximallyExpand(dd, lbnx, ubv, fv);
    if (t == NULL)
      return (NULL);
  } else {
    assert(topub == toplb && topub == top && lbv == zero);
    t = zero;
  }
  cuddRef(t);

  /* If the top variable appears only in lb, the positive and negative
  ** cofactors of each operand are the same.  We want to avoid a
  ** needless recursive call, which would force us to give up the
  ** cache optimization trick based on reference counts.
  */
  if (ubv == ubvn && fv == fvn) {
    res = t;
  } else {
    if (ubvn != zero) {
      e = ddBddMaximallyExpand(dd, lbnx, ubvn, fvn);
      if (e == NULL) {
        Cudd_IterDerefBdd(dd, t);
        return (NULL);
      }
    } else {
      assert(topub == toplb && topub == top && lbvn == zero);
      e = zero;
    }

    if (t == e) {
      res = t;
    } else {
      cuddRef(e);

      if (toplb == top) {
        if (lbv == zero) {
          /* Top variable appears in negative phase. */
          if (t != one) {
            DdNode *newT;
            if (Cudd_IsComplement(t)) {
              newT = cuddUniqueInter(dd, index, Cudd_Not(t), zero);
              if (newT == NULL) {
                Cudd_IterDerefBdd(dd, t);
                Cudd_IterDerefBdd(dd, e);
                return (NULL);
              }
              newT = Cudd_Not(newT);
            } else {
              newT = cuddUniqueInter(dd, index, t, one);
              if (newT == NULL) {
                Cudd_IterDerefBdd(dd, t);
                Cudd_IterDerefBdd(dd, e);
                return (NULL);
              }
            }
            cuddRef(newT);
            cuddDeref(t);
            t = newT;
          }
        } else if (lbvn == zero) {
          /* Top variable appears in positive phase. */
          if (e != one) {
            DdNode *newE;
            newE = cuddUniqueInter(dd, index, one, e);
            if (newE == NULL) {
              Cudd_IterDerefBdd(dd, t);
              Cudd_IterDerefBdd(dd, e);
              return (NULL);
            }
            cuddRef(newE);
            cuddDeref(e);
            e = newE;
          }
        } else {
          /* Not a cube. */
          Cudd_IterDerefBdd(dd, t);
          Cudd_IterDerefBdd(dd, e);
          return (NULL);
        }
      }

      /* Combine results. */
      res = cuddBddAndRecur(dd, t, e);
      if (res == NULL) {
        Cudd_IterDerefBdd(dd, t);
        Cudd_IterDerefBdd(dd, e);
        return (NULL);
      }
      cuddRef(res);
      Cudd_IterDerefBdd(dd, t);
      Cudd_IterDerefBdd(dd, e);
    }
  }

  /* Cache result and return. */
  if (F->ref != 1) {
    cuddCacheInsert(dd, DD_BDD_MAX_EXP_TAG, lb, ub, f, res);
  }
  cuddDeref(res);
  return (res);

} /* end of ddBddMaximallyExpand */

/**Function********************************************************************

  Synopsis    [Performs shortest path computation on a unate function.]

  Description [Performs shortest path computation on a unate function.
  Returns the length of the shortest path to one if successful;
  CUDD_OUT_OF_MEM otherwise.  This function is based on the observation
  that in the BDD of a unate function no node except the constant is
  reachable from the root via paths of different parity.]

  SideEffects [None]

  SeeAlso     [getShortest]

******************************************************************************/
static int ddBddShortestPathUnate(DdManager *dd, DdNode *f, int *phases,
                                  st_table *table) {
  int positive, l, lT, lE;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = Cudd_Not(one);
  DdNode *F, *fv, *fvn;

  if (st_lookup_int(table, f, &l)) {
    return (l);
  }
  if (f == one) {
    l = 0;
  } else if (f == zero) {
    l = DD_BIGGY;
  } else {
    F = Cudd_Regular(f);
    fv = cuddT(F);
    fvn = cuddE(F);
    if (f != F) {
      fv = Cudd_Not(fv);
      fvn = Cudd_Not(fvn);
    }
    lT = ddBddShortestPathUnate(dd, fv, phases, table);
    lE = ddBddShortestPathUnate(dd, fvn, phases, table);
    positive = phases[F->index];
    l = positive ? ddMin(lT + 1, lE) : ddMin(lT, lE + 1);
  }
  if (st_insert(table, f, (void *)(ptrint)l) == ST_OUT_OF_MEM) {
    return (CUDD_OUT_OF_MEM);
  }
  return (l);

} /* end of ddShortestPathUnate */

/* end of ddGetLargestCubeUnate */
/**CFile***********************************************************************

  FileName    [cuddSymmetry.c]

  PackageName [cudd]

  Synopsis    [Functions for symmetry-based variable reordering.]

  Description [External procedures included in this file:
                <ul>
                <li> Cudd_SymmProfile()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddSymmCheck()
                <li> cuddSymmSifting()
                <li> cuddSymmSiftingConv()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> ddSymmUniqueCompare()
                <li> ddSymmSiftingAux()
                <li> ddSymmSiftingConvAux()
                <li> ddSymmSiftingUp()
                <li> ddSymmSiftingDown()
                <li> ddSymmGroupMove()
                <li> ddSymmGroupMoveBackward()
                <li> ddSymmSiftingBackward()
                <li> ddSymmSummary()
                </ul>]

  Author      [Shipra Panda, Fabio Somenzi]

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

#define MV_OOM (Move *)1

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
// static char rcsid[] DD_UNUSED = "$Id: cuddSymmetry.c,v 1.28 2012/02/05
// 01:07:19 fabio Exp $"; #endif

static int *entry;

extern int ddTotalNumberSwapping;
#ifdef DD_STATS
extern int ddTotalNISwaps;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddSymmUniqueCompare(int *ptrX, int *ptrY);
static int ddSymmSiftingAux(DdManager *table, int x, int xLow, int xHigh);
static int ddSymmSiftingConvAux(DdManager *table, int x, int xLow, int xHigh);
static Move *ddSymmSiftingUp(DdManager *table, int y, int xLow);
static Move *ddSymmSiftingDown(DdManager *table, int x, int xHigh);
static int ddSymmGroupMove(DdManager *table, int x, int y, Move **moves);
static int ddSymmGroupMoveBackward(DdManager *table, int x, int y);
static int ddSymmSiftingBackward(DdManager *table, Move *moves, int size);
static void ddSymmSummary(DdManager *table, int lower, int upper, int *symvars,
                          int *symgroups);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Checks for symmetry of x and y.]

  Description [Checks for symmetry of x and y. Ignores projection
  functions, unless they are isolated. Returns 1 in case of symmetry; 0
  otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddSymmCheck(DdManager *table, int x, int y) {
  DdNode *f, *f0, *f1, *f01, *f00, *f11, *f10;
  int comple;        /* f0 is complemented */
  int xsymmy;        /* x and y may be positively symmetric */
  int xsymmyp;       /* x and y may be negatively symmetric */
  int arccount;      /* number of arcs from layer x to layer y */
  int TotalRefCount; /* total reference count of layer y minus 1 */
  int yindex;
  int i;
  DdNodePtr *list;
  int slots;
  DdNode *sentinel = &(table->sentinel);
#ifdef DD_DEBUG
  int xindex;
#endif

  /* Checks that x and y are not the projection functions.
  ** For x it is sufficient to check whether there is only one
  ** node; indeed, if there is one node, it is the projection function
  ** and it cannot point to y. Hence, if y isn't just the projection
  ** function, it has one arc coming from a layer different from x.
  */
  if (table->subtables[x].keys == 1) {
    return (0);
  }
  yindex = table->invperm[y];
  if (table->subtables[y].keys == 1) {
    if (table->vars[yindex]->ref == 1)
      return (0);
  }

  xsymmy = xsymmyp = 1;
  arccount = 0;
  slots = table->subtables[x].slots;
  list = table->subtables[x].nodelist;
  for (i = 0; i < slots; i++) {
    f = list[i];
    while (f != sentinel) {
      /* Find f1, f0, f11, f10, f01, f00. */
      f1 = cuddT(f);
      f0 = Cudd_Regular(cuddE(f));
      comple = Cudd_IsComplement(cuddE(f));
      if ((int)f1->index == yindex) {
        arccount++;
        f11 = cuddT(f1);
        f10 = cuddE(f1);
      } else {
        if ((int)f0->index != yindex) {
          /* If f is an isolated projection function it is
          ** allowed to bypass layer y.
          */
          if (f1 != DD_ONE(table) || f0 != DD_ONE(table) || f->ref != 1)
            return (0); /* f bypasses layer y */
        }
        f11 = f10 = f1;
      }
      if ((int)f0->index == yindex) {
        arccount++;
        f01 = cuddT(f0);
        f00 = cuddE(f0);
      } else {
        f01 = f00 = f0;
      }
      if (comple) {
        f01 = Cudd_Not(f01);
        f00 = Cudd_Not(f00);
      }

      if (f1 != DD_ONE(table) || f0 != DD_ONE(table) || f->ref != 1) {
        xsymmy &= f01 == f10;
        xsymmyp &= f11 == f00;
        if ((xsymmy == 0) && (xsymmyp == 0))
          return (0);
      }

      f = f->next;
    } /* while */
  }   /* for */

  /* Calculate the total reference counts of y */
  TotalRefCount = -1; /* -1 for projection function */
  slots = table->subtables[y].slots;
  list = table->subtables[y].nodelist;
  for (i = 0; i < slots; i++) {
    f = list[i];
    while (f != sentinel) {
      TotalRefCount += f->ref;
      f = f->next;
    }
  }

#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  if (arccount == TotalRefCount) {
    xindex = table->invperm[x];
    (void)fprintf(table->out, "Found symmetry! x =%d\ty = %d\tPos(%d,%d)\n",
                  xindex, yindex, x, y);
  }
#endif

  return (arccount == TotalRefCount);

} /* end of cuddSymmCheck */

/**Function********************************************************************

  Synopsis    [Symmetric sifting algorithm.]

  Description [Symmetric sifting algorithm.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries in
    each unique subtable.
    <li> Sift the variable up and down, remembering each time the total
    size of the DD heap and grouping variables that are symmetric.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 plus the number of symmetric variables if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddSymmSiftingConv]

******************************************************************************/
int cuddSymmSifting(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int size;
  int x;
  int result;
  int symvars;
  int symgroups;
#ifdef DD_STATS
  int previousSize;
#endif

  size = table->size;

  /* Find order in which to sift variables. */
  var = NULL;
  entry = ALLOC(int, size);
  if (entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddSymmSiftingOutOfMem;
  }
  var = ALLOC(int, size);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddSymmSiftingOutOfMem;
  }

  for (i = 0; i < size; i++) {
    x = table->perm[i];
    entry[i] = table->subtables[x].keys;
    var[i] = i;
  }

  qsort((void *)var, size, sizeof(int), (DD_QSFP)ddSymmUniqueCompare);

  /* Initialize the symmetry of each subtable to itself. */
  for (i = lower; i <= upper; i++) {
    table->subtables[i].next = i;
  }

  for (i = 0; i < ddMin(table->siftMaxVar, size); i++) {
    if (ddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (util_cpu_time() - table->startTime > table->timeLimit) {
      table->autoDyn = 0; /* prevent further reordering */
      break;
    }
    x = table->perm[var[i]];
#ifdef DD_STATS
    previousSize = table->keys - table->isolated;
#endif
    if (x < lower || x > upper)
      continue;
    if (table->subtables[x].next == (unsigned)x) {
      result = ddSymmSiftingAux(table, x, lower, upper);
      if (!result)
        goto ddSymmSiftingOutOfMem;
#ifdef DD_STATS
      if (table->keys < (unsigned)previousSize + table->isolated) {
        (void)fprintf(table->out, "-");
      } else if (table->keys > (unsigned)previousSize + table->isolated) {
        (void)fprintf(table->out, "+"); /* should never happen */
      } else {
        (void)fprintf(table->out, "=");
      }
      fflush(table->out);
#endif
    }
  }

  FREE(var);
  FREE(entry);

  ddSymmSummary(table, lower, upper, &symvars, &symgroups);

#ifdef DD_STATS
  (void)fprintf(table->out, "\n#:S_SIFTING %8d: symmetric variables\n",
                symvars);
  (void)fprintf(table->out, "#:G_SIFTING %8d: symmetric groups", symgroups);
#endif

  return (1 + symvars);

ddSymmSiftingOutOfMem:

  if (entry != NULL)
    FREE(entry);
  if (var != NULL)
    FREE(var);

  return (0);

} /* end of cuddSymmSifting */

/**Function********************************************************************

  Synopsis    [Symmetric sifting to convergence algorithm.]

  Description [Symmetric sifting to convergence algorithm.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries in
    each unique subtable.
    <li> Sift the variable up and down, remembering each time the total
    size of the DD heap and grouping variables that are symmetric.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    <li> Repeat 1-4 until no further improvement.
    </ol>
  Returns 1 plus the number of symmetric variables if successful; 0
  otherwise.]

  SideEffects [None]

  SeeAlso     [cuddSymmSifting]

******************************************************************************/
int cuddSymmSiftingConv(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int size;
  int x;
  int result;
  int symvars;
  int symgroups;
  int classes;
  int initialSize;
#ifdef DD_STATS
  int previousSize;
#endif

  initialSize = table->keys - table->isolated;

  size = table->size;

  /* Find order in which to sift variables. */
  var = NULL;
  entry = ALLOC(int, size);
  if (entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddSymmSiftingConvOutOfMem;
  }
  var = ALLOC(int, size);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto ddSymmSiftingConvOutOfMem;
  }

  for (i = 0; i < size; i++) {
    x = table->perm[i];
    entry[i] = table->subtables[x].keys;
    var[i] = i;
  }

  qsort((void *)var, size, sizeof(int), (DD_QSFP)ddSymmUniqueCompare);

  /* Initialize the symmetry of each subtable to itself
  ** for first pass of converging symmetric sifting.
  */
  for (i = lower; i <= upper; i++) {
    table->subtables[i].next = i;
  }

  for (i = 0; i < ddMin(table->siftMaxVar, table->size); i++) {
    if (ddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (util_cpu_time() - table->startTime > table->timeLimit) {
      table->autoDyn = 0; /* prevent further reordering */
      break;
    }
    x = table->perm[var[i]];
    if (x < lower || x > upper)
      continue;
    /* Only sift if not in symmetry group already. */
    if (table->subtables[x].next == (unsigned)x) {
#ifdef DD_STATS
      previousSize = table->keys - table->isolated;
#endif
      result = ddSymmSiftingAux(table, x, lower, upper);
      if (!result)
        goto ddSymmSiftingConvOutOfMem;
#ifdef DD_STATS
      if (table->keys < (unsigned)previousSize + table->isolated) {
        (void)fprintf(table->out, "-");
      } else if (table->keys > (unsigned)previousSize + table->isolated) {
        (void)fprintf(table->out, "+");
      } else {
        (void)fprintf(table->out, "=");
      }
      fflush(table->out);
#endif
    }
  }

  /* Sifting now until convergence. */
  while ((unsigned)initialSize > table->keys - table->isolated) {
    initialSize = table->keys - table->isolated;
#ifdef DD_STATS
    (void)fprintf(table->out, "\n");
#endif
    /* Here we consider only one representative for each symmetry class. */
    for (x = lower, classes = 0; x <= upper; x++, classes++) {
      while ((unsigned)x < table->subtables[x].next) {
        x = table->subtables[x].next;
      }
      /* Here x is the largest index in a group.
      ** Groups consist of adjacent variables.
      ** Hence, the next increment of x will move it to a new group.
      */
      i = table->invperm[x];
      entry[i] = table->subtables[x].keys;
      var[classes] = i;
    }

    qsort((void *)var, classes, sizeof(int), (DD_QSFP)ddSymmUniqueCompare);

    /* Now sift. */
    for (i = 0; i < ddMin(table->siftMaxVar, classes); i++) {
      if (ddTotalNumberSwapping >= table->siftMaxSwap)
        break;
      if (util_cpu_time() - table->startTime > table->timeLimit) {
        table->autoDyn = 0; /* prevent further reordering */
        break;
      }
      x = table->perm[var[i]];
      if ((unsigned)x >= table->subtables[x].next) {
#ifdef DD_STATS
        previousSize = table->keys - table->isolated;
#endif
        result = ddSymmSiftingConvAux(table, x, lower, upper);
        if (!result)
          goto ddSymmSiftingConvOutOfMem;
#ifdef DD_STATS
        if (table->keys < (unsigned)previousSize + table->isolated) {
          (void)fprintf(table->out, "-");
        } else if (table->keys > (unsigned)previousSize + table->isolated) {
          (void)fprintf(table->out, "+");
        } else {
          (void)fprintf(table->out, "=");
        }
        fflush(table->out);
#endif
      }
    } /* for */
  }

  ddSymmSummary(table, lower, upper, &symvars, &symgroups);

#ifdef DD_STATS
  (void)fprintf(table->out, "\n#:S_SIFTING %8d: symmetric variables\n",
                symvars);
  (void)fprintf(table->out, "#:G_SIFTING %8d: symmetric groups", symgroups);
#endif

  FREE(var);
  FREE(entry);

  return (1 + symvars);

ddSymmSiftingConvOutOfMem:

  if (entry != NULL)
    FREE(entry);
  if (var != NULL)
    FREE(var);

  return (0);

} /* end of cuddSymmSiftingConv */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the variables
  according to the number of keys in the subtables.
  Returns the difference in number of keys between the two
  variables being compared.]

  SideEffects [None]

******************************************************************************/
static int ddSymmUniqueCompare(int *ptrX, int *ptrY) {
#if 0
if (entry[*ptrY] == entry[*ptrX]) {
return((*ptrX) - (*ptrY));
}
#endif
  return (entry[*ptrY] - entry[*ptrX]);

} /* end of ddSymmUniqueCompare */

/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Assumes that x is not part of a symmetry group. Returns 1 if
  successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSymmSiftingAux(DdManager *table, int x, int xLow, int xHigh) {
  Move *move;
  Move *moveUp;   /* list of up moves */
  Move *moveDown; /* list of down moves */
  int initialSize;
  int result;
  int i;
  int topbot; /* index to either top or bottom of symmetry group */
  int initGroupSize, finalGroupSize;

#ifdef DD_DEBUG
  /* check for previously detected symmetry */
  assert(table->subtables[x].next == (unsigned)x);
#endif

  initialSize = table->keys - table->isolated;

  moveDown = NULL;
  moveUp = NULL;

  if ((x - xLow) > (xHigh - x)) {
    /* Will go down first, unless x == xHigh:
    ** Look for consecutive symmetries above x.
    */
    for (i = x; i > xLow; i--) {
      if (!cuddSymmCheck(table, i - 1, i))
        break;
      topbot = table->subtables[i - 1].next; /* find top of i-1's group */
      table->subtables[i - 1].next = i;
      table->subtables[x].next = topbot; /* x is bottom of group so its */
      /* next is top of i-1's group */
      i = topbot + 1; /* add 1 for i--; new i is top of symm group */
    }
  } else {
    /* Will go up first unless x == xlow:
    ** Look for consecutive symmetries below x.
    */
    for (i = x; i < xHigh; i++) {
      if (!cuddSymmCheck(table, i, i + 1))
        break;
      /* find bottom of i+1's symm group */
      topbot = i + 1;
      while ((unsigned)topbot < table->subtables[topbot].next) {
        topbot = table->subtables[topbot].next;
      }
      table->subtables[topbot].next = table->subtables[i].next;
      table->subtables[i].next = i + 1;
      i = topbot - 1; /* subtract 1 for i++; new i is bottom of group */
    }
  }

  /* Now x may be in the middle of a symmetry group.
  ** Find bottom of x's symm group.
  */
  while ((unsigned)x < table->subtables[x].next)
    x = table->subtables[x].next;

  if (x == xLow) { /* Sift down */

#ifdef DD_DEBUG
    /* x must be a singleton */
    assert((unsigned)x == table->subtables[x].next);
#endif
    if (x == xHigh)
      return (1); /* just one variable */

    initGroupSize = 1;

    moveDown = ddSymmSiftingDown(table, x, xHigh);
    /* after this point x --> xHigh, unless early term */
    if (moveDown == MV_OOM)
      goto ddSymmSiftingAuxOutOfMem;
    if (moveDown == NULL)
      return (1);

    x = moveDown->y;
    /* Find bottom of x's group */
    i = x;
    while ((unsigned)i < table->subtables[i].next) {
      i = table->subtables[i].next;
    }
#ifdef DD_DEBUG
    /* x should be the top of the symmetry group and i the bottom */
    assert((unsigned)i >= table->subtables[i].next);
    assert((unsigned)x == table->subtables[i].next);
#endif
    finalGroupSize = i - x + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetry groups detected, return to best position */
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    } else {
      initialSize = table->keys - table->isolated;
      moveUp = ddSymmSiftingUp(table, x, xLow);
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    }
    if (!result)
      goto ddSymmSiftingAuxOutOfMem;

  } else if (cuddNextHigh(table, x) > xHigh) { /* Sift up */
    /* Find top of x's symm group */
    i = x;                        /* bottom */
    x = table->subtables[x].next; /* top */

    if (x == xLow)
      return (1); /* just one big group */

    initGroupSize = i - x + 1;

    moveUp = ddSymmSiftingUp(table, x, xLow);
    /* after this point x --> xLow, unless early term */
    if (moveUp == MV_OOM)
      goto ddSymmSiftingAuxOutOfMem;
    if (moveUp == NULL)
      return (1);

    x = moveUp->x;
    /* Find top of x's group */
    i = table->subtables[x].next;
#ifdef DD_DEBUG
    /* x should be the bottom of the symmetry group and i the top */
    assert((unsigned)x >= table->subtables[x].next);
    assert((unsigned)i == table->subtables[x].next);
#endif
    finalGroupSize = x - i + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetry groups detected, return to best position */
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    } else {
      initialSize = table->keys - table->isolated;
      moveDown = ddSymmSiftingDown(table, x, xHigh);
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    }
    if (!result)
      goto ddSymmSiftingAuxOutOfMem;

  } else if ((x - xLow) > (xHigh - x)) { /* must go down first: shorter */

    moveDown = ddSymmSiftingDown(table, x, xHigh);
    /* at this point x == xHigh, unless early term */
    if (moveDown == MV_OOM)
      goto ddSymmSiftingAuxOutOfMem;

    if (moveDown != NULL) {
      x = moveDown->y; /* x is top here */
      i = x;
      while ((unsigned)i < table->subtables[i].next) {
        i = table->subtables[i].next;
      }
    } else {
      i = x;
      while ((unsigned)i < table->subtables[i].next) {
        i = table->subtables[i].next;
      }
      x = table->subtables[i].next;
    }
#ifdef DD_DEBUG
    /* x should be the top of the symmetry group and i the bottom */
    assert((unsigned)i >= table->subtables[i].next);
    assert((unsigned)x == table->subtables[i].next);
#endif
    initGroupSize = i - x + 1;

    moveUp = ddSymmSiftingUp(table, x, xLow);
    if (moveUp == MV_OOM)
      goto ddSymmSiftingAuxOutOfMem;

    if (moveUp != NULL) {
      x = moveUp->x;
      i = table->subtables[x].next;
    } else {
      i = x;
      while ((unsigned)x < table->subtables[x].next)
        x = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x should be the bottom of the symmetry group and i the top */
    assert((unsigned)x >= table->subtables[x].next);
    assert((unsigned)i == table->subtables[x].next);
#endif
    finalGroupSize = x - i + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetry groups detected, return to best position */
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    } else {
      while (moveDown != NULL) {
        move = moveDown->next;
        cuddDeallocMove(table, moveDown);
        moveDown = move;
      }
      initialSize = table->keys - table->isolated;
      moveDown = ddSymmSiftingDown(table, x, xHigh);
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    }
    if (!result)
      goto ddSymmSiftingAuxOutOfMem;

  } else { /* moving up first: shorter */
    /* Find top of x's symmetry group */
    x = table->subtables[x].next;

    moveUp = ddSymmSiftingUp(table, x, xLow);
    /* at this point x == xHigh, unless early term */
    if (moveUp == MV_OOM)
      goto ddSymmSiftingAuxOutOfMem;

    if (moveUp != NULL) {
      x = moveUp->x;
      i = table->subtables[x].next;
    } else {
      while ((unsigned)x < table->subtables[x].next)
        x = table->subtables[x].next;
      i = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x is bottom of the symmetry group and i is top */
    assert((unsigned)x >= table->subtables[x].next);
    assert((unsigned)i == table->subtables[x].next);
#endif
    initGroupSize = x - i + 1;

    moveDown = ddSymmSiftingDown(table, x, xHigh);
    if (moveDown == MV_OOM)
      goto ddSymmSiftingAuxOutOfMem;

    if (moveDown != NULL) {
      x = moveDown->y;
      i = x;
      while ((unsigned)i < table->subtables[i].next) {
        i = table->subtables[i].next;
      }
    } else {
      i = x;
      x = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x should be the top of the symmetry group and i the bottom */
    assert((unsigned)i >= table->subtables[i].next);
    assert((unsigned)x == table->subtables[i].next);
#endif
    finalGroupSize = i - x + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetries detected, go back to best position */
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    } else {
      while (moveUp != NULL) {
        move = moveUp->next;
        cuddDeallocMove(table, moveUp);
        moveUp = move;
      }
      initialSize = table->keys - table->isolated;
      moveUp = ddSymmSiftingUp(table, x, xLow);
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    }
    if (!result)
      goto ddSymmSiftingAuxOutOfMem;
  }

  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (1);

ddSymmSiftingAuxOutOfMem:
  if (moveDown != MV_OOM) {
    while (moveDown != NULL) {
      move = moveDown->next;
      cuddDeallocMove(table, moveDown);
      moveDown = move;
    }
  }
  if (moveUp != MV_OOM) {
    while (moveUp != NULL) {
      move = moveUp->next;
      cuddDeallocMove(table, moveUp);
      moveUp = move;
    }
  }

  return (0);

} /* end of ddSymmSiftingAux */

/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Assumes that x is either an isolated variable, or it is the bottom of
  a symmetry group. All symmetries may not have been found, because of
  exceeded growth limit. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSymmSiftingConvAux(DdManager *table, int x, int xLow, int xHigh) {
  Move *move;
  Move *moveUp;   /* list of up moves */
  Move *moveDown; /* list of down moves */
  int initialSize;
  int result;
  int i;
  int initGroupSize, finalGroupSize;

  initialSize = table->keys - table->isolated;

  moveDown = NULL;
  moveUp = NULL;

  if (x == xLow) { /* Sift down */
#ifdef DD_DEBUG
    /* x is bottom of symmetry group */
    assert((unsigned)x >= table->subtables[x].next);
#endif
    i = table->subtables[x].next;
    initGroupSize = x - i + 1;

    moveDown = ddSymmSiftingDown(table, x, xHigh);
    /* at this point x == xHigh, unless early term */
    if (moveDown == MV_OOM)
      goto ddSymmSiftingConvAuxOutOfMem;
    if (moveDown == NULL)
      return (1);

    x = moveDown->y;
    i = x;
    while ((unsigned)i < table->subtables[i].next) {
      i = table->subtables[i].next;
    }
#ifdef DD_DEBUG
    /* x should be the top of the symmetric group and i the bottom */
    assert((unsigned)i >= table->subtables[i].next);
    assert((unsigned)x == table->subtables[i].next);
#endif
    finalGroupSize = i - x + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetries detected, go back to best position */
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    } else {
      initialSize = table->keys - table->isolated;
      moveUp = ddSymmSiftingUp(table, x, xLow);
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    }
    if (!result)
      goto ddSymmSiftingConvAuxOutOfMem;

  } else if (cuddNextHigh(table, x) > xHigh) { /* Sift up */
    /* Find top of x's symm group */
    while ((unsigned)x < table->subtables[x].next)
      x = table->subtables[x].next;
    i = x;                        /* bottom */
    x = table->subtables[x].next; /* top */

    if (x == xLow)
      return (1);

    initGroupSize = i - x + 1;

    moveUp = ddSymmSiftingUp(table, x, xLow);
    /* at this point x == xLow, unless early term */
    if (moveUp == MV_OOM)
      goto ddSymmSiftingConvAuxOutOfMem;
    if (moveUp == NULL)
      return (1);

    x = moveUp->x;
    i = table->subtables[x].next;
#ifdef DD_DEBUG
    /* x should be the bottom of the symmetry group and i the top */
    assert((unsigned)x >= table->subtables[x].next);
    assert((unsigned)i == table->subtables[x].next);
#endif
    finalGroupSize = x - i + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetry groups detected, return to best position */
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    } else {
      initialSize = table->keys - table->isolated;
      moveDown = ddSymmSiftingDown(table, x, xHigh);
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    }
    if (!result)
      goto ddSymmSiftingConvAuxOutOfMem;

  } else if ((x - xLow) > (xHigh - x)) { /* must go down first: shorter */
    moveDown = ddSymmSiftingDown(table, x, xHigh);
    /* at this point x == xHigh, unless early term */
    if (moveDown == MV_OOM)
      goto ddSymmSiftingConvAuxOutOfMem;

    if (moveDown != NULL) {
      x = moveDown->y;
      i = x;
      while ((unsigned)i < table->subtables[i].next) {
        i = table->subtables[i].next;
      }
    } else {
      while ((unsigned)x < table->subtables[x].next)
        x = table->subtables[x].next;
      i = x;
      x = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x should be the top of the symmetry group and i the bottom */
    assert((unsigned)i >= table->subtables[i].next);
    assert((unsigned)x == table->subtables[i].next);
#endif
    initGroupSize = i - x + 1;

    moveUp = ddSymmSiftingUp(table, x, xLow);
    if (moveUp == MV_OOM)
      goto ddSymmSiftingConvAuxOutOfMem;

    if (moveUp != NULL) {
      x = moveUp->x;
      i = table->subtables[x].next;
    } else {
      i = x;
      while ((unsigned)x < table->subtables[x].next)
        x = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x should be the bottom of the symmetry group and i the top */
    assert((unsigned)x >= table->subtables[x].next);
    assert((unsigned)i == table->subtables[x].next);
#endif
    finalGroupSize = x - i + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetry groups detected, return to best position */
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    } else {
      while (moveDown != NULL) {
        move = moveDown->next;
        cuddDeallocMove(table, moveDown);
        moveDown = move;
      }
      initialSize = table->keys - table->isolated;
      moveDown = ddSymmSiftingDown(table, x, xHigh);
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    }
    if (!result)
      goto ddSymmSiftingConvAuxOutOfMem;

  } else { /* moving up first: shorter */
    /* Find top of x's symmetry group */
    x = table->subtables[x].next;

    moveUp = ddSymmSiftingUp(table, x, xLow);
    /* at this point x == xHigh, unless early term */
    if (moveUp == MV_OOM)
      goto ddSymmSiftingConvAuxOutOfMem;

    if (moveUp != NULL) {
      x = moveUp->x;
      i = table->subtables[x].next;
    } else {
      i = x;
      while ((unsigned)x < table->subtables[x].next)
        x = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x is bottom of the symmetry group and i is top */
    assert((unsigned)x >= table->subtables[x].next);
    assert((unsigned)i == table->subtables[x].next);
#endif
    initGroupSize = x - i + 1;

    moveDown = ddSymmSiftingDown(table, x, xHigh);
    if (moveDown == MV_OOM)
      goto ddSymmSiftingConvAuxOutOfMem;

    if (moveDown != NULL) {
      x = moveDown->y;
      i = x;
      while ((unsigned)i < table->subtables[i].next) {
        i = table->subtables[i].next;
      }
    } else {
      i = x;
      x = table->subtables[x].next;
    }
#ifdef DD_DEBUG
    /* x should be the top of the symmetry group and i the bottom */
    assert((unsigned)i >= table->subtables[i].next);
    assert((unsigned)x == table->subtables[i].next);
#endif
    finalGroupSize = i - x + 1;

    if (initGroupSize == finalGroupSize) {
      /* No new symmetries detected, go back to best position */
      result = ddSymmSiftingBackward(table, moveDown, initialSize);
    } else {
      while (moveUp != NULL) {
        move = moveUp->next;
        cuddDeallocMove(table, moveUp);
        moveUp = move;
      }
      initialSize = table->keys - table->isolated;
      moveUp = ddSymmSiftingUp(table, x, xLow);
      result = ddSymmSiftingBackward(table, moveUp, initialSize);
    }
    if (!result)
      goto ddSymmSiftingConvAuxOutOfMem;
  }

  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (1);

ddSymmSiftingConvAuxOutOfMem:
  if (moveDown != MV_OOM) {
    while (moveDown != NULL) {
      move = moveDown->next;
      cuddDeallocMove(table, moveDown);
      moveDown = move;
    }
  }
  if (moveUp != MV_OOM) {
    while (moveUp != NULL) {
      move = moveUp->next;
      cuddDeallocMove(table, moveUp);
      moveUp = move;
    }
  }

  return (0);

} /* end of ddSymmSiftingConvAux */

/**Function********************************************************************

  Synopsis    [Moves x up until either it reaches the bound (xLow) or
  the size of the DD heap increases too much.]

  Description [Moves x up until either it reaches the bound (xLow) or
  the size of the DD heap increases too much. Assumes that x is the top
  of a symmetry group.  Checks x for symmetry to the adjacent
  variables. If symmetry is found, the symmetry group of x is merged
  with the symmetry group of the other variable. Returns the set of
  moves in case of success; MV_OOM if memory is full.]

  SideEffects [None]

******************************************************************************/
static Move *ddSymmSiftingUp(DdManager *table, int y, int xLow) {
  Move *moves;
  Move *move;
  int x;
  int size;
  int i;
  int gxtop, gybot;
  int limitSize;
  int xindex, yindex;
  int zindex;
  int z;
  int isolated;
  int L; /* lower bound on DD size */
#ifdef DD_DEBUG
  int checkL;
#endif

  moves = NULL;
  yindex = table->invperm[y];

  /* Initialize the lower bound.
  ** The part of the DD below the bottom of y' group will not change.
  ** The part of the DD above y that does not interact with y will not
  ** change. The rest may vanish in the best case, except for
  ** the nodes at level xLow, which will not vanish, regardless.
  */
  limitSize = L = table->keys - table->isolated;
  gybot = y;
  while ((unsigned)gybot < table->subtables[gybot].next)
    gybot = table->subtables[gybot].next;
  for (z = xLow + 1; z <= gybot; z++) {
    zindex = table->invperm[z];
    if (zindex == yindex || cuddTestInteract(table, zindex, yindex)) {
      isolated = table->vars[zindex]->ref == 1;
      L -= table->subtables[z].keys - isolated;
    }
  }

  x = cuddNextLow(table, y);
  while (x >= xLow && L <= limitSize) {
#ifdef DD_DEBUG
    gybot = y;
    while ((unsigned)gybot < table->subtables[gybot].next)
      gybot = table->subtables[gybot].next;
    checkL = table->keys - table->isolated;
    for (z = xLow + 1; z <= gybot; z++) {
      zindex = table->invperm[z];
      if (zindex == yindex || cuddTestInteract(table, zindex, yindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkL -= table->subtables[z].keys - isolated;
      }
    }
    assert(L == checkL);
#endif
    gxtop = table->subtables[x].next;
    if (cuddSymmCheck(table, x, y)) {
      /* Symmetry found, attach symm groups */
      table->subtables[x].next = y;
      i = table->subtables[y].next;
      while (table->subtables[i].next != (unsigned)y)
        i = table->subtables[i].next;
      table->subtables[i].next = gxtop;
    } else if (table->subtables[x].next == (unsigned)x &&
               table->subtables[y].next == (unsigned)y) {
      /* x and y have self symmetry */
      xindex = table->invperm[x];
      size = cuddSwapInPlace(table, x, y);
#ifdef DD_DEBUG
      assert(table->subtables[x].next == (unsigned)x);
      assert(table->subtables[y].next == (unsigned)y);
#endif
      if (size == 0)
        goto ddSymmSiftingUpOutOfMem;
      /* Update the lower bound. */
      if (cuddTestInteract(table, xindex, yindex)) {
        isolated = table->vars[xindex]->ref == 1;
        L += table->subtables[y].keys - isolated;
      }
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSymmSiftingUpOutOfMem;
      move->x = x;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (moves);
      if (size < limitSize)
        limitSize = size;
    } else { /* Group move */
      size = ddSymmGroupMove(table, x, y, &moves);
      if (size == 0)
        goto ddSymmSiftingUpOutOfMem;
      /* Update the lower bound. */
      z = moves->y;
      do {
        zindex = table->invperm[z];
        if (cuddTestInteract(table, zindex, yindex)) {
          isolated = table->vars[zindex]->ref == 1;
          L += table->subtables[z].keys - isolated;
        }
        z = table->subtables[z].next;
      } while (z != (int)moves->y);
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (moves);
      if (size < limitSize)
        limitSize = size;
    }
    y = gxtop;
    x = cuddNextLow(table, y);
  }

  return (moves);

ddSymmSiftingUpOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (MV_OOM);

} /* end of ddSymmSiftingUp */

/**Function********************************************************************

  Synopsis    [Moves x down until either it reaches the bound (xHigh) or
  the size of the DD heap increases too much.]

  Description [Moves x down until either it reaches the bound (xHigh)
  or the size of the DD heap increases too much. Assumes that x is the
  bottom of a symmetry group. Checks x for symmetry to the adjacent
  variables. If symmetry is found, the symmetry group of x is merged
  with the symmetry group of the other variable. Returns the set of
  moves in case of success; MV_OOM if memory is full.]

  SideEffects [None]

******************************************************************************/
static Move *ddSymmSiftingDown(DdManager *table, int x, int xHigh) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limitSize;
  int gxtop, gybot;
  int R; /* upper bound on node decrease */
  int xindex, yindex;
  int isolated;
  int z;
  int zindex;
#ifdef DD_DEBUG
  int checkR;
#endif

  moves = NULL;
  /* Initialize R */
  xindex = table->invperm[x];
  gxtop = table->subtables[x].next;
  limitSize = size = table->keys - table->isolated;
  R = 0;
  for (z = xHigh; z > gxtop; z--) {
    zindex = table->invperm[z];
    if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
      isolated = table->vars[zindex]->ref == 1;
      R += table->subtables[z].keys - isolated;
    }
  }

  y = cuddNextHigh(table, x);
  while (y <= xHigh && size - R < limitSize) {
#ifdef DD_DEBUG
    gxtop = table->subtables[x].next;
    checkR = 0;
    for (z = xHigh; z > gxtop; z--) {
      zindex = table->invperm[z];
      if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
        isolated = table->vars[zindex]->ref == 1;
        checkR += table->subtables[z].keys - isolated;
      }
    }
    assert(R == checkR);
#endif
    gybot = table->subtables[y].next;
    while (table->subtables[gybot].next != (unsigned)y)
      gybot = table->subtables[gybot].next;
    if (cuddSymmCheck(table, x, y)) {
      /* Symmetry found, attach symm groups */
      gxtop = table->subtables[x].next;
      table->subtables[x].next = y;
      table->subtables[gybot].next = gxtop;
    } else if (table->subtables[x].next == (unsigned)x &&
               table->subtables[y].next == (unsigned)y) {
      /* x and y have self symmetry */
      /* Update upper bound on node decrease. */
      yindex = table->invperm[y];
      if (cuddTestInteract(table, xindex, yindex)) {
        isolated = table->vars[yindex]->ref == 1;
        R -= table->subtables[y].keys - isolated;
      }
      size = cuddSwapInPlace(table, x, y);
#ifdef DD_DEBUG
      assert(table->subtables[x].next == (unsigned)x);
      assert(table->subtables[y].next == (unsigned)y);
#endif
      if (size == 0)
        goto ddSymmSiftingDownOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto ddSymmSiftingDownOutOfMem;
      move->x = x;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (moves);
      if (size < limitSize)
        limitSize = size;
    } else { /* Group move */
      /* Update upper bound on node decrease: first phase. */
      gxtop = table->subtables[x].next;
      z = gxtop + 1;
      do {
        zindex = table->invperm[z];
        if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
          isolated = table->vars[zindex]->ref == 1;
          R -= table->subtables[z].keys - isolated;
        }
        z++;
      } while (z <= gybot);
      size = ddSymmGroupMove(table, x, y, &moves);
      if (size == 0)
        goto ddSymmSiftingDownOutOfMem;
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (moves);
      if (size < limitSize)
        limitSize = size;
      /* Update upper bound on node decrease: second phase. */
      gxtop = table->subtables[gybot].next;
      for (z = gxtop + 1; z <= gybot; z++) {
        zindex = table->invperm[z];
        if (zindex == xindex || cuddTestInteract(table, xindex, zindex)) {
          isolated = table->vars[zindex]->ref == 1;
          R += table->subtables[z].keys - isolated;
        }
      }
    }
    x = gybot;
    y = cuddNextHigh(table, x);
  }

  return (moves);

ddSymmSiftingDownOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (MV_OOM);

} /* end of ddSymmSiftingDown */

/**Function********************************************************************

  Synopsis    [Swaps two groups.]

  Description [Swaps two groups. x is assumed to be the bottom variable
  of the first group. y is assumed to be the top variable of the second
  group.  Updates the list of moves. Returns the number of keys in the
  table if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSymmGroupMove(DdManager *table, int x, int y, Move **moves) {
  Move *move;
  int size;
  int i, j;
  int xtop, xbot, xsize, ytop, ybot, ysize, newxtop;
  int swapx, swapy;

#ifdef DD_DEBUG
  assert(x < y); /* we assume that x < y */
#endif
  /* Find top, bottom, and size for the two groups. */
  xbot = x;
  xtop = table->subtables[x].next;
  xsize = xbot - xtop + 1;
  ybot = y;
  while ((unsigned)ybot < table->subtables[ybot].next)
    ybot = table->subtables[ybot].next;
  ytop = y;
  ysize = ybot - ytop + 1;

  /* Sift the variables of the second group up through the first group. */
  for (i = 1; i <= ysize; i++) {
    for (j = 1; j <= xsize; j++) {
      size = cuddSwapInPlace(table, x, y);
      if (size == 0)
        return (0);
      swapx = x;
      swapy = y;
      y = x;
      x = y - 1;
    }
    y = ytop + i;
    x = y - 1;
  }

  /* fix symmetries */
  y = xtop; /* ytop is now where xtop used to be */
  for (i = 0; i < ysize - 1; i++) {
    table->subtables[y].next = y + 1;
    y = y + 1;
  }
  table->subtables[y].next = xtop; /* y is bottom of its group, join */
  /* its symmetry to top of its group */
  x = y + 1;
  newxtop = x;
  for (i = 0; i < xsize - 1; i++) {
    table->subtables[x].next = x + 1;
    x = x + 1;
  }
  table->subtables[x].next = newxtop; /* x is bottom of its group, join */
  /* its symmetry to top of its group */
  /* Store group move */
  move = (Move *)cuddDynamicAllocNode(table);
  if (move == NULL)
    return (0);
  move->x = swapx;
  move->y = swapy;
  move->size = size;
  move->next = *moves;
  *moves = move;

  return (size);

} /* end of ddSymmGroupMove */

/**Function********************************************************************

  Synopsis    [Undoes the swap of two groups.]

  Description [Undoes the swap of two groups. x is assumed to be the
  bottom variable of the first group. y is assumed to be the top
  variable of the second group.  Returns the number of keys in the table
  if successful; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSymmGroupMoveBackward(DdManager *table, int x, int y) {
  int size;
  int i, j;
  int xtop, xbot, xsize, ytop, ybot, ysize, newxtop;

#ifdef DD_DEBUG
  assert(x < y); /* We assume that x < y */
#endif

  /* Find top, bottom, and size for the two groups. */
  xbot = x;
  xtop = table->subtables[x].next;
  xsize = xbot - xtop + 1;
  ybot = y;
  while ((unsigned)ybot < table->subtables[ybot].next)
    ybot = table->subtables[ybot].next;
  ytop = y;
  ysize = ybot - ytop + 1;

  /* Sift the variables of the second group up through the first group. */
  for (i = 1; i <= ysize; i++) {
    for (j = 1; j <= xsize; j++) {
      size = cuddSwapInPlace(table, x, y);
      if (size == 0)
        return (0);
      y = x;
      x = cuddNextLow(table, y);
    }
    y = ytop + i;
    x = y - 1;
  }

  /* Fix symmetries. */
  y = xtop;
  for (i = 0; i < ysize - 1; i++) {
    table->subtables[y].next = y + 1;
    y = y + 1;
  }
  table->subtables[y].next = xtop; /* y is bottom of its group, join */
  /* its symmetry to top of its group */
  x = y + 1;
  newxtop = x;
  for (i = 0; i < xsize - 1; i++) {
    table->subtables[x].next = x + 1;
    x = x + 1;
  }
  table->subtables[x].next = newxtop; /* x is bottom of its group, join */
  /* its symmetry to top of its group */

  return (size);

} /* end of ddSymmGroupMoveBackward */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the DD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the DD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddSymmSiftingBackward(DdManager *table, Move *moves, int size) {
  Move *move;
  int res;

  for (move = moves; move != NULL; move = move->next) {
    if (move->size < size) {
      size = move->size;
    }
  }

  for (move = moves; move != NULL; move = move->next) {
    if (move->size == size)
      return (1);
    if (table->subtables[move->x].next == move->x &&
        table->subtables[move->y].next == move->y) {
      res = cuddSwapInPlace(table, (int)move->x, (int)move->y);
#ifdef DD_DEBUG
      assert(table->subtables[move->x].next == move->x);
      assert(table->subtables[move->y].next == move->y);
#endif
    } else { /* Group move necessary */
      res = ddSymmGroupMoveBackward(table, (int)move->x, (int)move->y);
    }
    if (!res)
      return (0);
  }

  return (1);

} /* end of ddSymmSiftingBackward */

/**Function********************************************************************

  Synopsis    [Counts numbers of symmetric variables and symmetry
  groups.]

  Description []

  SideEffects [None]

******************************************************************************/
static void ddSymmSummary(DdManager *table, int lower, int upper, int *symvars,
                          int *symgroups) {
  int i, x, gbot;
  int TotalSymm = 0;
  int TotalSymmGroups = 0;

  for (i = lower; i <= upper; i++) {
    if (table->subtables[i].next != (unsigned)i) {
      TotalSymmGroups++;
      x = i;
      do {
        TotalSymm++;
        gbot = x;
        x = table->subtables[x].next;
      } while (x != i);
#ifdef DD_DEBUG
      assert(table->subtables[gbot].next == (unsigned)i);
#endif
      i = gbot;
    }
  }
  *symvars = TotalSymm;
  *symgroups = TotalSymmGroups;

  return;

} /* end of ddSymmSummary */

/**CFile***********************************************************************

  FileName    [cuddTable.c]

  PackageName [cudd]

  Synopsis    [Unique table management functions.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_Prime()
                <li> Cudd_Reserve()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddAllocNode()
                <li> cuddInitTable()
                <li> cuddFreeTable()
                <li> cuddGarbageCollect()
                <li> cuddZddGetNode()
                <li> cuddZddGetNodeIVO()
                <li> cuddUniqueInter()
                <li> cuddUniqueInterIVO()
                <li> cuddUniqueInterZdd()
                <li> cuddUniqueConst()
                <li> cuddRehash()
                <li> cuddShrinkSubtable()
                <li> cuddInsertSubtables()
                <li> cuddDestroySubtables()
                <li> cuddResizeTableZdd()
                <li> cuddSlowTableGrowth()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> ddRehashZdd()
                <li> ddResizeTable()
                <li> cuddFindParent()
                <li> cuddOrderedInsert()
                <li> cuddOrderedThread()
                <li> cuddRotateLeft()
                <li> cuddRotateRight()
                <li> cuddDoRebalance()
                <li> cuddCheckCollisionOrdering()
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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
/* Constants for red/black trees. */
#define DD_STACK_SIZE 128
#define DD_RED 0
#define DD_BLACK 1
#define DD_PAGE_SIZE 8192
#define DD_PAGE_MASK ~(DD_PAGE_SIZE - 1)
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/* This is a hack for when CUDD_VALUE_TYPE is double */
typedef union hack {
  CUDD_VALUE_TYPE value;
  unsigned int bits[2];
} hack;

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

//#ifndef lint
// static char rcsid[] DD_UNUSED = "$Id: cuddTable.c,v 1.126 2012/02/05 01:07:19
// fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
/* Macros for red/black trees. */
#define DD_INSERT_COMPARE(x, y)                                                \
  (((ptruint)(x)&DD_PAGE_MASK) - ((ptruint)(y)&DD_PAGE_MASK))
#define DD_COLOR(p) ((p)->index)
#define DD_IS_BLACK(p) ((p)->index == DD_BLACK)
#define DD_IS_RED(p) ((p)->index == DD_RED)
#define DD_LEFT(p) cuddT(p)
#define DD_RIGHT(p) cuddE(p)
#define DD_NEXT(p) ((p)->next)
#endif
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static void ddRehashZdd(DdManager *unique, int i);
static int ddResizeTable(DdManager *unique, int index, int amount);

DD_INLINE static void ddFixLimits(DdManager *unique);
#ifdef DD_RED_BLACK_FREE_LIST
static void cuddOrderedInsert(DdNodePtr *root, DdNodePtr node);
static DdNode *cuddOrderedThread(DdNode *root, DdNode *list);
static void cuddRotateLeft(DdNodePtr *nodeP);
static void cuddRotateRight(DdNodePtr *nodeP);
static void cuddDoRebalance(DdNodePtr **stack, int stackN);
#endif
static void ddPatchTree(DdManager *dd, MtrNode *treenode);
#ifdef DD_DEBUG
static int cuddCheckCollisionOrdering(DdManager *unique, int i, int j);
#endif
static void ddReportRefMess(DdManager *unique, int i, const char *caller);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Fast storage allocation for DdNodes in the table.]

  Description [Fast storage allocation for DdNodes in the table. The
  first 4 bytes of a chunk contain a pointer to the next block; the
  rest contains DD_MEM_CHUNK spaces for DdNodes.  Returns a pointer to
  a new node if successful; NULL is memory is full.]

  SideEffects [None]

  SeeAlso     [cuddDynamicAllocNode]

******************************************************************************/
DdNode *cuddAllocNode(DdManager *unique) {
  int i;
  DdNodePtr *mem;
  DdNode *list, *node;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  if (unique->nextFree == NULL) { /* free list is empty */
    /* Check for exceeded limits. */
    if ((unique->keys - unique->dead) + (unique->keysZ - unique->deadZ) >
        unique->maxLive) {
      unique->errorCode = CUDD_TOO_MANY_NODES;
      return (NULL);
    }
    if (util_cpu_time() - unique->startTime > unique->timeLimit) {
      unique->errorCode = CUDD_TIMEOUT_EXPIRED;
      return (NULL);
    }
    if (unique->stash == NULL || unique->memused > unique->maxmemhard) {
      (void)cuddGarbageCollect(unique, 1);
      mem = NULL;
    }
    if (unique->nextFree == NULL) {
      if (unique->memused > unique->maxmemhard) {
        unique->errorCode = CUDD_MAX_MEM_EXCEEDED;
        return (NULL);
      }
      /* Try to allocate a new block. */
      saveHandler = MMoutOfMemory;
      MMoutOfMemory = Cudd_OutOfMem;
      mem = (DdNodePtr *)ALLOC(DdNode, DD_MEM_CHUNK + 1);
      MMoutOfMemory = saveHandler;
      if (mem == NULL) {
        /* No more memory: Try collecting garbage. If this succeeds,
        ** we end up with mem still NULL, but unique->nextFree !=
        ** NULL. */
        if (cuddGarbageCollect(unique, 1) == 0) {
          /* Last resort: Free the memory stashed away, if there
          ** any. If this succeeeds, mem != NULL and
          ** unique->nextFree still NULL. */
          if (unique->stash != NULL) {
            FREE(unique->stash);
            unique->stash = NULL;
            /* Inhibit resizing of tables. */
            cuddSlowTableGrowth(unique);
            /* Now try again. */
            mem = (DdNodePtr *)ALLOC(DdNode, DD_MEM_CHUNK + 1);
          }
          if (mem == NULL) {
            /* Out of luck. Call the default handler to do
            ** whatever it specifies for a failed malloc.
            ** If this handler returns, then set error code,
            ** print warning, and return. */
            (*MMoutOfMemory)(sizeof(DdNode) * (DD_MEM_CHUNK + 1));
            unique->errorCode = CUDD_MEMORY_OUT;
#ifdef DD_VERBOSE
            (void)fprintf(unique->err, "cuddAllocNode: out of memory");
            (void)fprintf(unique->err, "Memory in use = %lu\n",
                          unique->memused);
#endif
            return (NULL);
          }
        }
      }
      if (mem != NULL) { /* successful allocation; slice memory */
        ptruint offset;
        unique->memused += (DD_MEM_CHUNK + 1) * sizeof(DdNode);
        mem[0] = (DdNodePtr)unique->memoryList;
        unique->memoryList = mem;

        /* Here we rely on the fact that a DdNode is as large
        ** as 4 pointers.  */
        offset = (ptruint)mem & (sizeof(DdNode) - 1);
        mem += (sizeof(DdNode) - offset) / sizeof(DdNodePtr);
        assert(((ptruint)mem & (sizeof(DdNode) - 1)) == 0);
        list = (DdNode *)mem;

        i = 1;
        do {
          list[i - 1].ref = 0;
          list[i - 1].next = &list[i];
        } while (++i < DD_MEM_CHUNK);

        list[DD_MEM_CHUNK - 1].ref = 0;
        list[DD_MEM_CHUNK - 1].next = NULL;

        unique->nextFree = &list[0];
      }
    }
  }
  unique->allocated++;
  node = unique->nextFree;
  unique->nextFree = node->next;
  return (node);

} /* end of cuddAllocNode */

/**Function********************************************************************

  Synopsis    [Creates and initializes the unique table.]

  Description [Creates and initializes the unique table. Returns a pointer
  to the table if successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Init cuddFreeTable]

******************************************************************************/
DdManager *cuddInitTable(
    unsigned int numVars /* Initial number of BDD variables (and subtables) */,
    unsigned int numVarsZ /* Initial number of ZDD variables (and subtables) */,
    unsigned int numSlots /* Initial size of the BDD subtables */,
    unsigned int looseUpTo /* Limit for fast table growth */) {
  DdManager *unique = ALLOC(DdManager, 1);
  int i, j;
  DdNodePtr *nodelist;
  DdNode *sentinel;
  unsigned int slots;
  int shift;

  if (unique == NULL) {
    return (NULL);
  }
  sentinel = &(unique->sentinel);
  sentinel->ref = 0;
  sentinel->index = 0;
  cuddT(sentinel) = NULL;
  cuddE(sentinel) = NULL;
  sentinel->next = NULL;
  unique->epsilon = DD_EPSILON;
  unique->size = numVars;
  unique->sizeZ = numVarsZ;
  unique->maxSize = ddMax(DD_DEFAULT_RESIZE, numVars);
  unique->maxSizeZ = ddMax(DD_DEFAULT_RESIZE, numVarsZ);

  /* Adjust the requested number of slots to a power of 2. */
  slots = 8;
  while (slots < numSlots) {
    slots <<= 1;
  }
  unique->initSlots = slots;
  shift = sizeof(int) * 8 - cuddComputeFloorLog2(slots);

  unique->slots = (numVars + numVarsZ + 1) * slots;
  unique->keys = 0;
  unique->maxLive = ~0; /* very large number */
  unique->keysZ = 0;
  unique->dead = 0;
  unique->deadZ = 0;
  unique->gcFrac = DD_GC_FRAC_HI;
  unique->minDead = (unsigned)(DD_GC_FRAC_HI * (double)unique->slots);
  unique->looseUpTo = looseUpTo;
  unique->gcEnabled = 1;
  unique->allocated = 0;
  unique->reclaimed = 0;
  unique->subtables = ALLOC(DdSubtable, unique->maxSize);
  if (unique->subtables == NULL) {
    FREE(unique);
    return (NULL);
  }
  unique->subtableZ = ALLOC(DdSubtable, unique->maxSizeZ);
  if (unique->subtableZ == NULL) {
    FREE(unique->subtables);
    FREE(unique);
    return (NULL);
  }
  unique->perm = ALLOC(int, unique->maxSize);
  if (unique->perm == NULL) {
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique);
    return (NULL);
  }
  unique->invperm = ALLOC(int, unique->maxSize);
  if (unique->invperm == NULL) {
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique->perm);
    FREE(unique);
    return (NULL);
  }
  unique->permZ = ALLOC(int, unique->maxSizeZ);
  if (unique->permZ == NULL) {
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique->perm);
    FREE(unique->invperm);
    FREE(unique);
    return (NULL);
  }
  unique->invpermZ = ALLOC(int, unique->maxSizeZ);
  if (unique->invpermZ == NULL) {
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique->perm);
    FREE(unique->invperm);
    FREE(unique->permZ);
    FREE(unique);
    return (NULL);
  }
  unique->map = NULL;
  unique->stack =
      ALLOC(DdNodePtr, ddMax(unique->maxSize, unique->maxSizeZ) + 1);
  if (unique->stack == NULL) {
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique->perm);
    FREE(unique->invperm);
    FREE(unique->permZ);
    FREE(unique->invpermZ);
    FREE(unique);
    return (NULL);
  }
  unique->stack[0] = NULL; /* to suppress harmless UMR */

#ifndef DD_NO_DEATH_ROW
  unique->deathRowDepth = 1 << cuddComputeFloorLog2(unique->looseUpTo >> 2);
  unique->deathRow = ALLOC(DdNodePtr, unique->deathRowDepth);
  if (unique->deathRow == NULL) {
    FREE(unique->subtables);
    FREE(unique->subtableZ);
    FREE(unique->perm);
    FREE(unique->invperm);
    FREE(unique->permZ);
    FREE(unique->invpermZ);
    FREE(unique->stack);
    FREE(unique);
    return (NULL);
  }
  for (i = 0; i < unique->deathRowDepth; i++) {
    unique->deathRow[i] = NULL;
  }
  unique->nextDead = 0;
  unique->deadMask = unique->deathRowDepth - 1;
#endif

  for (i = 0; (unsigned)i < numVars; i++) {
    unique->subtables[i].slots = slots;
    unique->subtables[i].shift = shift;
    unique->subtables[i].keys = 0;
    unique->subtables[i].dead = 0;
    unique->subtables[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
    unique->subtables[i].bindVar = 0;
    unique->subtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
    unique->subtables[i].pairIndex = 0;
    unique->subtables[i].varHandled = 0;
    unique->subtables[i].varToBeGrouped = CUDD_LAZY_NONE;

    nodelist = unique->subtables[i].nodelist = ALLOC(DdNodePtr, slots);
    if (nodelist == NULL) {
      for (j = 0; j < i; j++) {
        FREE(unique->subtables[j].nodelist);
      }
      FREE(unique->subtables);
      FREE(unique->subtableZ);
      FREE(unique->perm);
      FREE(unique->invperm);
      FREE(unique->permZ);
      FREE(unique->invpermZ);
      FREE(unique->stack);
      FREE(unique);
      return (NULL);
    }
    for (j = 0; (unsigned)j < slots; j++) {
      nodelist[j] = sentinel;
    }
    unique->perm[i] = i;
    unique->invperm[i] = i;
  }
  for (i = 0; (unsigned)i < numVarsZ; i++) {
    unique->subtableZ[i].slots = slots;
    unique->subtableZ[i].shift = shift;
    unique->subtableZ[i].keys = 0;
    unique->subtableZ[i].dead = 0;
    unique->subtableZ[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
    nodelist = unique->subtableZ[i].nodelist = ALLOC(DdNodePtr, slots);
    if (nodelist == NULL) {
      for (j = 0; (unsigned)j < numVars; j++) {
        FREE(unique->subtables[j].nodelist);
      }
      FREE(unique->subtables);
      for (j = 0; j < i; j++) {
        FREE(unique->subtableZ[j].nodelist);
      }
      FREE(unique->subtableZ);
      FREE(unique->perm);
      FREE(unique->invperm);
      FREE(unique->permZ);
      FREE(unique->invpermZ);
      FREE(unique->stack);
      FREE(unique);
      return (NULL);
    }
    for (j = 0; (unsigned)j < slots; j++) {
      nodelist[j] = NULL;
    }
    unique->permZ[i] = i;
    unique->invpermZ[i] = i;
  }
  unique->constants.slots = slots;
  unique->constants.shift = shift;
  unique->constants.keys = 0;
  unique->constants.dead = 0;
  unique->constants.maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
  nodelist = unique->constants.nodelist = ALLOC(DdNodePtr, slots);
  if (nodelist == NULL) {
    for (j = 0; (unsigned)j < numVars; j++) {
      FREE(unique->subtables[j].nodelist);
    }
    FREE(unique->subtables);
    for (j = 0; (unsigned)j < numVarsZ; j++) {
      FREE(unique->subtableZ[j].nodelist);
    }
    FREE(unique->subtableZ);
    FREE(unique->perm);
    FREE(unique->invperm);
    FREE(unique->permZ);
    FREE(unique->invpermZ);
    FREE(unique->stack);
    FREE(unique);
    return (NULL);
  }
  for (j = 0; (unsigned)j < slots; j++) {
    nodelist[j] = NULL;
  }

  unique->memoryList = NULL;
  unique->nextFree = NULL;

  unique->memused =
      sizeof(DdManager) +
      (unique->maxSize + unique->maxSizeZ) *
          (sizeof(DdSubtable) + 2 * sizeof(int)) +
      (numVars + 1) * slots * sizeof(DdNodePtr) +
      (ddMax(unique->maxSize, unique->maxSizeZ) + 1) * sizeof(DdNodePtr);
#ifndef DD_NO_DEATH_ROW
  unique->memused += unique->deathRowDepth * sizeof(DdNodePtr);
#endif

  /* Initialize fields concerned with automatic dynamic reordering. */
  unique->reordered = 0;
  unique->reorderings = 0;
  unique->maxReorderings = ~0;
  unique->siftMaxVar = DD_SIFT_MAX_VAR;
  unique->siftMaxSwap = DD_SIFT_MAX_SWAPS;
  unique->maxGrowth = DD_MAX_REORDER_GROWTH;
  unique->maxGrowthAlt = 2.0 * DD_MAX_REORDER_GROWTH;
  unique->reordCycle = 0; /* do not use alternate threshold */
  unique->autoDyn = 0;    /* initially disabled */
  unique->autoDynZ = 0;   /* initially disabled */
  unique->autoMethod = CUDD_REORDER_SIFT;
  unique->autoMethodZ = CUDD_REORDER_SIFT;
  unique->realign = 0;  /* initially disabled */
  unique->realignZ = 0; /* initially disabled */
  unique->nextDyn = DD_FIRST_REORDER;
  unique->countDead = ~0;
  unique->tree = NULL;
  unique->treeZ = NULL;
  unique->groupcheck = CUDD_GROUP_CHECK7;
  unique->recomb = DD_DEFAULT_RECOMB;
  unique->symmviolation = 0;
  unique->arcviolation = 0;
  unique->populationSize = 0;
  unique->numberXovers = 0;
  unique->randomizeOrder = 0;
  unique->linear = NULL;
  unique->linearSize = 0;

  /* Initialize ZDD universe. */
  unique->univ = (DdNodePtr *)NULL;

  /* Initialize auxiliary fields. */
  unique->localCaches = NULL;
  unique->preGCHook = NULL;
  unique->postGCHook = NULL;
  unique->preReorderingHook = NULL;
  unique->postReorderingHook = NULL;
  unique->out = stdout;
  unique->err = stderr;
  unique->errorCode = CUDD_NO_ERROR;
  unique->startTime = util_cpu_time();
  unique->timeLimit = ~0UL;

  /* Initialize statistical counters. */
  unique->maxmemhard = ~0UL;
  unique->garbageCollections = 0;
  unique->GCTime = 0;
  unique->reordTime = 0;
#ifdef DD_STATS
  unique->nodesDropped = 0;
  unique->nodesFreed = 0;
#endif
  unique->peakLiveNodes = 0;
#ifdef DD_UNIQUE_PROFILE
  unique->uniqueLookUps = 0;
  unique->uniqueLinks = 0;
#endif
#ifdef DD_COUNT
  unique->recursiveCalls = 0;
  unique->swapSteps = 0;
#ifdef DD_STATS
  unique->nextSample = 250000;
#endif
#endif

  return (unique);

} /* end of cuddInitTable */

/**Function********************************************************************

  Synopsis    [Frees the resources associated to a unique table.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddInitTable]

******************************************************************************/
void cuddFreeTable(DdManager *unique) {
  DdNodePtr *next;
  DdNodePtr *memlist = unique->memoryList;
  int i;

  if (unique->univ != NULL)
    cuddZddFreeUniv(unique);
  while (memlist != NULL) {
    next = (DdNodePtr *)memlist[0]; /* link to next block */
    FREE(memlist);
    memlist = next;
  }
  unique->nextFree = NULL;
  unique->memoryList = NULL;

  for (i = 0; i < unique->size; i++) {
    FREE(unique->subtables[i].nodelist);
  }
  for (i = 0; i < unique->sizeZ; i++) {
    FREE(unique->subtableZ[i].nodelist);
  }
  FREE(unique->constants.nodelist);
  FREE(unique->subtables);
  FREE(unique->subtableZ);
  FREE(unique->acache);
  FREE(unique->perm);
  FREE(unique->permZ);
  FREE(unique->invperm);
  FREE(unique->invpermZ);
  FREE(unique->vars);
  if (unique->map != NULL)
    FREE(unique->map);
  FREE(unique->stack);
#ifndef DD_NO_DEATH_ROW
  FREE(unique->deathRow);
#endif
  if (unique->tree != NULL)
    Mtr_FreeTree(unique->tree);
  if (unique->treeZ != NULL)
    Mtr_FreeTree(unique->treeZ);
  if (unique->linear != NULL)
    FREE(unique->linear);
  while (unique->preGCHook != NULL)
    Cudd_RemoveHook(unique, unique->preGCHook->f, CUDD_PRE_GC_HOOK);
  while (unique->postGCHook != NULL)
    Cudd_RemoveHook(unique, unique->postGCHook->f, CUDD_POST_GC_HOOK);
  while (unique->preReorderingHook != NULL)
    Cudd_RemoveHook(unique, unique->preReorderingHook->f,
                    CUDD_PRE_REORDERING_HOOK);
  while (unique->postReorderingHook != NULL)
    Cudd_RemoveHook(unique, unique->postReorderingHook->f,
                    CUDD_POST_REORDERING_HOOK);
  FREE(unique);

} /* end of cuddFreeTable */

/**Function********************************************************************

  Synopsis    [Performs garbage collection on the unique tables.]

  Description [Performs garbage collection on the BDD and ZDD unique tables.
  If clearCache is 0, the cache is not cleared. This should only be
  specified if the cache has been cleared right before calling
  cuddGarbageCollect. (As in the case of dynamic reordering.)
  Returns the total number of deleted nodes.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddGarbageCollect(DdManager *unique, int clearCache) {
  DdHook *hook;
  DdCache *cache = unique->cache;
  DdNode *sentinel = &(unique->sentinel);
  DdNodePtr *nodelist;
  int i, j, deleted, totalDeleted, totalDeletedZ;
  DdCache *c;
  DdNode *node, *next;
  DdNodePtr *lastP;
  int slots;
  unsigned long localTime;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
  DdNodePtr tree;
#else
  DdNodePtr *memListTrav, *nxtNode;
  DdNode *downTrav, *sentry;
  int k;
#endif
#endif

#ifndef DD_NO_DEATH_ROW
  cuddClearDeathRow(unique);
#endif

  hook = unique->preGCHook;
  while (hook != NULL) {
    int res = (hook->f)(unique, "DD", NULL);
    if (res == 0)
      return (0);
    hook = hook->next;
  }

  if (unique->dead + unique->deadZ == 0) {
    hook = unique->postGCHook;
    while (hook != NULL) {
      int res = (hook->f)(unique, "DD", NULL);
      if (res == 0)
        return (0);
      hook = hook->next;
    }
    return (0);
  }

  /* If many nodes are being reclaimed, we want to resize the tables
  ** more aggressively, to reduce the frequency of garbage collection.
  */
  if (clearCache && unique->gcFrac == DD_GC_FRAC_LO &&
      unique->slots <= unique->looseUpTo && unique->stash != NULL) {
    unique->minDead = (unsigned)(DD_GC_FRAC_HI * (double)unique->slots);
#ifdef DD_VERBOSE
    (void)fprintf(unique->err, "GC fraction = %.2f\t", DD_GC_FRAC_HI);
    (void)fprintf(unique->err, "minDead = %d\n", unique->minDead);
#endif
    unique->gcFrac = DD_GC_FRAC_HI;
    return (0);
  }

  localTime = util_cpu_time();

  unique->garbageCollections++;
#ifdef DD_VERBOSE
  (void)fprintf(unique->err,
                "garbage collecting (%d dead BDD nodes out of %d, min %d)...",
                unique->dead, unique->keys, unique->minDead);
  (void)fprintf(unique->err,
                "                   (%d dead ZDD nodes out of %d)...",
                unique->deadZ, unique->keysZ);
#endif

  /* Remove references to garbage collected nodes from the cache. */
  if (clearCache) {
    slots = unique->cacheSlots;
    for (i = 0; i < slots; i++) {
      c = &cache[i];
      if (c->data != NULL) {
        if (cuddClean(c->f)->ref == 0 || cuddClean(c->g)->ref == 0 ||
            (((ptruint)c->f & 0x2) && Cudd_Regular(c->h)->ref == 0) ||
            (c->data != DD_NON_CONSTANT && Cudd_Regular(c->data)->ref == 0)) {
          c->data = NULL;
          unique->cachedeletions++;
        }
      }
    }
    cuddLocalCacheClearDead(unique);
  }

  /* Now return dead nodes to free list. Count them for sanity check. */
  totalDeleted = 0;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
  tree = NULL;
#endif
#endif

  for (i = 0; i < unique->size; i++) {
    if (unique->subtables[i].dead == 0)
      continue;
    nodelist = unique->subtables[i].nodelist;

    deleted = 0;
    slots = unique->subtables[i].slots;
    for (j = 0; j < slots; j++) {
      lastP = &(nodelist[j]);
      node = *lastP;
      while (node != sentinel) {
        next = node->next;
        if (node->ref == 0) {
          deleted++;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
          cuddOrderedInsert(&tree, node);
#endif
#else
          cuddDeallocNode(unique, node);
#endif
        } else {
          *lastP = node;
          lastP = &(node->next);
        }
        node = next;
      }
      *lastP = sentinel;
    }
    if ((unsigned)deleted != unique->subtables[i].dead) {
      ddReportRefMess(unique, i, "cuddGarbageCollect");
    }
    totalDeleted += deleted;
    unique->subtables[i].keys -= deleted;
    unique->subtables[i].dead = 0;
  }
  if (unique->constants.dead != 0) {
    nodelist = unique->constants.nodelist;
    deleted = 0;
    slots = unique->constants.slots;
    for (j = 0; j < slots; j++) {
      lastP = &(nodelist[j]);
      node = *lastP;
      while (node != NULL) {
        next = node->next;
        if (node->ref == 0) {
          deleted++;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
          cuddOrderedInsert(&tree, node);
#endif
#else
          cuddDeallocNode(unique, node);
#endif
        } else {
          *lastP = node;
          lastP = &(node->next);
        }
        node = next;
      }
      *lastP = NULL;
    }
    if ((unsigned)deleted != unique->constants.dead) {
      ddReportRefMess(unique, CUDD_CONST_INDEX, "cuddGarbageCollect");
    }
    totalDeleted += deleted;
    unique->constants.keys -= deleted;
    unique->constants.dead = 0;
  }
  if ((unsigned)totalDeleted != unique->dead) {
    ddReportRefMess(unique, -1, "cuddGarbageCollect");
  }
  unique->keys -= totalDeleted;
  unique->dead = 0;
#ifdef DD_STATS
  unique->nodesFreed += (double)totalDeleted;
#endif

  totalDeletedZ = 0;

  for (i = 0; i < unique->sizeZ; i++) {
    if (unique->subtableZ[i].dead == 0)
      continue;
    nodelist = unique->subtableZ[i].nodelist;

    deleted = 0;
    slots = unique->subtableZ[i].slots;
    for (j = 0; j < slots; j++) {
      lastP = &(nodelist[j]);
      node = *lastP;
      while (node != NULL) {
        next = node->next;
        if (node->ref == 0) {
          deleted++;
#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
          cuddOrderedInsert(&tree, node);
#endif
#else
          cuddDeallocNode(unique, node);
#endif
        } else {
          *lastP = node;
          lastP = &(node->next);
        }
        node = next;
      }
      *lastP = NULL;
    }
    if ((unsigned)deleted != unique->subtableZ[i].dead) {
      ddReportRefMess(unique, i, "cuddGarbageCollect");
    }
    totalDeletedZ += deleted;
    unique->subtableZ[i].keys -= deleted;
    unique->subtableZ[i].dead = 0;
  }

  /* No need to examine the constant table for ZDDs.
  ** If we did we should be careful not to count whatever dead
  ** nodes we found there among the dead ZDD nodes. */
  if ((unsigned)totalDeletedZ != unique->deadZ) {
    ddReportRefMess(unique, -1, "cuddGarbageCollect");
  }
  unique->keysZ -= totalDeletedZ;
  unique->deadZ = 0;
#ifdef DD_STATS
  unique->nodesFreed += (double)totalDeletedZ;
#endif

#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
  unique->nextFree = cuddOrderedThread(tree, unique->nextFree);
#else
  memListTrav = unique->memoryList;
  sentry = NULL;
  while (memListTrav != NULL) {
    ptruint offset;
    nxtNode = (DdNodePtr *)memListTrav[0];
    offset = (ptruint)memListTrav & (sizeof(DdNode) - 1);
    memListTrav += (sizeof(DdNode) - offset) / sizeof(DdNodePtr);
    downTrav = (DdNode *)memListTrav;
    k = 0;
    do {
      if (downTrav[k].ref == 0) {
        if (sentry == NULL) {
          unique->nextFree = sentry = &downTrav[k];
        } else {
          /* First hook sentry->next to the dead node and then
          ** reassign sentry to the dead node. */
          sentry = (sentry->next = &downTrav[k]);
        }
      }
    } while (++k < DD_MEM_CHUNK);
    memListTrav = nxtNode;
  }
  sentry->next = NULL;
#endif
#endif

  unique->GCTime += util_cpu_time() - localTime;

  hook = unique->postGCHook;
  while (hook != NULL) {
    int res = (hook->f)(unique, "DD", NULL);
    if (res == 0)
      return (0);
    hook = hook->next;
  }

#ifdef DD_VERBOSE
  (void)fprintf(unique->err, " done\n");
#endif

  return (totalDeleted + totalDeletedZ);

} /* end of cuddGarbageCollect */

/**Function********************************************************************

  Synopsis [Wrapper for cuddUniqueInterZdd.]

  Description [Wrapper for cuddUniqueInterZdd, which applies the ZDD
  reduction rule. Returns a pointer to the result node under normal
  conditions; NULL if reordering occurred or memory was exhausted.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInterZdd]

******************************************************************************/
DdNode *cuddZddGetNode(DdManager *zdd, int id, DdNode *T, DdNode *E) {
  DdNode *node;

  if (T == DD_ZERO(zdd))
    return (E);
  node = cuddUniqueInterZdd(zdd, id, T, E);
  return (node);

} /* end of cuddZddGetNode */

/**Function********************************************************************

  Synopsis [Wrapper for cuddUniqueInterZdd that is independent of variable
  ordering.]

  Description [Wrapper for cuddUniqueInterZdd that is independent of
  variable ordering (IVO). This function does not require parameter
  index to precede the indices of the top nodes of g and h in the
  variable order.  Returns a pointer to the result node under normal
  conditions; NULL if reordering occurred or memory was exhausted.]

  SideEffects [None]

  SeeAlso     [cuddZddGetNode cuddZddIsop]

******************************************************************************/
DdNode *cuddZddGetNodeIVO(DdManager *dd, int index, DdNode *g, DdNode *h) {
  DdNode *f, *r, *t;
  DdNode *zdd_one = DD_ONE(dd);
  DdNode *zdd_zero = DD_ZERO(dd);

  f = cuddUniqueInterZdd(dd, index, zdd_one, zdd_zero);
  if (f == NULL) {
    return (NULL);
  }
  cuddRef(f);
  t = cuddZddProduct(dd, f, g);
  if (t == NULL) {
    Cudd_RecursiveDerefZdd(dd, f);
    return (NULL);
  }
  cuddRef(t);
  Cudd_RecursiveDerefZdd(dd, f);
  r = cuddZddUnion(dd, t, h);
  if (r == NULL) {
    Cudd_RecursiveDerefZdd(dd, t);
    return (NULL);
  }
  cuddRef(r);
  Cudd_RecursiveDerefZdd(dd, t);

  cuddDeref(r);
  return (r);

} /* end of cuddZddGetNodeIVO */

/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of an internal node.]

  Description [Checks the unique table for the existence of an internal
  node. If it does not exist, it creates a new one.  Does not
  modify the reference count of whatever is returned.  A newly created
  internal node comes back with a reference count 0.  For a newly
  created node, increments the reference counts of what T and E point
  to.  Returns a pointer to the new node if successful; NULL if memory
  is exhausted or if reordering took place.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInterZdd]

******************************************************************************/
DdNode *cuddUniqueInter(DdManager *unique, int index, DdNode *T, DdNode *E) {
  int pos;
  unsigned int level;
  int retval;
  DdNodePtr *nodelist;
  DdNode *looking;
  DdNodePtr *previousP;
  DdSubtable *subtable;
  int gcNumber;

#ifdef DD_UNIQUE_PROFILE
  unique->uniqueLookUps++;
#endif

  if ((0x1ffffUL & (unsigned long)unique->cacheMisses) == 0) {
    if (util_cpu_time() - unique->startTime > unique->timeLimit) {
      unique->errorCode = CUDD_TIMEOUT_EXPIRED;
      return (NULL);
    }
  }
  if (index >= unique->size) {
    int amount = ddMax(DD_DEFAULT_RESIZE, unique->size / 20);
    if (!ddResizeTable(unique, index, amount))
      return (NULL);
  }

  level = unique->perm[index];
  subtable = &(unique->subtables[level]);

#ifdef DD_DEBUG
  assert(level < (unsigned)cuddI(unique, T->index));
  assert(level < (unsigned)cuddI(unique, Cudd_Regular(E)->index));
#endif

  pos = ddHash(T, E, subtable->shift);
  nodelist = subtable->nodelist;
  previousP = &(nodelist[pos]);
  looking = *previousP;

  while (T < cuddT(looking)) {
    previousP = &(looking->next);
    looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLinks++;
#endif
  }
  while (T == cuddT(looking) && E < cuddE(looking)) {
    previousP = &(looking->next);
    looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLinks++;
#endif
  }
  if (T == cuddT(looking) && E == cuddE(looking)) {
    if (looking->ref == 0) {
      cuddReclaim(unique, looking);
    }
    return (looking);
  }

  /* countDead is 0 if deads should be counted and ~0 if they should not. */
  if (unique->autoDyn &&
      unique->keys - (unique->dead & unique->countDead) >= unique->nextDyn &&
      unique->maxReorderings > 0) {
    unsigned long cpuTime;
#ifdef DD_DEBUG
    retval = Cudd_DebugCheck(unique);
    if (retval != 0)
      return (NULL);
    retval = Cudd_CheckKeys(unique);
    if (retval != 0)
      return (NULL);
#endif
    retval =
        Cudd_ReduceHeap(unique, unique->autoMethod, 10); /* 10 = whatever */
    unique->maxReorderings--;
    if (retval == 0) {
      unique->reordered = 2;
    } else if ((cpuTime = util_cpu_time()) - unique->startTime >
               unique->timeLimit) {
      unique->errorCode = CUDD_TIMEOUT_EXPIRED;
      unique->reordered = 0;
    } else if (unique->timeLimit - (cpuTime - unique->startTime) <
               unique->reordTime) {
      unique->autoDyn = 0;
    }
#ifdef DD_DEBUG
    retval = Cudd_DebugCheck(unique);
    if (retval != 0)
      unique->reordered = 2;
    retval = Cudd_CheckKeys(unique);
    if (retval != 0)
      unique->reordered = 2;
#endif
    return (NULL);
  }

  if (subtable->keys > subtable->maxKeys) {
    if (unique->gcEnabled &&
        ((unique->dead > unique->minDead) ||
         ((unique->dead > unique->minDead / 2) &&
          (subtable->dead > subtable->keys * 0.95)))) { /* too many dead */
      if (util_cpu_time() - unique->startTime > unique->timeLimit) {
        unique->errorCode = CUDD_TIMEOUT_EXPIRED;
        return (NULL);
      }
      (void)cuddGarbageCollect(unique, 1);
    } else {
      cuddRehash(unique, (int)level);
    }
    /* Update pointer to insertion point. In the case of rehashing,
    ** the slot may have changed. In the case of garbage collection,
    ** the predecessor may have been dead. */
    pos = ddHash(T, E, subtable->shift);
    nodelist = subtable->nodelist;
    previousP = &(nodelist[pos]);
    looking = *previousP;

    while (T < cuddT(looking)) {
      previousP = &(looking->next);
      looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
      unique->uniqueLinks++;
#endif
    }
    while (T == cuddT(looking) && E < cuddE(looking)) {
      previousP = &(looking->next);
      looking = *previousP;
#ifdef DD_UNIQUE_PROFILE
      unique->uniqueLinks++;
#endif
    }
  }

  gcNumber = unique->garbageCollections;
  looking = cuddAllocNode(unique);
  if (looking == NULL) {
    return (NULL);
  }
  unique->keys++;
  subtable->keys++;

  if (gcNumber != unique->garbageCollections) {
    DdNode *looking2;
    pos = ddHash(T, E, subtable->shift);
    nodelist = subtable->nodelist;
    previousP = &(nodelist[pos]);
    looking2 = *previousP;

    while (T < cuddT(looking2)) {
      previousP = &(looking2->next);
      looking2 = *previousP;
#ifdef DD_UNIQUE_PROFILE
      unique->uniqueLinks++;
#endif
    }
    while (T == cuddT(looking2) && E < cuddE(looking2)) {
      previousP = &(looking2->next);
      looking2 = *previousP;
#ifdef DD_UNIQUE_PROFILE
      unique->uniqueLinks++;
#endif
    }
  }
  looking->index = index;
  cuddT(looking) = T;
  cuddE(looking) = E;
  looking->next = *previousP;
  *previousP = looking;
  cuddSatInc(T->ref); /* we know T is a regular pointer */
  cuddRef(E);

#ifdef DD_DEBUG
  cuddCheckCollisionOrdering(unique, level, pos);
#endif

  return (looking);

} /* end of cuddUniqueInter */

/**Function********************************************************************

  Synopsis [Wrapper for cuddUniqueInter that is independent of variable
  ordering.]

  Description [Wrapper for cuddUniqueInter that is independent of
  variable ordering (IVO). This function does not require parameter
  index to precede the indices of the top nodes of T and E in the
  variable order.  Returns a pointer to the result node under normal
  conditions; NULL if reordering occurred or memory was exhausted.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInter Cudd_MakeBddFromZddCover]

******************************************************************************/
DdNode *cuddUniqueInterIVO(DdManager *unique, int index, DdNode *T, DdNode *E) {
  DdNode *result;
  DdNode *v;

  v = cuddUniqueInter(unique, index, DD_ONE(unique), Cudd_Not(DD_ONE(unique)));
  if (v == NULL)
    return (NULL);
  /* Since v is a projection function, we can skip the call to cuddRef. */
  result = cuddBddIteRecur(unique, v, T, E);
  return (result);

} /* end of cuddUniqueInterIVO */

/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of an internal
  ZDD node.]

  Description [Checks the unique table for the existence of an internal
  ZDD node. If it does not exist, it creates a new one.  Does not
  modify the reference count of whatever is returned.  A newly created
  internal node comes back with a reference count 0.  For a newly
  created node, increments the reference counts of what T and E point
  to.  Returns a pointer to the new node if successful; NULL if memory
  is exhausted or if reordering took place.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInter]

******************************************************************************/
DdNode *cuddUniqueInterZdd(DdManager *unique, int index, DdNode *T, DdNode *E) {
  int pos;
  unsigned int level;
  int retval;
  DdNodePtr *nodelist;
  DdNode *looking;
  DdSubtable *subtable;

#ifdef DD_UNIQUE_PROFILE
  unique->uniqueLookUps++;
#endif

  if (index >= unique->sizeZ) {
    if (!cuddResizeTableZdd(unique, index))
      return (NULL);
  }

  level = unique->permZ[index];
  subtable = &(unique->subtableZ[level]);

#ifdef DD_DEBUG
  assert(level < (unsigned)cuddIZ(unique, T->index));
  assert(level < (unsigned)cuddIZ(unique, Cudd_Regular(E)->index));
#endif

  if (subtable->keys > subtable->maxKeys) {
    if (unique->gcEnabled &&
        ((unique->deadZ > unique->minDead) ||
         (10 * subtable->dead > 9 * subtable->keys))) { /* too many dead */
      (void)cuddGarbageCollect(unique, 1);
    } else {
      ddRehashZdd(unique, (int)level);
    }
  }

  pos = ddHash(T, E, subtable->shift);
  nodelist = subtable->nodelist;
  looking = nodelist[pos];

  while (looking != NULL) {
    if (cuddT(looking) == T && cuddE(looking) == E) {
      if (looking->ref == 0) {
        cuddReclaimZdd(unique, looking);
      }
      return (looking);
    }
    looking = looking->next;
#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLinks++;
#endif
  }

  /* countDead is 0 if deads should be counted and ~0 if they should not. */
  if (unique->autoDynZ &&
      unique->keysZ - (unique->deadZ & unique->countDead) >= unique->nextDyn) {
#ifdef DD_DEBUG
    retval = Cudd_DebugCheck(unique);
    if (retval != 0)
      return (NULL);
    retval = Cudd_CheckKeys(unique);
    if (retval != 0)
      return (NULL);
#endif
    retval =
        Cudd_zddReduceHeap(unique, unique->autoMethodZ, 10); /* 10 = whatever */
    if (retval == 0)
      unique->reordered = 2;
#ifdef DD_DEBUG
    retval = Cudd_DebugCheck(unique);
    if (retval != 0)
      unique->reordered = 2;
    retval = Cudd_CheckKeys(unique);
    if (retval != 0)
      unique->reordered = 2;
#endif
    return (NULL);
  }

  unique->keysZ++;
  subtable->keys++;

  looking = cuddAllocNode(unique);
  if (looking == NULL)
    return (NULL);
  looking->index = index;
  cuddT(looking) = T;
  cuddE(looking) = E;
  looking->next = nodelist[pos];
  nodelist[pos] = looking;
  cuddRef(T);
  cuddRef(E);

  return (looking);

} /* end of cuddUniqueInterZdd */

/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of a constant node.]

  Description [Checks the unique table for the existence of a constant node.
  If it does not exist, it creates a new one.  Does not
  modify the reference count of whatever is returned.  A newly created
  internal node comes back with a reference count 0.  Returns a
  pointer to the new node.]

  SideEffects [None]

******************************************************************************/
DdNode *cuddUniqueConst(DdManager *unique, CUDD_VALUE_TYPE value) {
  int pos;
  DdNodePtr *nodelist;
  DdNode *looking;
  hack split;

#ifdef DD_UNIQUE_PROFILE
  unique->uniqueLookUps++;
#endif

  if (unique->constants.keys > unique->constants.maxKeys) {
    if (unique->gcEnabled &&
        ((unique->dead > unique->minDead) ||
         (10 * unique->constants.dead >
          9 * unique->constants.keys))) { /* too many dead */
      (void)cuddGarbageCollect(unique, 1);
    } else {
      cuddRehash(unique, CUDD_CONST_INDEX);
    }
  }

  cuddAdjust(value); /* for the case of crippled infinities */

  if (ddAbs(value) < unique->epsilon) {
    value = 0.0;
  }
  split.value = value;

  pos = ddHash(split.bits[0], split.bits[1], unique->constants.shift);
  nodelist = unique->constants.nodelist;
  looking = nodelist[pos];

  /* Here we compare values both for equality and for difference less
   * than epsilon. The first comparison is required when values are
   * infinite, since Infinity - Infinity is NaN and NaN < X is 0 for
   * every X.
   */
  while (looking != NULL) {
    if (looking->type.value == value ||
        ddEqualVal(looking->type.value, value, unique->epsilon)) {
      if (looking->ref == 0) {
        cuddReclaim(unique, looking);
      }
      return (looking);
    }
    looking = looking->next;
#ifdef DD_UNIQUE_PROFILE
    unique->uniqueLinks++;
#endif
  }

  unique->keys++;
  unique->constants.keys++;

  looking = cuddAllocNode(unique);
  if (looking == NULL)
    return (NULL);
  looking->index = CUDD_CONST_INDEX;
  looking->type.value = value;
  looking->next = nodelist[pos];
  nodelist[pos] = looking;

  return (looking);

} /* end of cuddUniqueConst */

/**Function********************************************************************

  Synopsis    [Rehashes a unique subtable.]

  Description [Doubles the size of a unique subtable and rehashes its
  contents.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddRehash(DdManager *unique, int i) {
  unsigned int slots, oldslots;
  int shift, oldshift;
  int j, pos;
  DdNodePtr *nodelist, *oldnodelist;
  DdNode *node, *next;
  DdNode *sentinel = &(unique->sentinel);
  hack split;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  if (unique->gcFrac == DD_GC_FRAC_HI && unique->slots > unique->looseUpTo) {
    unique->gcFrac = DD_GC_FRAC_LO;
    unique->minDead = (unsigned)(DD_GC_FRAC_LO * (double)unique->slots);
#ifdef DD_VERBOSE
    (void)fprintf(unique->err, "GC fraction = %.2f\t", DD_GC_FRAC_LO);
    (void)fprintf(unique->err, "minDead = %d\n", unique->minDead);
#endif
  }

  if (unique->gcFrac != DD_GC_FRAC_MIN && unique->memused > unique->maxmem) {
    unique->gcFrac = DD_GC_FRAC_MIN;
    unique->minDead = (unsigned)(DD_GC_FRAC_MIN * (double)unique->slots);
#ifdef DD_VERBOSE
    (void)fprintf(unique->err, "GC fraction = %.2f\t", DD_GC_FRAC_MIN);
    (void)fprintf(unique->err, "minDead = %d\n", unique->minDead);
#endif
    cuddShrinkDeathRow(unique);
    if (cuddGarbageCollect(unique, 1) > 0)
      return;
  }

  if (i != CUDD_CONST_INDEX) {
    oldslots = unique->subtables[i].slots;
    oldshift = unique->subtables[i].shift;
    oldnodelist = unique->subtables[i].nodelist;

    /* Compute the new size of the subtable. */
    slots = oldslots << 1;
    shift = oldshift - 1;

    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    nodelist = ALLOC(DdNodePtr, slots);
    MMoutOfMemory = saveHandler;
    if (nodelist == NULL) {
      (void)fprintf(unique->err,
                    "Unable to resize subtable %d for lack of memory\n", i);
      /* Prevent frequent resizing attempts. */
      (void)cuddGarbageCollect(unique, 1);
      if (unique->stash != NULL) {
        FREE(unique->stash);
        unique->stash = NULL;
        /* Inhibit resizing of tables. */
        cuddSlowTableGrowth(unique);
      }
      return;
    }
    unique->subtables[i].nodelist = nodelist;
    unique->subtables[i].slots = slots;
    unique->subtables[i].shift = shift;
    unique->subtables[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;

    /* Move the nodes from the old table to the new table.
    ** This code depends on the type of hash function.
    ** It assumes that the effect of doubling the size of the table
    ** is to retain one more bit of the 32-bit hash value.
    ** The additional bit is the LSB. */
    for (j = 0; (unsigned)j < oldslots; j++) {
      DdNodePtr *evenP, *oddP;
      node = oldnodelist[j];
      evenP = &(nodelist[j << 1]);
      oddP = &(nodelist[(j << 1) + 1]);
      while (node != sentinel) {
        next = node->next;
        pos = ddHash(cuddT(node), cuddE(node), shift);
        if (pos & 1) {
          *oddP = node;
          oddP = &(node->next);
        } else {
          *evenP = node;
          evenP = &(node->next);
        }
        node = next;
      }
      *evenP = *oddP = sentinel;
    }
    FREE(oldnodelist);

#ifdef DD_VERBOSE
    (void)fprintf(unique->err,
                  "rehashing layer %d: keys %d dead %d new size %d\n", i,
                  unique->subtables[i].keys, unique->subtables[i].dead, slots);
#endif
  } else {
    oldslots = unique->constants.slots;
    oldshift = unique->constants.shift;
    oldnodelist = unique->constants.nodelist;

    /* The constant subtable is never subjected to reordering.
    ** Therefore, when it is resized, it is because it has just
    ** reached the maximum load. We can safely just double the size,
    ** with no need for the loop we use for the other tables.
    */
    slots = oldslots << 1;
    shift = oldshift - 1;
    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    nodelist = ALLOC(DdNodePtr, slots);
    MMoutOfMemory = saveHandler;
    if (nodelist == NULL) {
      (void)fprintf(unique->err,
                    "Unable to resize constant subtable for lack of memory\n");
      (void)cuddGarbageCollect(unique, 1);
      for (j = 0; j < unique->size; j++) {
        unique->subtables[j].maxKeys <<= 1;
      }
      unique->constants.maxKeys <<= 1;
      return;
    }
    unique->constants.slots = slots;
    unique->constants.shift = shift;
    unique->constants.maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
    unique->constants.nodelist = nodelist;
    for (j = 0; (unsigned)j < slots; j++) {
      nodelist[j] = NULL;
    }
    for (j = 0; (unsigned)j < oldslots; j++) {
      node = oldnodelist[j];
      while (node != NULL) {
        next = node->next;
        split.value = cuddV(node);
        pos = ddHash(split.bits[0], split.bits[1], shift);
        node->next = nodelist[pos];
        nodelist[pos] = node;
        node = next;
      }
    }
    FREE(oldnodelist);

#ifdef DD_VERBOSE
    (void)fprintf(unique->err,
                  "rehashing constants: keys %d dead %d new size %d\n",
                  unique->constants.keys, unique->constants.dead, slots);
#endif
  }

  /* Update global data */

  unique->memused += (slots - oldslots) * sizeof(DdNodePtr);
  unique->slots += (slots - oldslots);
  ddFixLimits(unique);

} /* end of cuddRehash */

/**Function********************************************************************

  Synopsis [Increases the number of ZDD subtables in a unique table so
  that it meets or exceeds index.]

  Description [Increases the number of ZDD subtables in a unique table so
  that it meets or exceeds index.  When new ZDD variables are created, it
  is possible to preserve the functions unchanged, or it is possible to
  preserve the covers unchanged, but not both. cuddResizeTableZdd preserves
  the covers.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [ddResizeTable]

******************************************************************************/
int cuddResizeTableZdd(DdManager *unique, int index) {
  DdSubtable *newsubtables;
  DdNodePtr *newnodelist;
  int oldsize, newsize;
  int i, j, reorderSave;
  unsigned int numSlots = unique->initSlots;
  int *newperm, *newinvperm;

  oldsize = unique->sizeZ;
  /* Easy case: there is still room in the current table. */
  if (index < unique->maxSizeZ) {
    for (i = oldsize; i <= index; i++) {
      unique->subtableZ[i].slots = numSlots;
      unique->subtableZ[i].shift =
          sizeof(int) * 8 - cuddComputeFloorLog2(numSlots);
      unique->subtableZ[i].keys = 0;
      unique->subtableZ[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
      unique->subtableZ[i].dead = 0;
      unique->permZ[i] = i;
      unique->invpermZ[i] = i;
      newnodelist = unique->subtableZ[i].nodelist = ALLOC(DdNodePtr, numSlots);
      if (newnodelist == NULL) {
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      for (j = 0; (unsigned)j < numSlots; j++) {
        newnodelist[j] = NULL;
      }
    }
  } else {
    /* The current table is too small: we need to allocate a new,
    ** larger one; move all old subtables, and initialize the new
    ** subtables up to index included.
    */
    newsize = index + DD_DEFAULT_RESIZE;
#ifdef DD_VERBOSE
    (void)fprintf(unique->err, "Increasing the ZDD table size from %d to %d\n",
                  unique->maxSizeZ, newsize);
#endif
    newsubtables = ALLOC(DdSubtable, newsize);
    if (newsubtables == NULL) {
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    newperm = ALLOC(int, newsize);
    if (newperm == NULL) {
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    newinvperm = ALLOC(int, newsize);
    if (newinvperm == NULL) {
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    unique->memused +=
        (newsize - unique->maxSizeZ) * ((numSlots + 1) * sizeof(DdNode *) +
                                        2 * sizeof(int) + sizeof(DdSubtable));
    if (newsize > unique->maxSize) {
      FREE(unique->stack);
      unique->stack = ALLOC(DdNodePtr, newsize + 1);
      if (unique->stack == NULL) {
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      unique->stack[0] = NULL; /* to suppress harmless UMR */
      unique->memused += (newsize - ddMax(unique->maxSize, unique->maxSizeZ)) *
                         sizeof(DdNode *);
    }
    for (i = 0; i < oldsize; i++) {
      newsubtables[i].slots = unique->subtableZ[i].slots;
      newsubtables[i].shift = unique->subtableZ[i].shift;
      newsubtables[i].keys = unique->subtableZ[i].keys;
      newsubtables[i].maxKeys = unique->subtableZ[i].maxKeys;
      newsubtables[i].dead = unique->subtableZ[i].dead;
      newsubtables[i].nodelist = unique->subtableZ[i].nodelist;
      newperm[i] = unique->permZ[i];
      newinvperm[i] = unique->invpermZ[i];
    }
    for (i = oldsize; i <= index; i++) {
      newsubtables[i].slots = numSlots;
      newsubtables[i].shift = sizeof(int) * 8 - cuddComputeFloorLog2(numSlots);
      newsubtables[i].keys = 0;
      newsubtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
      newsubtables[i].dead = 0;
      newperm[i] = i;
      newinvperm[i] = i;
      newnodelist = newsubtables[i].nodelist = ALLOC(DdNodePtr, numSlots);
      if (newnodelist == NULL) {
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      for (j = 0; (unsigned)j < numSlots; j++) {
        newnodelist[j] = NULL;
      }
    }
    FREE(unique->subtableZ);
    unique->subtableZ = newsubtables;
    unique->maxSizeZ = newsize;
    FREE(unique->permZ);
    unique->permZ = newperm;
    FREE(unique->invpermZ);
    unique->invpermZ = newinvperm;
  }
  unique->slots += (index + 1 - unique->sizeZ) * numSlots;
  ddFixLimits(unique);
  unique->sizeZ = index + 1;

  /* Now that the table is in a coherent state, update the ZDD
  ** universe. We need to temporarily disable reordering,
  ** because we cannot reorder without universe in place.
  */

  reorderSave = unique->autoDynZ;
  unique->autoDynZ = 0;
  cuddZddFreeUniv(unique);
  if (!cuddZddInitUniv(unique)) {
    unique->autoDynZ = reorderSave;
    return (0);
  }
  unique->autoDynZ = reorderSave;

  return (1);

} /* end of cuddResizeTableZdd */

/**Function********************************************************************

  Synopsis    [Adjusts parameters of a table to slow down its growth.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void cuddSlowTableGrowth(DdManager *unique) {
  int i;

  unique->maxCacheHard = unique->cacheSlots - 1;
  unique->cacheSlack = -(int)(unique->cacheSlots + 1);
  for (i = 0; i < unique->size; i++) {
    unique->subtables[i].maxKeys <<= 2;
  }
  unique->gcFrac = DD_GC_FRAC_MIN;
  unique->minDead = (unsigned)(DD_GC_FRAC_MIN * (double)unique->slots);
  cuddShrinkDeathRow(unique);
  (void)fprintf(unique->err, "Slowing down table growth: ");
  (void)fprintf(unique->err, "GC fraction = %.2f\t", unique->gcFrac);
  (void)fprintf(unique->err, "minDead = %u\n", unique->minDead);

} /* end of cuddSlowTableGrowth */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Rehashes a ZDD unique subtable.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddRehash]

******************************************************************************/
static void ddRehashZdd(DdManager *unique, int i) {
  unsigned int slots, oldslots;
  int shift, oldshift;
  int j, pos;
  DdNodePtr *nodelist, *oldnodelist;
  DdNode *node, *next;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

  if (unique->slots > unique->looseUpTo) {
    unique->minDead = (unsigned)(DD_GC_FRAC_LO * (double)unique->slots);
#ifdef DD_VERBOSE
    if (unique->gcFrac == DD_GC_FRAC_HI) {
      (void)fprintf(unique->err, "GC fraction = %.2f\t", DD_GC_FRAC_LO);
      (void)fprintf(unique->err, "minDead = %d\n", unique->minDead);
    }
#endif
    unique->gcFrac = DD_GC_FRAC_LO;
  }

  assert(i != CUDD_MAXINDEX);
  oldslots = unique->subtableZ[i].slots;
  oldshift = unique->subtableZ[i].shift;
  oldnodelist = unique->subtableZ[i].nodelist;

  /* Compute the new size of the subtable. Normally, we just
  ** double.  However, after reordering, a table may be severely
  ** overloaded. Therefore, we iterate. */
  slots = oldslots;
  shift = oldshift;
  do {
    slots <<= 1;
    shift--;
  } while (slots * DD_MAX_SUBTABLE_DENSITY < unique->subtableZ[i].keys);

  saveHandler = MMoutOfMemory;
  MMoutOfMemory = Cudd_OutOfMem;
  nodelist = ALLOC(DdNodePtr, slots);
  MMoutOfMemory = saveHandler;
  if (nodelist == NULL) {
    (void)fprintf(unique->err,
                  "Unable to resize ZDD subtable %d for lack of memory.\n", i);
    (void)cuddGarbageCollect(unique, 1);
    for (j = 0; j < unique->sizeZ; j++) {
      unique->subtableZ[j].maxKeys <<= 1;
    }
    return;
  }
  unique->subtableZ[i].nodelist = nodelist;
  unique->subtableZ[i].slots = slots;
  unique->subtableZ[i].shift = shift;
  unique->subtableZ[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
  for (j = 0; (unsigned)j < slots; j++) {
    nodelist[j] = NULL;
  }
  for (j = 0; (unsigned)j < oldslots; j++) {
    node = oldnodelist[j];
    while (node != NULL) {
      next = node->next;
      pos = ddHash(cuddT(node), cuddE(node), shift);
      node->next = nodelist[pos];
      nodelist[pos] = node;
      node = next;
    }
  }
  FREE(oldnodelist);

#ifdef DD_VERBOSE
  (void)fprintf(unique->err,
                "rehashing layer %d: keys %d dead %d new size %d\n", i,
                unique->subtableZ[i].keys, unique->subtableZ[i].dead, slots);
#endif

  /* Update global data. */
  unique->memused += (slots - oldslots) * sizeof(DdNode *);
  unique->slots += (slots - oldslots);
  ddFixLimits(unique);

} /* end of ddRehashZdd */

/**Function********************************************************************

  Synopsis [Increases the number of subtables in a unique table so
  that it meets or exceeds index.]

  Description [Increases the number of subtables in a unique table so
  that it meets or exceeds index.  The parameter amount determines how
  much spare space is allocated to prevent too frequent resizing.  If
  index is negative, the table is resized, but no new variables are
  created.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_Reserve cuddResizeTableZdd]

******************************************************************************/
static int ddResizeTable(DdManager *unique, int index, int amount) {
  DdSubtable *newsubtables;
  DdNodePtr *newnodelist;
  DdNodePtr *newvars;
  DdNode *sentinel = &(unique->sentinel);
  int oldsize, newsize;
  int i, j, reorderSave;
  int numSlots = unique->initSlots;
  int *newperm, *newinvperm, *newmap;
  DdNode *one, *zero;

  oldsize = unique->size;
  /* Easy case: there is still room in the current table. */
  if (index >= 0 && index < unique->maxSize) {
    for (i = oldsize; i <= index; i++) {
      unique->subtables[i].slots = numSlots;
      unique->subtables[i].shift =
          sizeof(int) * 8 - cuddComputeFloorLog2(numSlots);
      unique->subtables[i].keys = 0;
      unique->subtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
      unique->subtables[i].dead = 0;
      unique->subtables[i].bindVar = 0;
      unique->subtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
      unique->subtables[i].pairIndex = 0;
      unique->subtables[i].varHandled = 0;
      unique->subtables[i].varToBeGrouped = CUDD_LAZY_NONE;

      unique->perm[i] = i;
      unique->invperm[i] = i;
      newnodelist = unique->subtables[i].nodelist = ALLOC(DdNodePtr, numSlots);
      if (newnodelist == NULL) {
        for (j = oldsize; j < i; j++) {
          FREE(unique->subtables[j].nodelist);
        }
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      for (j = 0; j < numSlots; j++) {
        newnodelist[j] = sentinel;
      }
    }
    if (unique->map != NULL) {
      for (i = oldsize; i <= index; i++) {
        unique->map[i] = i;
      }
    }
  } else {
    /* The current table is too small: we need to allocate a new,
    ** larger one; move all old subtables, and initialize the new
    ** subtables up to index included.
    */
    newsize = (index < 0) ? amount : index + amount;
#ifdef DD_VERBOSE
    (void)fprintf(unique->err, "Increasing the table size from %d to %d\n",
                  unique->maxSize, newsize);
#endif
    newsubtables = ALLOC(DdSubtable, newsize);
    if (newsubtables == NULL) {
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    newvars = ALLOC(DdNodePtr, newsize);
    if (newvars == NULL) {
      FREE(newsubtables);
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    newperm = ALLOC(int, newsize);
    if (newperm == NULL) {
      FREE(newsubtables);
      FREE(newvars);
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    newinvperm = ALLOC(int, newsize);
    if (newinvperm == NULL) {
      FREE(newsubtables);
      FREE(newvars);
      FREE(newperm);
      unique->errorCode = CUDD_MEMORY_OUT;
      return (0);
    }
    if (unique->map != NULL) {
      newmap = ALLOC(int, newsize);
      if (newmap == NULL) {
        FREE(newsubtables);
        FREE(newvars);
        FREE(newperm);
        FREE(newinvperm);
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      unique->memused += (newsize - unique->maxSize) * sizeof(int);
    }
    unique->memused +=
        (newsize - unique->maxSize) * ((numSlots + 1) * sizeof(DdNode *) +
                                       2 * sizeof(int) + sizeof(DdSubtable));
    if (newsize > unique->maxSizeZ) {
      FREE(unique->stack);
      unique->stack = ALLOC(DdNodePtr, newsize + 1);
      if (unique->stack == NULL) {
        FREE(newsubtables);
        FREE(newvars);
        FREE(newperm);
        FREE(newinvperm);
        if (unique->map != NULL) {
          FREE(newmap);
        }
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      unique->stack[0] = NULL; /* to suppress harmless UMR */
      unique->memused += (newsize - ddMax(unique->maxSize, unique->maxSizeZ)) *
                         sizeof(DdNode *);
    }
    for (i = 0; i < oldsize; i++) {
      newsubtables[i].slots = unique->subtables[i].slots;
      newsubtables[i].shift = unique->subtables[i].shift;
      newsubtables[i].keys = unique->subtables[i].keys;
      newsubtables[i].maxKeys = unique->subtables[i].maxKeys;
      newsubtables[i].dead = unique->subtables[i].dead;
      newsubtables[i].nodelist = unique->subtables[i].nodelist;
      newsubtables[i].bindVar = unique->subtables[i].bindVar;
      newsubtables[i].varType = unique->subtables[i].varType;
      newsubtables[i].pairIndex = unique->subtables[i].pairIndex;
      newsubtables[i].varHandled = unique->subtables[i].varHandled;
      newsubtables[i].varToBeGrouped = unique->subtables[i].varToBeGrouped;

      newvars[i] = unique->vars[i];
      newperm[i] = unique->perm[i];
      newinvperm[i] = unique->invperm[i];
    }
    for (i = oldsize; i <= index; i++) {
      newsubtables[i].slots = numSlots;
      newsubtables[i].shift = sizeof(int) * 8 - cuddComputeFloorLog2(numSlots);
      newsubtables[i].keys = 0;
      newsubtables[i].maxKeys = numSlots * DD_MAX_SUBTABLE_DENSITY;
      newsubtables[i].dead = 0;
      newsubtables[i].bindVar = 0;
      newsubtables[i].varType = CUDD_VAR_PRIMARY_INPUT;
      newsubtables[i].pairIndex = 0;
      newsubtables[i].varHandled = 0;
      newsubtables[i].varToBeGrouped = CUDD_LAZY_NONE;

      newperm[i] = i;
      newinvperm[i] = i;
      newnodelist = newsubtables[i].nodelist = ALLOC(DdNodePtr, numSlots);
      if (newnodelist == NULL) {
        unique->errorCode = CUDD_MEMORY_OUT;
        return (0);
      }
      for (j = 0; j < numSlots; j++) {
        newnodelist[j] = sentinel;
      }
    }
    if (unique->map != NULL) {
      for (i = 0; i < oldsize; i++) {
        newmap[i] = unique->map[i];
      }
      for (i = oldsize; i <= index; i++) {
        newmap[i] = i;
      }
      FREE(unique->map);
      unique->map = newmap;
    }
    FREE(unique->subtables);
    unique->subtables = newsubtables;
    unique->maxSize = newsize;
    FREE(unique->vars);
    unique->vars = newvars;
    FREE(unique->perm);
    unique->perm = newperm;
    FREE(unique->invperm);
    unique->invperm = newinvperm;
  }

  /* Now that the table is in a coherent state, create the new
  ** projection functions. We need to temporarily disable reordering,
  ** because we cannot reorder without projection functions in place.
  **/
  if (index >= 0) {
    one = unique->one;
    zero = Cudd_Not(one);

    unique->size = index + 1;
    if (unique->tree != NULL) {
      unique->tree->size = ddMax(unique->tree->size, unique->size);
    }
    unique->slots += (index + 1 - oldsize) * numSlots;
    ddFixLimits(unique);

    reorderSave = unique->autoDyn;
    unique->autoDyn = 0;
    for (i = oldsize; i <= index; i++) {
      unique->vars[i] = cuddUniqueInter(unique, i, one, zero);
      if (unique->vars[i] == NULL) {
        unique->autoDyn = reorderSave;
        for (j = oldsize; j < i; j++) {
          Cudd_IterDerefBdd(unique, unique->vars[j]);
          cuddDeallocNode(unique, unique->vars[j]);
          unique->vars[j] = NULL;
        }
        for (j = oldsize; j <= index; j++) {
          FREE(unique->subtables[j].nodelist);
          unique->subtables[j].nodelist = NULL;
        }
        unique->size = oldsize;
        unique->slots -= (index + 1 - oldsize) * numSlots;
        ddFixLimits(unique);
        return (0);
      }
      cuddRef(unique->vars[i]);
    }
    unique->autoDyn = reorderSave;
  }

  return (1);

} /* end of ddResizeTable */

/* end of cuddFindParent */

/**Function********************************************************************

  Synopsis    [Adjusts the values of table limits.]

  Description [Adjusts the values of table fields controlling the.
  sizes of subtables and computed table. If the computed table is too small
  according to the new values, it is resized.]

  SideEffects [Modifies manager fields. May resize computed table.]

  SeeAlso     []

******************************************************************************/
DD_INLINE
static void ddFixLimits(DdManager *unique) {
  unique->minDead = (unsigned)(unique->gcFrac * (double)unique->slots);
  unique->cacheSlack = (int)ddMin(unique->maxCacheHard,
                                  DD_MAX_CACHE_TO_SLOTS_RATIO * unique->slots) -
                       2 * (int)unique->cacheSlots;
  if (unique->cacheSlots < unique->slots / 2 && unique->cacheSlack >= 0)
    cuddCacheResize(unique);
  return;

} /* end of ddFixLimits */

#ifndef DD_UNSORTED_FREE_LIST
#ifdef DD_RED_BLACK_FREE_LIST
/**Function********************************************************************

  Synopsis    [Inserts a DdNode in a red/black search tree.]

  Description [Inserts a DdNode in a red/black search tree. Nodes from
  the same "page" (defined by DD_PAGE_MASK) are linked in a LIFO list.]

  SideEffects [None]

  SeeAlso     [cuddOrderedThread]

******************************************************************************/
static void cuddOrderedInsert(DdNodePtr *root, DdNodePtr node) {
  DdNode *scan;
  DdNodePtr *scanP;
  DdNodePtr *stack[DD_STACK_SIZE];
  int stackN = 0;

  scanP = root;
  while ((scan = *scanP) != NULL) {
    stack[stackN++] = scanP;
    if (DD_INSERT_COMPARE(node, scan) == 0) { /* add to page list */
      DD_NEXT(node) = DD_NEXT(scan);
      DD_NEXT(scan) = node;
      return;
    }
    scanP = (node < scan) ? &DD_LEFT(scan) : &DD_RIGHT(scan);
  }
  DD_RIGHT(node) = DD_LEFT(node) = DD_NEXT(node) = NULL;
  DD_COLOR(node) = DD_RED;
  *scanP = node;
  stack[stackN] = &node;
  cuddDoRebalance(stack, stackN);

} /* end of cuddOrderedInsert */

/**Function********************************************************************

  Synopsis    [Threads all the nodes of a search tree into a linear list.]

  Description [Threads all the nodes of a search tree into a linear
  list. For each node of the search tree, the "left" child, if non-null, has
  a lower address than its parent, and the "right" child, if non-null, has a
  higher address than its parent.
  The list is sorted in order of increasing addresses. The search
  tree is destroyed as a result of this operation. The last element of
  the linear list is made to point to the address passed in list. Each
  node if the search tree is a linearly-linked list of nodes from the
  same memory page (as defined in DD_PAGE_MASK). When a node is added to
  the linear list, all the elements of the linked list are added.]

  SideEffects [The search tree is destroyed as a result of this operation.]

  SeeAlso     [cuddOrderedInsert]

******************************************************************************/
static DdNode *cuddOrderedThread(DdNode *root, DdNode *list) {
  DdNode *current, *next, *prev, *end;

  current = root;
  /* The first word in the node is used to implement a stack that holds
  ** the nodes from the root of the tree to the current node. Here we
  ** put the root of the tree at the bottom of the stack.
  */
  *((DdNodePtr *)current) = NULL;

  while (current != NULL) {
    if (DD_RIGHT(current) != NULL) {
      /* If possible, we follow the "right" link. Eventually we'll
      ** find the node with the largest address in the current tree.
      ** In this phase we use the first word of a node to implemen
      ** a stack of the nodes on the path from the root to "current".
      ** Also, we disconnect the "right" pointers to indicate that
      ** we have already followed them.
      */
      next = DD_RIGHT(current);
      DD_RIGHT(current) = NULL;
      *((DdNodePtr *)next) = current;
      current = next;
    } else {
      /* We can't proceed along the "right" links any further.
      ** Hence "current" is the largest element in the current tree.
      ** We make this node the new head of "list". (Repeating this
      ** operation until the tree is empty yields the desired linear
      ** threading of all nodes.)
      */
      prev = *((DdNodePtr *)current); /* save prev node on stack in prev */
      /* Traverse the linked list of current until the end. */
      for (end = current; DD_NEXT(end) != NULL; end = DD_NEXT(end))
        ;
      DD_NEXT(end) = list; /* attach "list" at end and make */
      list = current;      /* "current" the new head of "list" */
      /* Now, if current has a "left" child, we push it on the stack.
      ** Otherwise, we just continue with the parent of "current".
      */
      if (DD_LEFT(current) != NULL) {
        next = DD_LEFT(current);
        *((DdNodePtr *)next) = prev;
        current = next;
      } else {
        current = prev;
      }
    }
  }

  return (list);

} /* end of cuddOrderedThread */

/**Function********************************************************************

  Synopsis    [Performs the left rotation for red/black trees.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddRotateRight]

******************************************************************************/
DD_INLINE
static void cuddRotateLeft(DdNodePtr *nodeP) {
  DdNode *newRoot;
  DdNode *oldRoot = *nodeP;

  *nodeP = newRoot = DD_RIGHT(oldRoot);
  DD_RIGHT(oldRoot) = DD_LEFT(newRoot);
  DD_LEFT(newRoot) = oldRoot;

} /* end of cuddRotateLeft */

/**Function********************************************************************

  Synopsis    [Performs the right rotation for red/black trees.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddRotateLeft]

******************************************************************************/
DD_INLINE
static void cuddRotateRight(DdNodePtr *nodeP) {
  DdNode *newRoot;
  DdNode *oldRoot = *nodeP;

  *nodeP = newRoot = DD_LEFT(oldRoot);
  DD_LEFT(oldRoot) = DD_RIGHT(newRoot);
  DD_RIGHT(newRoot) = oldRoot;

} /* end of cuddRotateRight */

/**Function********************************************************************

  Synopsis    [Rebalances a red/black tree.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void cuddDoRebalance(DdNodePtr **stack, int stackN) {
  DdNodePtr *xP, *parentP, *grandpaP;
  DdNode *x, *y, *parent, *grandpa;

  xP = stack[stackN];
  x = *xP;
  /* Work our way back up, re-balancing the tree. */
  while (--stackN >= 0) {
    parentP = stack[stackN];
    parent = *parentP;
    if (DD_IS_BLACK(parent))
      break;
    /* Since the root is black, here a non-null grandparent exists. */
    grandpaP = stack[stackN - 1];
    grandpa = *grandpaP;
    if (parent == DD_LEFT(grandpa)) {
      y = DD_RIGHT(grandpa);
      if (y != NULL && DD_IS_RED(y)) {
        DD_COLOR(parent) = DD_BLACK;
        DD_COLOR(y) = DD_BLACK;
        DD_COLOR(grandpa) = DD_RED;
        x = grandpa;
        stackN--;
      } else {
        if (x == DD_RIGHT(parent)) {
          cuddRotateLeft(parentP);
          DD_COLOR(x) = DD_BLACK;
        } else {
          DD_COLOR(parent) = DD_BLACK;
        }
        DD_COLOR(grandpa) = DD_RED;
        cuddRotateRight(grandpaP);
        break;
      }
    } else {
      y = DD_LEFT(grandpa);
      if (y != NULL && DD_IS_RED(y)) {
        DD_COLOR(parent) = DD_BLACK;
        DD_COLOR(y) = DD_BLACK;
        DD_COLOR(grandpa) = DD_RED;
        x = grandpa;
        stackN--;
      } else {
        if (x == DD_LEFT(parent)) {
          cuddRotateRight(parentP);
          DD_COLOR(x) = DD_BLACK;
        } else {
          DD_COLOR(parent) = DD_BLACK;
        }
        DD_COLOR(grandpa) = DD_RED;
        cuddRotateLeft(grandpaP);
      }
    }
  }
  DD_COLOR(*(stack[0])) = DD_BLACK;

} /* end of cuddDoRebalance */
#endif
#endif

/**Function********************************************************************

  Synopsis    [Fixes a variable tree after the insertion of new subtables.]

  Description [Fixes a variable tree after the insertion of new subtables.
  After such an insertion, the low fields of the tree below the insertion
  point are inconsistent.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void ddPatchTree(DdManager *dd, MtrNode *treenode) {
  MtrNode *auxnode = treenode;

  while (auxnode != NULL) {
    auxnode->low = dd->perm[auxnode->index];
    if (auxnode->child != NULL) {
      ddPatchTree(dd, auxnode->child);
    }
    auxnode = auxnode->younger;
  }

  return;

} /* end of ddPatchTree */

#ifdef DD_DEBUG
/**Function********************************************************************

  Synopsis    [Checks whether a collision list is ordered.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddCheckCollisionOrdering(DdManager *unique, int i, int j) {
  int slots;
  DdNode *node, *next;
  DdNodePtr *nodelist;
  DdNode *sentinel = &(unique->sentinel);

  nodelist = unique->subtables[i].nodelist;
  slots = unique->subtables[i].slots;
  node = nodelist[j];
  if (node == sentinel)
    return (1);
  next = node->next;
  while (next != sentinel) {
    if (cuddT(node) < cuddT(next) ||
        (cuddT(node) == cuddT(next) && cuddE(node) < cuddE(next))) {
      (void)fprintf(unique->err, "Unordered list: index %u, position %d\n", i,
                    j);
      return (0);
    }
    node = next;
    next = node->next;
  }
  return (1);

} /* end of cuddCheckCollisionOrdering */
#endif

/**Function********************************************************************

  Synopsis    [Reports problem in garbage collection.]

  Description []

  SideEffects [None]

  SeeAlso     [cuddGarbageCollect cuddGarbageCollectZdd]

******************************************************************************/
static void
ddReportRefMess(DdManager *unique /* manager */,
                int i /* table in which the problem occurred */,
                const char *caller /* procedure that detected the problem */) {
  if (i == CUDD_CONST_INDEX) {
    (void)fprintf(unique->err, "%s: problem in constants\n", caller);
  } else if (i != -1) {
    (void)fprintf(unique->err, "%s: problem in table %d\n", caller, i);
  }
  (void)fprintf(unique->err, "  dead count != deleted\n");
  (void)fprintf(unique->err, "  This problem is often due to a missing \
call to Cudd_Ref\n  or to an extra call to Cudd_RecursiveDeref.\n  \
See the CUDD Programmer's Guide for additional details.");
  abort();

} /* end of ddReportRefMess */
/**CFile***********************************************************************

  FileName    [cuddUtil.c]

  PackageName [cudd]

  Synopsis    [Utility functions.]

  Description [External procedures included in this module:
                <ul>
                <li> Cudd_PrintMinterm()
                <li> Cudd_bddPrintCover()
                <li> Cudd_PrintDebug()
                <li> Cudd_DagSize()
                <li> Cudd_EstimateCofactor()
                <li> Cudd_EstimateCofactorSimple()
                <li> Cudd_SharingSize()
                <li> Cudd_CountMinterm()
                <li> Cudd_EpdCountMinterm()
                <li> Cudd_CountPath()
                <li> Cudd_CountPathsToNonZero()
                <li> Cudd_SupportIndices()
                <li> Cudd_Support()
                <li> Cudd_SupportIndex()
                <li> Cudd_SupportSize()
                <li> Cudd_VectorSupportIndices()
                <li> Cudd_VectorSupport()
                <li> Cudd_VectorSupportIndex()
                <li> Cudd_VectorSupportSize()
                <li> Cudd_ClassifySupport()
                <li> Cudd_CountLeaves()
                <li> Cudd_bddPickOneCube()
                <li> Cudd_bddPickOneMinterm()
                <li> Cudd_bddPickArbitraryMinterms()
                <li> Cudd_SubsetWithMaskVars()
                <li> Cudd_FirstCube()
                <li> Cudd_NextCube()
                <li> Cudd_bddComputeCube()
                <li> Cudd_addComputeCube()
                <li> Cudd_FirstNode()
                <li> Cudd_NextNode()
                <li> Cudd_GenFree()
                <li> Cudd_IsGenEmpty()
                <li> Cudd_IndicesToCube()
                <li> Cudd_PrintVersion()
                <li> Cudd_AverageDistance()
                <li> Cudd_Random()
                <li> Cudd_Srandom()
                <li> Cudd_Density()
                </ul>
        Internal procedures included in this module:
                <ul>
                <li> cuddP()
                <li> cuddStCountfree()
                <li> cuddCollectNodes()
                <li> cuddNodeArray()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> dp2()
                <li> ddPrintMintermAux()
                <li> ddDagInt()
                <li> ddCountMintermAux()
                <li> ddEpdCountMintermAux()
                <li> ddCountPathAux()
                <li> ddSupportStep()
                <li> ddClearFlag()
                <li> ddLeavesInt()
                <li> ddPickArbitraryMinterms()
                <li> ddPickRepresentativeCube()
                <li> ddEpdFree()
                <li> ddFindSupport()
                <li> ddClearVars()
                <li> indexCompare()
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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/* Random generator constants. */
#define MODULUS1 2147483563
#define LEQA1 40014
#define LEQQ1 53668
#define LEQR1 12211
#define MODULUS2 2147483399
#define LEQA2 40692
#define LEQQ2 52774
#define LEQR2 3791
#define STAB_SIZE 64
#define STAB_DIV (1 + (MODULUS1 - 1) / STAB_SIZE)

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
// static char rcsid[] DD_UNUSED = "$Id: cuddUtil.c,v 1.83 2012/02/05 01:07:19
// fabio Exp $"; #endif

static DdNode *background, *zero;

static long cuddRand = 0;
static long cuddRand2;
static long shuffleSelect;
static long shuffleTable[STAB_SIZE];

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#define bang(f) ((Cudd_IsComplement(f)) ? '!' : ' ')

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int dp2(DdManager *dd, DdNode *f, st_table *t);
static void ddPrintMintermAux(DdManager *dd, DdNode *node, int *list);
static int ddDagInt(DdNode *n);
static int cuddNodeArrayRecur(DdNode *f, DdNodePtr *table, int index);
static int cuddEstimateCofactor(DdManager *dd, st_table *table, DdNode *node,
                                int i, int phase, DdNode **ptr);
static DdNode *cuddUniqueLookup(DdManager *unique, int index, DdNode *T,
                                DdNode *E);
static int cuddEstimateCofactorSimple(DdNode *node, int i);
static double ddCountMintermAux(DdNode *node, double max, DdHashTable *table);
static int ddEpdCountMintermAux(DdNode *node, EpDouble *max, EpDouble *epd,
                                st_table *table);
static double ddCountPathAux(DdNode *node, st_table *table);
static double ddCountPathsToNonZero(DdNode *N, st_table *table);
static void ddSupportStep(DdNode *f, int *support);
static void ddClearFlag(DdNode *f);
static int ddLeavesInt(DdNode *n);
static int ddPickArbitraryMinterms(DdManager *dd, DdNode *node, int nvars,
                                   int nminterms, char **string);

static void ddFindSupport(DdManager *dd, DdNode *f, int *SP);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Counts the number of minterms of a DD.]

  Description [Counts the number of minterms of a DD. The function is
  assumed to depend on nvars variables. The minterm count is
  represented as a double, to allow for a larger number of variables.
  Returns the number of minterms of the function rooted at node if
  successful; (double) CUDD_OUT_OF_MEM otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_PrintDebug Cudd_CountPath]

******************************************************************************/
double Cudd_CountMinterm(DdManager *manager, DdNode *node, int nvars) {
  double max;
  DdHashTable *table;
  double res;
  CUDD_VALUE_TYPE epsilon;

  background = manager->background;
  zero = Cudd_Not(manager->one);

  max = pow(2.0, (double)nvars);
  table = cuddHashTableInit(manager, 1, 2);
  if (table == NULL) {
    return ((double)CUDD_OUT_OF_MEM);
  }
  epsilon = Cudd_ReadEpsilon(manager);
  Cudd_SetEpsilon(manager, (CUDD_VALUE_TYPE)0.0);
  res = ddCountMintermAux(node, max, table);
  cuddHashTableQuit(table);
  Cudd_SetEpsilon(manager, epsilon);

  return (res);

} /* end of Cudd_CountMinterm */

/**Function********************************************************************

  Synopsis    [Finds the first cube of a decision diagram.]

  Description [Defines an iterator on the onset of a decision diagram
  and finds its first cube. Returns a generator that contains the
  information necessary to continue the enumeration if successful; NULL
  otherwise.<p>
  A cube is represented as an array of literals, which are integers in
  {0, 1, 2}; 0 represents a complemented literal, 1 represents an
  uncomplemented literal, and 2 stands for don't care. The enumeration
  produces a disjoint cover of the function associated with the diagram.
  The size of the array equals the number of variables in the manager at
  the time Cudd_FirstCube is called.<p>
  For each cube, a value is also returned. This value is always 1 for a
  BDD, while it may be different from 1 for an ADD.
  For BDDs, the offset is the set of cubes whose value is the logical zero.
  For ADDs, the offset is the set of cubes whose value is the
  background value. The cubes of the offset are not enumerated.]

  SideEffects [The first cube and its value are returned as side effects.]

  SeeAlso     [Cudd_ForeachCube Cudd_NextCube Cudd_GenFree Cudd_IsGenEmpty
  Cudd_FirstNode]

******************************************************************************/
DdGen *Cudd_FirstCube(DdManager *dd, DdNode *f, int **cube,
                      CUDD_VALUE_TYPE *value) {
  DdGen *gen;
  DdNode *top, *treg, *next, *nreg, *prev, *preg;
  int i;
  int nvars;

  /* Sanity Check. */
  if (dd == NULL || f == NULL)
    return (NULL);

  /* Allocate generator an initialize it. */
  gen = ALLOC(DdGen, 1);
  if (gen == NULL) {
    dd->errorCode = CUDD_MEMORY_OUT;
    return (NULL);
  }

  gen->manager = dd;
  gen->type = CUDD_GEN_CUBES;
  gen->status = CUDD_GEN_EMPTY;
  gen->gen.cubes.cube = NULL;
  gen->gen.cubes.value = DD_ZERO_VAL;
  gen->stack.sp = 0;
  gen->stack.stack = NULL;
  gen->node = NULL;

  nvars = dd->size;
  gen->gen.cubes.cube = ALLOC(int, nvars);
  if (gen->gen.cubes.cube == NULL) {
    dd->errorCode = CUDD_MEMORY_OUT;
    FREE(gen);
    return (NULL);
  }
  for (i = 0; i < nvars; i++)
    gen->gen.cubes.cube[i] = 2;

  /* The maximum stack depth is one plus the number of variables.
  ** because a path may have nodes at all levels, including the
  ** constant level.
  */
  gen->stack.stack = ALLOC(DdNodePtr, nvars + 1);
  if (gen->stack.stack == NULL) {
    dd->errorCode = CUDD_MEMORY_OUT;
    FREE(gen->gen.cubes.cube);
    FREE(gen);
    return (NULL);
  }
  for (i = 0; i <= nvars; i++)
    gen->stack.stack[i] = NULL;

  /* Find the first cube of the onset. */
  gen->stack.stack[gen->stack.sp] = f;
  gen->stack.sp++;

  while (1) {
    top = gen->stack.stack[gen->stack.sp - 1];
    treg = Cudd_Regular(top);
    if (!cuddIsConstant(treg)) {
      /* Take the else branch first. */
      gen->gen.cubes.cube[treg->index] = 0;
      next = cuddE(treg);
      if (top != treg)
        next = Cudd_Not(next);
      gen->stack.stack[gen->stack.sp] = next;
      gen->stack.sp++;
    } else if (top == Cudd_Not(DD_ONE(dd)) || top == dd->background) {
      /* Backtrack */
      while (1) {
        if (gen->stack.sp == 1) {
          /* The current node has no predecessor. */
          gen->status = CUDD_GEN_EMPTY;
          gen->stack.sp--;
          goto done;
        }
        prev = gen->stack.stack[gen->stack.sp - 2];
        preg = Cudd_Regular(prev);
        nreg = cuddT(preg);
        if (prev != preg) {
          next = Cudd_Not(nreg);
        } else {
          next = nreg;
        }
        if (next != top) { /* follow the then branch next */
          gen->gen.cubes.cube[preg->index] = 1;
          gen->stack.stack[gen->stack.sp - 1] = next;
          break;
        }
        /* Pop the stack and try again. */
        gen->gen.cubes.cube[preg->index] = 2;
        gen->stack.sp--;
        top = gen->stack.stack[gen->stack.sp - 1];
        treg = Cudd_Regular(top);
      }
    } else {
      gen->status = CUDD_GEN_NONEMPTY;
      gen->gen.cubes.value = cuddV(top);
      goto done;
    }
  }

done:
  *cube = gen->gen.cubes.cube;
  *value = gen->gen.cubes.value;
  return (gen);

} /* end of Cudd_FirstCube */

/**Function********************************************************************

  Synopsis    [Generates the next cube of a decision diagram onset.]

  Description [Generates the next cube of a decision diagram onset,
  using generator gen. Returns 0 if the enumeration is completed; 1
  otherwise.]

  SideEffects [The cube and its value are returned as side effects. The
  generator is modified.]

  SeeAlso     [Cudd_ForeachCube Cudd_FirstCube Cudd_GenFree Cudd_IsGenEmpty
  Cudd_NextNode]

******************************************************************************/
int Cudd_NextCube(DdGen *gen, int **cube, CUDD_VALUE_TYPE *value) {
  DdNode *top, *treg, *next, *nreg, *prev, *preg;
  DdManager *dd = gen->manager;

  /* Backtrack from previously reached terminal node. */
  while (1) {
    if (gen->stack.sp == 1) {
      /* The current node has no predecessor. */
      gen->status = CUDD_GEN_EMPTY;
      gen->stack.sp--;
      goto done;
    }
    top = gen->stack.stack[gen->stack.sp - 1];
    treg = Cudd_Regular(top);
    prev = gen->stack.stack[gen->stack.sp - 2];
    preg = Cudd_Regular(prev);
    nreg = cuddT(preg);
    if (prev != preg) {
      next = Cudd_Not(nreg);
    } else {
      next = nreg;
    }
    if (next != top) { /* follow the then branch next */
      gen->gen.cubes.cube[preg->index] = 1;
      gen->stack.stack[gen->stack.sp - 1] = next;
      break;
    }
    /* Pop the stack and try again. */
    gen->gen.cubes.cube[preg->index] = 2;
    gen->stack.sp--;
  }

  while (1) {
    top = gen->stack.stack[gen->stack.sp - 1];
    treg = Cudd_Regular(top);
    if (!cuddIsConstant(treg)) {
      /* Take the else branch first. */
      gen->gen.cubes.cube[treg->index] = 0;
      next = cuddE(treg);
      if (top != treg)
        next = Cudd_Not(next);
      gen->stack.stack[gen->stack.sp] = next;
      gen->stack.sp++;
    } else if (top == Cudd_Not(DD_ONE(dd)) || top == dd->background) {
      /* Backtrack */
      while (1) {
        if (gen->stack.sp == 1) {
          /* The current node has no predecessor. */
          gen->status = CUDD_GEN_EMPTY;
          gen->stack.sp--;
          goto done;
        }
        prev = gen->stack.stack[gen->stack.sp - 2];
        preg = Cudd_Regular(prev);
        nreg = cuddT(preg);
        if (prev != preg) {
          next = Cudd_Not(nreg);
        } else {
          next = nreg;
        }
        if (next != top) { /* follow the then branch next */
          gen->gen.cubes.cube[preg->index] = 1;
          gen->stack.stack[gen->stack.sp - 1] = next;
          break;
        }
        /* Pop the stack and try again. */
        gen->gen.cubes.cube[preg->index] = 2;
        gen->stack.sp--;
        top = gen->stack.stack[gen->stack.sp - 1];
        treg = Cudd_Regular(top);
      }
    } else {
      gen->status = CUDD_GEN_NONEMPTY;
      gen->gen.cubes.value = cuddV(top);
      goto done;
    }
  }

done:
  if (gen->status == CUDD_GEN_EMPTY)
    return (0);
  *cube = gen->gen.cubes.cube;
  *value = gen->gen.cubes.value;
  return (1);

} /* end of Cudd_NextCube */

/**Function********************************************************************

  Synopsis    [Finds the first prime of a Boolean function.]

  Description [Defines an iterator on a pair of BDDs describing a
  (possibly incompletely specified) Boolean functions and finds the
  first cube of a cover of the function.  Returns a generator
  that contains the information necessary to continue the enumeration
  if successful; NULL otherwise.<p>

  The two argument BDDs are the lower and upper bounds of an interval.
  It is a mistake to call this function with a lower bound that is not
  less than or equal to the upper bound.<p>

  A cube is represented as an array of literals, which are integers in
  {0, 1, 2}; 0 represents a complemented literal, 1 represents an
  uncomplemented literal, and 2 stands for don't care. The enumeration
  produces a prime and irredundant cover of the function associated
  with the two BDDs.  The size of the array equals the number of
  variables in the manager at the time Cudd_FirstCube is called.<p>

  This iterator can only be used on BDDs.]

  SideEffects [The first cube is returned as side effect.]

  SeeAlso     [Cudd_ForeachPrime Cudd_NextPrime Cudd_GenFree Cudd_IsGenEmpty
  Cudd_FirstCube Cudd_FirstNode]

******************************************************************************/
DdGen *Cudd_FirstPrime(DdManager *dd, DdNode *l, DdNode *u, int **cube) {
  DdGen *gen;
  DdNode *implicant, *prime, *tmp;
  int length, result;

  /* Sanity Check. */
  if (dd == NULL || l == NULL || u == NULL)
    return (NULL);

  /* Allocate generator an initialize it. */
  gen = ALLOC(DdGen, 1);
  if (gen == NULL) {
    dd->errorCode = CUDD_MEMORY_OUT;
    return (NULL);
  }

  gen->manager = dd;
  gen->type = CUDD_GEN_PRIMES;
  gen->status = CUDD_GEN_EMPTY;
  gen->gen.primes.cube = NULL;
  gen->gen.primes.ub = u;
  gen->stack.sp = 0;
  gen->stack.stack = NULL;
  gen->node = l;
  cuddRef(l);

  gen->gen.primes.cube = ALLOC(int, dd->size);
  if (gen->gen.primes.cube == NULL) {
    dd->errorCode = CUDD_MEMORY_OUT;
    FREE(gen);
    return (NULL);
  }

  if (gen->node == Cudd_ReadLogicZero(dd)) {
    gen->status = CUDD_GEN_EMPTY;
  } else {
    implicant = Cudd_LargestCube(dd, gen->node, &length);
    if (implicant == NULL) {
      Cudd_RecursiveDeref(dd, gen->node);
      FREE(gen->gen.primes.cube);
      FREE(gen);
      return (NULL);
    }
    cuddRef(implicant);
    prime = Cudd_bddMakePrime(dd, implicant, gen->gen.primes.ub);
    if (prime == NULL) {
      Cudd_RecursiveDeref(dd, gen->node);
      Cudd_RecursiveDeref(dd, implicant);
      FREE(gen->gen.primes.cube);
      FREE(gen);
      return (NULL);
    }
    cuddRef(prime);
    Cudd_RecursiveDeref(dd, implicant);
    tmp = Cudd_bddAnd(dd, gen->node, Cudd_Not(prime));
    if (tmp == NULL) {
      Cudd_RecursiveDeref(dd, gen->node);
      Cudd_RecursiveDeref(dd, prime);
      FREE(gen->gen.primes.cube);
      FREE(gen);
      return (NULL);
    }
    cuddRef(tmp);
    Cudd_RecursiveDeref(dd, gen->node);
    gen->node = tmp;
    result = Cudd_BddToCubeArray(dd, prime, gen->gen.primes.cube);
    if (result == 0) {
      Cudd_RecursiveDeref(dd, gen->node);
      Cudd_RecursiveDeref(dd, prime);
      FREE(gen->gen.primes.cube);
      FREE(gen);
      return (NULL);
    }
    Cudd_RecursiveDeref(dd, prime);
    gen->status = CUDD_GEN_NONEMPTY;
  }
  *cube = gen->gen.primes.cube;
  return (gen);

} /* end of Cudd_FirstPrime */

/**Function********************************************************************

  Synopsis    [Generates the next prime of a Boolean function.]

  Description [Generates the next cube of a Boolean function,
  using generator gen. Returns 0 if the enumeration is completed; 1
  otherwise.]

  SideEffects [The cube and is returned as side effects. The
  generator is modified.]

  SeeAlso     [Cudd_ForeachPrime Cudd_FirstPrime Cudd_GenFree Cudd_IsGenEmpty
  Cudd_NextCube Cudd_NextNode]

******************************************************************************/
int Cudd_NextPrime(DdGen *gen, int **cube) {
  DdNode *implicant, *prime, *tmp;
  DdManager *dd = gen->manager;
  int length, result;

  if (gen->node == Cudd_ReadLogicZero(dd)) {
    gen->status = CUDD_GEN_EMPTY;
  } else {
    implicant = Cudd_LargestCube(dd, gen->node, &length);
    if (implicant == NULL) {
      gen->status = CUDD_GEN_EMPTY;
      return (0);
    }
    cuddRef(implicant);
    prime = Cudd_bddMakePrime(dd, implicant, gen->gen.primes.ub);
    if (prime == NULL) {
      Cudd_RecursiveDeref(dd, implicant);
      gen->status = CUDD_GEN_EMPTY;
      return (0);
    }
    cuddRef(prime);
    Cudd_RecursiveDeref(dd, implicant);
    tmp = Cudd_bddAnd(dd, gen->node, Cudd_Not(prime));
    if (tmp == NULL) {
      Cudd_RecursiveDeref(dd, prime);
      gen->status = CUDD_GEN_EMPTY;
      return (0);
    }
    cuddRef(tmp);
    Cudd_RecursiveDeref(dd, gen->node);
    gen->node = tmp;
    result = Cudd_BddToCubeArray(dd, prime, gen->gen.primes.cube);
    if (result == 0) {
      Cudd_RecursiveDeref(dd, prime);
      gen->status = CUDD_GEN_EMPTY;
      return (0);
    }
    Cudd_RecursiveDeref(dd, prime);
    gen->status = CUDD_GEN_NONEMPTY;
  }
  if (gen->status == CUDD_GEN_EMPTY)
    return (0);
  *cube = gen->gen.primes.cube;
  return (1);

} /* end of Cudd_NextPrime */

/**Function********************************************************************

  Synopsis    [Builds a positional array from the BDD of a cube.]

  Description [Builds a positional array from the BDD of a cube.
  Array must have one entry for each BDD variable.  The positional
  array has 1 in i-th position if the variable of index i appears in
  true form in the cube; it has 0 in i-th position if the variable of
  index i appears in complemented form in the cube; finally, it has 2
  in i-th position if the variable of index i does not appear in the
  cube.  Returns 1 if successful (the BDD is indeed a cube); 0
  otherwise.]

  SideEffects [The result is in the array passed by reference.]

  SeeAlso     [Cudd_CubeArrayToBdd]

******************************************************************************/
int Cudd_BddToCubeArray(DdManager *dd, DdNode *cube, int *array) {
  DdNode *scan, *t, *e;
  int i;
  int size = Cudd_ReadSize(dd);
  DdNode *zero = Cudd_Not(DD_ONE(dd));

  for (i = size - 1; i >= 0; i--) {
    array[i] = 2;
  }
  scan = cube;
  while (!Cudd_IsConstant(scan)) {
    int index = Cudd_Regular(scan)->index;
    cuddGetBranches(scan, &t, &e);
    if (t == zero) {
      array[index] = 0;
      scan = e;
    } else if (e == zero) {
      array[index] = 1;
      scan = t;
    } else {
      return (0); /* cube is not a cube */
    }
  }
  if (scan == zero) {
    return (0);
  } else {
    return (1);
  }

} /* end of Cudd_BddToCubeArray */

/**Function********************************************************************

  Synopsis    [Finds the first node of a decision diagram.]

  Description [Defines an iterator on the nodes of a decision diagram
  and finds its first node. Returns a generator that contains the
  information necessary to continue the enumeration if successful;
  NULL otherwise.  The nodes are enumerated in a reverse topological
  order, so that a node is always preceded in the enumeration by its
  descendants.]

  SideEffects [The first node is returned as a side effect.]

  SeeAlso     [Cudd_ForeachNode Cudd_NextNode Cudd_GenFree Cudd_IsGenEmpty
  Cudd_FirstCube]

******************************************************************************/
DdGen *Cudd_FirstNode(DdManager *dd, DdNode *f, DdNode **node) {
  DdGen *gen;
  int size;

  /* Sanity Check. */
  if (dd == NULL || f == NULL)
    return (NULL);

  /* Allocate generator an initialize it. */
  gen = ALLOC(DdGen, 1);
  if (gen == NULL) {
    dd->errorCode = CUDD_MEMORY_OUT;
    return (NULL);
  }

  gen->manager = dd;
  gen->type = CUDD_GEN_NODES;
  gen->status = CUDD_GEN_EMPTY;
  gen->stack.sp = 0;
  gen->node = NULL;

  /* Collect all the nodes on the generator stack for later perusal. */
  gen->stack.stack = cuddNodeArray(Cudd_Regular(f), &size);
  if (gen->stack.stack == NULL) {
    FREE(gen);
    dd->errorCode = CUDD_MEMORY_OUT;
    return (NULL);
  }
  gen->gen.nodes.size = size;

  /* Find the first node. */
  if (gen->stack.sp < gen->gen.nodes.size) {
    gen->status = CUDD_GEN_NONEMPTY;
    gen->node = gen->stack.stack[gen->stack.sp];
    *node = gen->node;
  }

  return (gen);

} /* end of Cudd_FirstNode */

/**Function********************************************************************

  Synopsis    [Finds the next node of a decision diagram.]

  Description [Finds the node of a decision diagram, using generator
  gen. Returns 0 if the enumeration is completed; 1 otherwise.]

  SideEffects [The next node is returned as a side effect.]

  SeeAlso     [Cudd_ForeachNode Cudd_FirstNode Cudd_GenFree Cudd_IsGenEmpty
  Cudd_NextCube]

******************************************************************************/
int Cudd_NextNode(DdGen *gen, DdNode **node) {
  /* Find the next node. */
  gen->stack.sp++;
  if (gen->stack.sp < gen->gen.nodes.size) {
    gen->node = gen->stack.stack[gen->stack.sp];
    *node = gen->node;
    return (1);
  } else {
    gen->status = CUDD_GEN_EMPTY;
    return (0);
  }

} /* end of Cudd_NextNode */

/**Function********************************************************************

  Synopsis    [Frees a CUDD generator.]

  Description [Frees a CUDD generator. Always returns 0, so that it can
  be used in mis-like foreach constructs.]

  SideEffects [None]

  SeeAlso     [Cudd_ForeachCube Cudd_ForeachNode Cudd_FirstCube Cudd_NextCube
  Cudd_FirstNode Cudd_NextNode Cudd_IsGenEmpty]

******************************************************************************/
int Cudd_GenFree(DdGen *gen) {
  if (gen == NULL)
    return (0);
  switch (gen->type) {
  case CUDD_GEN_CUBES:
  case CUDD_GEN_ZDD_PATHS:
    FREE(gen->gen.cubes.cube);
    FREE(gen->stack.stack);
    break;
  case CUDD_GEN_PRIMES:
    FREE(gen->gen.primes.cube);
    Cudd_RecursiveDeref(gen->manager, gen->node);
    break;
  case CUDD_GEN_NODES:
    FREE(gen->stack.stack);
    break;
  default:
    return (0);
  }
  FREE(gen);
  return (0);

} /* end of Cudd_GenFree */

/**Function********************************************************************

  Synopsis    [Queries the status of a generator.]

  Description [Queries the status of a generator. Returns 1 if the
  generator is empty or NULL; 0 otherswise.]

  SideEffects [None]

  SeeAlso     [Cudd_ForeachCube Cudd_ForeachNode Cudd_FirstCube Cudd_NextCube
  Cudd_FirstNode Cudd_NextNode Cudd_GenFree]

******************************************************************************/
int Cudd_IsGenEmpty(DdGen *gen) {
  if (gen == NULL)
    return (1);
  return (gen->status == CUDD_GEN_EMPTY);

} /* end of Cudd_IsGenEmpty */

/**Function********************************************************************

  Synopsis    [Portable random number generator.]

  Description [Portable number generator based on ran2 from "Numerical
  Recipes in C." It is a long period (> 2 * 10^18) random number generator
  of L'Ecuyer with Bays-Durham shuffle. Returns a long integer uniformly
  distributed between 0 and 2147483561 (inclusive of the endpoint values).
  The random generator can be explicitly initialized by calling
  Cudd_Srandom. If no explicit initialization is performed, then the
  seed 1 is assumed.]

  SideEffects [None]

  SeeAlso     [Cudd_Srandom]

******************************************************************************/
long Cudd_Random(void) {
  int i;      /* index in the shuffle table */
  long int w; /* work variable */

  /* cuddRand == 0 if the geneartor has not been initialized yet. */
  if (cuddRand == 0)
    Cudd_Srandom(1);

  /* Compute cuddRand = (cuddRand * LEQA1) % MODULUS1 avoiding
  ** overflows by Schrage's method.
  */
  w = cuddRand / LEQQ1;
  cuddRand = LEQA1 * (cuddRand - w * LEQQ1) - w * LEQR1;
  cuddRand += (cuddRand < 0) * MODULUS1;

  /* Compute cuddRand2 = (cuddRand2 * LEQA2) % MODULUS2 avoiding
  ** overflows by Schrage's method.
  */
  w = cuddRand2 / LEQQ2;
  cuddRand2 = LEQA2 * (cuddRand2 - w * LEQQ2) - w * LEQR2;
  cuddRand2 += (cuddRand2 < 0) * MODULUS2;

  /* cuddRand is shuffled with the Bays-Durham algorithm.
  ** shuffleSelect and cuddRand2 are combined to generate the output.
  */

  /* Pick one element from the shuffle table; "i" will be in the range
  ** from 0 to STAB_SIZE-1.
  */
  i = (int)(shuffleSelect / STAB_DIV);
  /* Mix the element of the shuffle table with the current iterate of
  ** the second sub-generator, and replace the chosen element of the
  ** shuffle table with the current iterate of the first sub-generator.
  */
  shuffleSelect = shuffleTable[i] - cuddRand2;
  shuffleTable[i] = cuddRand;
  shuffleSelect += (shuffleSelect < 1) * (MODULUS1 - 1);
  /* Since shuffleSelect != 0, and we want to be able to return 0,
  ** here we subtract 1 before returning.
  */
  return (shuffleSelect - 1);

} /* end of Cudd_Random */

/**Function********************************************************************

  Synopsis    [Initializer for the portable random number generator.]

  Description [Initializer for the portable number generator based on
  ran2 in "Numerical Recipes in C." The input is the seed for the
  generator. If it is negative, its absolute value is taken as seed.
  If it is 0, then 1 is taken as seed. The initialized sets up the two
  recurrences used to generate a long-period stream, and sets up the
  shuffle table.]

  SideEffects [None]

  SeeAlso     [Cudd_Random]

******************************************************************************/
void Cudd_Srandom(long seed) {
  int i;

  if (seed < 0)
    cuddRand = -seed;
  else if (seed == 0)
    cuddRand = 1;
  else
    cuddRand = seed;
  cuddRand2 = cuddRand;
  /* Load the shuffle table (after 11 warm-ups). */
  for (i = 0; i < STAB_SIZE + 11; i++) {
    long int w;
    w = cuddRand / LEQQ1;
    cuddRand = LEQA1 * (cuddRand - w * LEQQ1) - w * LEQR1;
    cuddRand += (cuddRand < 0) * MODULUS1;
    shuffleTable[i % STAB_SIZE] = cuddRand;
  }
  shuffleSelect = shuffleTable[1 % STAB_SIZE];

} /* end of Cudd_Srandom */

/**Function********************************************************************

  Synopsis    [Warns that a memory allocation failed.]

  Description [Warns that a memory allocation failed.
  This function can be used as replacement of MMout_of_memory to prevent
  the safe_mem functions of the util package from exiting when malloc
  returns NULL. One possible use is in case of discretionary allocations;
  for instance, the allocation of memory to enlarge the computed table.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void Cudd_OutOfMem(long size /* size of the allocation that failed */) {
  (void)fflush(stdout);
  (void)fprintf(stderr, "\nunable to allocate %ld bytes\n", size);
  return;

} /* end of Cudd_OutOfMem */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Recursively collects all the nodes of a DD in a symbol
  table.]

  Description [Traverses the DD f and collects all its nodes in a
  symbol table.  f is assumed to be a regular pointer and
  cuddCollectNodes guarantees this assumption in the recursive calls.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddCollectNodes(DdNode *f, st_table *visited) {
  DdNode *T, *E;
  int retval;

#ifdef DD_DEBUG
  assert(!Cudd_IsComplement(f));
#endif

  /* If already visited, nothing to do. */
  if (st_is_member(visited, (char *)f) == 1)
    return (1);

  /* Check for abnormal condition that should never happen. */
  if (f == NULL)
    return (0);

  /* Mark node as visited. */
  if (st_add_direct(visited, (char *)f, NULL) == ST_OUT_OF_MEM)
    return (0);

  /* Check terminal case. */
  if (cuddIsConstant(f))
    return (1);

  /* Recursive calls. */
  T = cuddT(f);
  retval = cuddCollectNodes(T, visited);
  if (retval != 1)
    return (retval);
  E = Cudd_Regular(cuddE(f));
  retval = cuddCollectNodes(E, visited);
  return (retval);

} /* end of cuddCollectNodes */

/**Function********************************************************************

  Synopsis    [Recursively collects all the nodes of a DD in an array.]

  Description [Traverses the DD f and collects all its nodes in an array.
  The caller should free the array returned by cuddNodeArray.
  Returns a pointer to the array of nodes in case of success; NULL
  otherwise.  The nodes are collected in reverse topological order, so
  that a node is always preceded in the array by all its descendants.]

  SideEffects [The number of nodes is returned as a side effect.]

  SeeAlso     [Cudd_FirstNode]

******************************************************************************/
DdNodePtr *cuddNodeArray(DdNode *f, int *n) {
  DdNodePtr *table;
  int size, retval;

  size = ddDagInt(Cudd_Regular(f));
  table = ALLOC(DdNodePtr, size);
  if (table == NULL) {
    ddClearFlag(Cudd_Regular(f));
    return (NULL);
  }

  retval = cuddNodeArrayRecur(f, table, 0);
  assert(retval == size);

  *n = size;
  return (table);

} /* cuddNodeArray */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Performs the recursive step of cuddP.]

  Description [Performs the recursive step of cuddP. Returns 1 in case
  of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int dp2(DdManager *dd, DdNode *f, st_table *t) {
  DdNode *g, *n, *N;
  int T, E;

  if (f == NULL) {
    return (0);
  }
  g = Cudd_Regular(f);
  if (cuddIsConstant(g)) {
#if SIZEOF_VOID_P == 8
    (void)fprintf(dd->out, "ID = %c0x%lx\tvalue = %-9g\n", bang(f),
                  (ptruint)g / (ptruint)sizeof(DdNode), cuddV(g));
#else
    (void)fprintf(dd->out, "ID = %c0x%x\tvalue = %-9g\n", bang(f),
                  (ptruint)g / (ptruint)sizeof(DdNode), cuddV(g));
#endif
    return (1);
  }
  if (st_is_member(t, (char *)g) == 1) {
    return (1);
  }
  if (st_add_direct(t, (char *)g, NULL) == ST_OUT_OF_MEM)
    return (0);
#ifdef DD_STATS
#if SIZEOF_VOID_P == 8
  (void)fprintf(dd->out, "ID = %c0x%lx\tindex = %d\tr = %d\t", bang(f),
                (ptruint)g / (ptruint)sizeof(DdNode), g->index, g->ref);
#else
  (void)fprintf(dd->out, "ID = %c0x%x\tindex = %d\tr = %d\t", bang(f),
                (ptruint)g / (ptruint)sizeof(DdNode), g->index, g->ref);
#endif
#else
#if SIZEOF_VOID_P == 8
  (void)fprintf(dd->out, "ID = %c0x%lx\tindex = %u\t", bang(f),
                (ptruint)g / (ptruint)sizeof(DdNode), g->index);
#else
  (void)fprintf(dd->out, "ID = %c0x%x\tindex = %hu\t", bang(f),
                (ptruint)g / (ptruint)sizeof(DdNode), g->index);
#endif
#endif
  n = cuddT(g);
  if (cuddIsConstant(n)) {
    (void)fprintf(dd->out, "T = %-9g\t", cuddV(n));
    T = 1;
  } else {
#if SIZEOF_VOID_P == 8
    (void)fprintf(dd->out, "T = 0x%lx\t", (ptruint)n / (ptruint)sizeof(DdNode));
#else
    (void)fprintf(dd->out, "T = 0x%x\t", (ptruint)n / (ptruint)sizeof(DdNode));
#endif
    T = 0;
  }

  n = cuddE(g);
  N = Cudd_Regular(n);
  if (cuddIsConstant(N)) {
    (void)fprintf(dd->out, "E = %c%-9g\n", bang(n), cuddV(N));
    E = 1;
  } else {
#if SIZEOF_VOID_P == 8
    (void)fprintf(dd->out, "E = %c0x%lx\n", bang(n),
                  (ptruint)N / (ptruint)sizeof(DdNode));
#else
    (void)fprintf(dd->out, "E = %c0x%x\n", bang(n),
                  (ptruint)N / (ptruint)sizeof(DdNode));
#endif
    E = 0;
  }
  if (E == 0) {
    if (dp2(dd, N, t) == 0)
      return (0);
  }
  if (T == 0) {
    if (dp2(dd, cuddT(g), t) == 0)
      return (0);
  }
  return (1);

} /* end of dp2 */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_PrintMinterm.]

  Description []

  SideEffects [None]

******************************************************************************/
static void ddPrintMintermAux(DdManager *dd /* manager */,
                              DdNode *node /* current node */,
                              int *list /* current recursion path */) {
  DdNode *N, *Nv, *Nnv;
  int i, v, index;

  N = Cudd_Regular(node);

  if (cuddIsConstant(N)) {
    /* Terminal case: Print one cube based on the current recursion
    ** path, unless we have reached the background value (ADDs) or
    ** the logical zero (BDDs).
    */
    if (node != background && node != zero) {
      for (i = 0; i < dd->size; i++) {
        v = list[i];
        if (v == 0)
          (void)fprintf(dd->out, "0");
        else if (v == 1)
          (void)fprintf(dd->out, "1");
        else
          (void)fprintf(dd->out, "-");
      }
      (void)fprintf(dd->out, " % g\n", cuddV(node));
    }
  } else {
    Nv = cuddT(N);
    Nnv = cuddE(N);
    if (Cudd_IsComplement(node)) {
      Nv = Cudd_Not(Nv);
      Nnv = Cudd_Not(Nnv);
    }
    index = N->index;
    list[index] = 0;
    ddPrintMintermAux(dd, Nnv, list);
    list[index] = 1;
    ddPrintMintermAux(dd, Nv, list);
    list[index] = 2;
  }
  return;

} /* end of ddPrintMintermAux */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_DagSize.]

  Description [Performs the recursive step of Cudd_DagSize. Returns the
  number of nodes in the graph rooted at n.]

  SideEffects [None]

******************************************************************************/
static int ddDagInt(DdNode *n) {
  int tval, eval;

  if (Cudd_IsComplement(n->next)) {
    return (0);
  }
  n->next = Cudd_Not(n->next);
  if (cuddIsConstant(n)) {
    return (1);
  }
  tval = ddDagInt(cuddT(n));
  eval = ddDagInt(Cudd_Regular(cuddE(n)));
  return (1 + tval + eval);

} /* end of ddDagInt */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of cuddNodeArray.]

  Description [Performs the recursive step of cuddNodeArray.  Returns
  an the number of nodes in the DD.  Clear the least significant bit
  of the next field that was used as visited flag by
  cuddNodeArrayRecur when counting the nodes.  node is supposed to be
  regular; the invariant is maintained by this procedure.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddNodeArrayRecur(DdNode *f, DdNodePtr *table, int index) {
  int tindex, eindex;

  if (!Cudd_IsComplement(f->next)) {
    return (index);
  }
  /* Clear visited flag. */
  f->next = Cudd_Regular(f->next);
  if (cuddIsConstant(f)) {
    table[index] = f;
    return (index + 1);
  }
  tindex = cuddNodeArrayRecur(cuddT(f), table, index);
  eindex = cuddNodeArrayRecur(Cudd_Regular(cuddE(f)), table, tindex);
  table[eindex] = f;
  return (eindex + 1);

} /* end of cuddNodeArrayRecur */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CofactorEstimate.]

  Description [Performs the recursive step of Cudd_CofactorEstimate.
  Returns an estimate of the number of nodes in the DD of a
  cofactor of node. Uses the least significant bit of the next field as
  visited flag. node is supposed to be regular; the invariant is maintained
  by this procedure.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddEstimateCofactor(DdManager *dd, st_table *table, DdNode *node,
                                int i, int phase, DdNode **ptr) {
  int tval, eval, val;
  DdNode *ptrT, *ptrE;

  if (Cudd_IsComplement(node->next)) {
    if (!st_lookup(table, (char *)node, (char **)ptr)) {
      if (st_add_direct(table, (char *)node, (char *)node) == ST_OUT_OF_MEM)
        return (CUDD_OUT_OF_MEM);
      *ptr = node;
    }
    return (0);
  }
  node->next = Cudd_Not(node->next);
  if (cuddIsConstant(node)) {
    *ptr = node;
    if (st_add_direct(table, (char *)node, (char *)node) == ST_OUT_OF_MEM)
      return (CUDD_OUT_OF_MEM);
    return (1);
  }
  if ((int)node->index == i) {
    if (phase == 1) {
      *ptr = cuddT(node);
      val = ddDagInt(cuddT(node));
    } else {
      *ptr = cuddE(node);
      val = ddDagInt(Cudd_Regular(cuddE(node)));
    }
    if (node->ref > 1) {
      if (st_add_direct(table, (char *)node, (char *)*ptr) == ST_OUT_OF_MEM)
        return (CUDD_OUT_OF_MEM);
    }
    return (val);
  }
  if (dd->perm[node->index] > dd->perm[i]) {
    *ptr = node;
    tval = ddDagInt(cuddT(node));
    eval = ddDagInt(Cudd_Regular(cuddE(node)));
    if (node->ref > 1) {
      if (st_add_direct(table, (char *)node, (char *)node) == ST_OUT_OF_MEM)
        return (CUDD_OUT_OF_MEM);
    }
    val = 1 + tval + eval;
    return (val);
  }
  tval = cuddEstimateCofactor(dd, table, cuddT(node), i, phase, &ptrT);
  eval = cuddEstimateCofactor(dd, table, Cudd_Regular(cuddE(node)), i, phase,
                              &ptrE);
  ptrE = Cudd_NotCond(ptrE, Cudd_IsComplement(cuddE(node)));
  if (ptrT == ptrE) { /* recombination */
    *ptr = ptrT;
    val = tval;
    if (node->ref > 1) {
      if (st_add_direct(table, (char *)node, (char *)*ptr) == ST_OUT_OF_MEM)
        return (CUDD_OUT_OF_MEM);
    }
  } else if ((ptrT != cuddT(node) || ptrE != cuddE(node)) &&
             (*ptr = cuddUniqueLookup(dd, node->index, ptrT, ptrE)) != NULL) {
    if (Cudd_IsComplement((*ptr)->next)) {
      val = 0;
    } else {
      val = 1 + tval + eval;
    }
    if (node->ref > 1) {
      if (st_add_direct(table, (char *)node, (char *)*ptr) == ST_OUT_OF_MEM)
        return (CUDD_OUT_OF_MEM);
    }
  } else {
    *ptr = node;
    val = 1 + tval + eval;
  }
  return (val);

} /* end of cuddEstimateCofactor */

/**Function********************************************************************

  Synopsis    [Checks the unique table for the existence of an internal node.]

  Description [Checks the unique table for the existence of an internal
  node. Returns a pointer to the node if it is in the table; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [cuddUniqueInter]

******************************************************************************/
static DdNode *cuddUniqueLookup(DdManager *unique, int index, DdNode *T,
                                DdNode *E) {
  int posn;
  unsigned int level;
  DdNodePtr *nodelist;
  DdNode *looking;
  DdSubtable *subtable;

  if (index >= unique->size) {
    return (NULL);
  }

  level = unique->perm[index];
  subtable = &(unique->subtables[level]);

#ifdef DD_DEBUG
  assert(level < (unsigned)cuddI(unique, T->index));
  assert(level < (unsigned)cuddI(unique, Cudd_Regular(E)->index));
#endif

  posn = ddHash(T, E, subtable->shift);
  nodelist = subtable->nodelist;
  looking = nodelist[posn];

  while (T < cuddT(looking)) {
    looking = Cudd_Regular(looking->next);
  }
  while (T == cuddT(looking) && E < cuddE(looking)) {
    looking = Cudd_Regular(looking->next);
  }
  if (cuddT(looking) == T && cuddE(looking) == E) {
    return (looking);
  }

  return (NULL);

} /* end of cuddUniqueLookup */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CofactorEstimateSimple.]

  Description [Performs the recursive step of Cudd_CofactorEstimateSimple.
  Returns an estimate of the number of nodes in the DD of the positive
  cofactor of node. Uses the least significant bit of the next field as
  visited flag. node is supposed to be regular; the invariant is maintained
  by this procedure.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddEstimateCofactorSimple(DdNode *node, int i) {
  int tval, eval;

  if (Cudd_IsComplement(node->next)) {
    return (0);
  }
  node->next = Cudd_Not(node->next);
  if (cuddIsConstant(node)) {
    return (1);
  }
  tval = cuddEstimateCofactorSimple(cuddT(node), i);
  if ((int)node->index == i)
    return (tval);
  eval = cuddEstimateCofactorSimple(Cudd_Regular(cuddE(node)), i);
  return (1 + tval + eval);

} /* end of cuddEstimateCofactorSimple */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountMinterm.]

  Description [Performs the recursive step of Cudd_CountMinterm.
  It is based on the following identity. Let |f| be the
  number of minterms of f. Then:
  <xmp>
    |f| = (|f0|+|f1|)/2
  </xmp>
  where f0 and f1 are the two cofactors of f.  Does not use the
  identity |f'| = max - |f|, to minimize loss of accuracy due to
  roundoff.  Returns the number of minterms of the function rooted at
  node.]

  SideEffects [None]

******************************************************************************/
static double ddCountMintermAux(DdNode *node, double max, DdHashTable *table) {
  DdNode *N, *Nt, *Ne;
  double min, minT, minE;
  DdNode *res;

  N = Cudd_Regular(node);

  if (cuddIsConstant(N)) {
    if (node == background || node == zero) {
      return (0.0);
    } else {
      return (max);
    }
  }
  if (N->ref != 1 && (res = cuddHashTableLookup1(table, node)) != NULL) {
    min = cuddV(res);
    if (res->ref == 0) {
      table->manager->dead++;
      table->manager->constants.dead++;
    }
    return (min);
  }

  Nt = cuddT(N);
  Ne = cuddE(N);
  if (Cudd_IsComplement(node)) {
    Nt = Cudd_Not(Nt);
    Ne = Cudd_Not(Ne);
  }

  minT = ddCountMintermAux(Nt, max, table);
  if (minT == (double)CUDD_OUT_OF_MEM)
    return ((double)CUDD_OUT_OF_MEM);
  minT *= 0.5;
  minE = ddCountMintermAux(Ne, max, table);
  if (minE == (double)CUDD_OUT_OF_MEM)
    return ((double)CUDD_OUT_OF_MEM);
  minE *= 0.5;
  min = minT + minE;

  if (N->ref != 1) {
    ptrint fanout = (ptrint)N->ref;
    cuddSatDec(fanout);
    res = cuddUniqueConst(table->manager, min);
    if (!cuddHashTableInsert1(table, node, res, fanout)) {
      cuddRef(res);
      Cudd_RecursiveDeref(table->manager, res);
      return ((double)CUDD_OUT_OF_MEM);
    }
  }

  return (min);

} /* end of ddCountMintermAux */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountPath.]

  Description [Performs the recursive step of Cudd_CountPath.
  It is based on the following identity. Let |f| be the
  number of paths of f. Then:
  <xmp>
    |f| = |f0|+|f1|
  </xmp>
  where f0 and f1 are the two cofactors of f.  Uses the
  identity |f'| = |f|, to improve the utilization of the (local) cache.
  Returns the number of paths of the function rooted at node.]

  SideEffects [None]

******************************************************************************/
static double ddCountPathAux(DdNode *node, st_table *table) {

  DdNode *Nv, *Nnv;
  double paths, *ppaths, paths1, paths2;
  double *dummy;

  if (cuddIsConstant(node)) {
    return (1.0);
  }
  if (st_lookup(table, node, &dummy)) {
    paths = *dummy;
    return (paths);
  }

  Nv = cuddT(node);
  Nnv = cuddE(node);

  paths1 = ddCountPathAux(Nv, table);
  if (paths1 == (double)CUDD_OUT_OF_MEM)
    return ((double)CUDD_OUT_OF_MEM);
  paths2 = ddCountPathAux(Cudd_Regular(Nnv), table);
  if (paths2 == (double)CUDD_OUT_OF_MEM)
    return ((double)CUDD_OUT_OF_MEM);
  paths = paths1 + paths2;

  ppaths = ALLOC(double, 1);
  if (ppaths == NULL) {
    return ((double)CUDD_OUT_OF_MEM);
  }

  *ppaths = paths;

  if (st_add_direct(table, (char *)node, (char *)ppaths) == ST_OUT_OF_MEM) {
    FREE(ppaths);
    return ((double)CUDD_OUT_OF_MEM);
  }
  return (paths);

} /* end of ddCountPathAux */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_EpdCountMinterm.]

  Description [Performs the recursive step of Cudd_EpdCountMinterm.
  It is based on the following identity. Let |f| be the
  number of minterms of f. Then:
  <xmp>
    |f| = (|f0|+|f1|)/2
  </xmp>
  where f0 and f1 are the two cofactors of f.  Does not use the
  identity |f'| = max - |f|, to minimize loss of accuracy due to
  roundoff.  Returns the number of minterms of the function rooted at
  node.]

  SideEffects [None]

******************************************************************************/
static int ddEpdCountMintermAux(DdNode *node, EpDouble *max, EpDouble *epd,
                                st_table *table) {
  DdNode *Nt, *Ne;
  EpDouble *min, minT, minE;
  EpDouble *res;
  int status;

  /* node is assumed to be regular */
  if (cuddIsConstant(node)) {
    if (node == background || node == zero) {
      EpdMakeZero(epd, 0);
    } else {
      EpdCopy(max, epd);
    }
    return (0);
  }
  if (node->ref != 1 && st_lookup(table, node, &res)) {
    EpdCopy(res, epd);
    return (0);
  }

  Nt = cuddT(node);
  Ne = cuddE(node);

  status = ddEpdCountMintermAux(Nt, max, &minT, table);
  if (status == CUDD_OUT_OF_MEM)
    return (CUDD_OUT_OF_MEM);
  EpdMultiply(&minT, (double)0.5);
  status = ddEpdCountMintermAux(Cudd_Regular(Ne), max, &minE, table);
  if (status == CUDD_OUT_OF_MEM)
    return (CUDD_OUT_OF_MEM);
  if (Cudd_IsComplement(Ne)) {
    EpdSubtract3(max, &minE, epd);
    EpdCopy(epd, &minE);
  }
  EpdMultiply(&minE, (double)0.5);
  EpdAdd3(&minT, &minE, epd);

  if (node->ref > 1) {
    min = EpdAlloc();
    if (!min)
      return (CUDD_OUT_OF_MEM);
    EpdCopy(epd, min);
    if (st_insert(table, (char *)node, (char *)min) == ST_OUT_OF_MEM) {
      EpdFree(min);
      return (CUDD_OUT_OF_MEM);
    }
  }

  return (0);

} /* end of ddEpdCountMintermAux */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountPathsToNonZero.]

  Description [Performs the recursive step of Cudd_CountPathsToNonZero.
  It is based on the following identity. Let |f| be the
  number of paths of f. Then:
  <xmp>
    |f| = |f0|+|f1|
  </xmp>
  where f0 and f1 are the two cofactors of f.  Returns the number of
  paths of the function rooted at node.]

  SideEffects [None]

******************************************************************************/
static double ddCountPathsToNonZero(DdNode *N, st_table *table) {

  DdNode *node, *Nt, *Ne;
  double paths, *ppaths, paths1, paths2;
  double *dummy;

  node = Cudd_Regular(N);
  if (cuddIsConstant(node)) {
    return ((double)!(Cudd_IsComplement(N) || cuddV(node) == DD_ZERO_VAL));
  }
  if (st_lookup(table, N, &dummy)) {
    paths = *dummy;
    return (paths);
  }

  Nt = cuddT(node);
  Ne = cuddE(node);
  if (node != N) {
    Nt = Cudd_Not(Nt);
    Ne = Cudd_Not(Ne);
  }

  paths1 = ddCountPathsToNonZero(Nt, table);
  if (paths1 == (double)CUDD_OUT_OF_MEM)
    return ((double)CUDD_OUT_OF_MEM);
  paths2 = ddCountPathsToNonZero(Ne, table);
  if (paths2 == (double)CUDD_OUT_OF_MEM)
    return ((double)CUDD_OUT_OF_MEM);
  paths = paths1 + paths2;

  ppaths = ALLOC(double, 1);
  if (ppaths == NULL) {
    return ((double)CUDD_OUT_OF_MEM);
  }

  *ppaths = paths;

  if (st_add_direct(table, (char *)N, (char *)ppaths) == ST_OUT_OF_MEM) {
    FREE(ppaths);
    return ((double)CUDD_OUT_OF_MEM);
  }
  return (paths);

} /* end of ddCountPathsToNonZero */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_Support.]

  Description [Performs the recursive step of Cudd_Support. Performs a
  DFS from f. The support is accumulated in supp as a side effect. Uses
  the LSB of the then pointer as visited flag.]

  SideEffects [None]

  SeeAlso     [ddClearFlag]

******************************************************************************/
static void ddSupportStep(DdNode *f, int *support) {
  if (cuddIsConstant(f) || Cudd_IsComplement(f->next))
    return;

  support[f->index] = 1;
  ddSupportStep(cuddT(f), support);
  ddSupportStep(Cudd_Regular(cuddE(f)), support);
  /* Mark as visited. */
  f->next = Cudd_Complement(f->next);

} /* end of ddSupportStep */

/**Function********************************************************************

  Synopsis    [Performs a DFS from f, clearing the LSB of the next
  pointers.]

  Description []

  SideEffects [None]

  SeeAlso     [ddSupportStep ddFindSupport ddLeavesInt ddDagInt]

******************************************************************************/
static void ddClearFlag(DdNode *f) {
  if (!Cudd_IsComplement(f->next)) {
    return;
  }
  /* Clear visited flag. */
  f->next = Cudd_Regular(f->next);
  if (cuddIsConstant(f)) {
    return;
  }
  ddClearFlag(cuddT(f));
  ddClearFlag(Cudd_Regular(cuddE(f)));
  return;

} /* end of ddClearFlag */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_CountLeaves.]

  Description [Performs the recursive step of Cudd_CountLeaves. Returns
  the number of leaves in the DD rooted at n.]

  SideEffects [None]

  SeeAlso     [Cudd_CountLeaves]

******************************************************************************/
static int ddLeavesInt(DdNode *n) {
  int tval, eval;

  if (Cudd_IsComplement(n->next)) {
    return (0);
  }
  n->next = Cudd_Not(n->next);
  if (cuddIsConstant(n)) {
    return (1);
  }
  tval = ddLeavesInt(cuddT(n));
  eval = ddLeavesInt(Cudd_Regular(cuddE(n)));
  return (tval + eval);

} /* end of ddLeavesInt */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_bddPickArbitraryMinterms.]

  Description [Performs the recursive step of Cudd_bddPickArbitraryMinterms.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [none]

  SeeAlso [Cudd_bddPickArbitraryMinterms]

******************************************************************************/
static int ddPickArbitraryMinterms(DdManager *dd, DdNode *node, int nvars,
                                   int nminterms, char **string) {
  DdNode *N, *T, *E;
  DdNode *one, *bzero;
  int i, t, result;
  double min1, min2;

  if (string == NULL || node == NULL)
    return (0);

  /* The constant 0 function has no on-set cubes. */
  one = DD_ONE(dd);
  bzero = Cudd_Not(one);
  if (nminterms == 0 || node == bzero)
    return (1);
  if (node == one) {
    return (1);
  }

  N = Cudd_Regular(node);
  T = cuddT(N);
  E = cuddE(N);
  if (Cudd_IsComplement(node)) {
    T = Cudd_Not(T);
    E = Cudd_Not(E);
  }

  min1 = Cudd_CountMinterm(dd, T, nvars) / 2.0;
  if (min1 == (double)CUDD_OUT_OF_MEM)
    return (0);
  min2 = Cudd_CountMinterm(dd, E, nvars) / 2.0;
  if (min2 == (double)CUDD_OUT_OF_MEM)
    return (0);

  t = (int)((double)nminterms * min1 / (min1 + min2) + 0.5);
  for (i = 0; i < t; i++)
    string[i][N->index] = '1';
  for (i = t; i < nminterms; i++)
    string[i][N->index] = '0';

  result = ddPickArbitraryMinterms(dd, T, nvars, t, &string[0]);
  if (result == 0)
    return (0);
  result = ddPickArbitraryMinterms(dd, E, nvars, nminterms - t, &string[t]);
  return (result);

} /* end of ddPickArbitraryMinterms */

/* end of ddPickRepresentativeCube */

/* end of ddEpdFree */

/**Function********************************************************************

  Synopsis [Recursively find the support of f.]

  Description [Recursively find the support of f.  This function uses the
  LSB of the next field of the nodes of f as visited flag.  It also uses the
  LSB of the next field of the variables as flag to remember whether a
  certain index has already been seen.  Finally, it uses the manager stack
  to record all seen indices.]

  SideEffects [The stack pointer SP is modified by side-effect.  The next
  fields are changed and need to be reset.]

******************************************************************************/
static void ddFindSupport(DdManager *dd, DdNode *f, int *SP) {
  int index;
  DdNode *var;

  if (cuddIsConstant(f) || Cudd_IsComplement(f->next)) {
    return;
  }

  index = f->index;
  var = dd->vars[index];
  /* It is possible that var is embedded in f.  That causes no problem,
  ** though, because if we see it after encountering another node with
  ** the same index, nothing is supposed to happen.
  */
  if (!Cudd_IsComplement(var->next)) {
    var->next = Cudd_Complement(var->next);
    dd->stack[*SP] = (DdNode *)(ptrint)index;
    (*SP)++;
  }
  ddFindSupport(dd, cuddT(f), SP);
  ddFindSupport(dd, Cudd_Regular(cuddE(f)), SP);
  /* Mark as visited. */
  f->next = Cudd_Complement(f->next);

} /* end of ddFindSupport */

/* end of ddClearVars */

/* end of indexCompare */
/**CFile***********************************************************************

  FileName    [cuddWindow.c]

  PackageName [cudd]

  Synopsis    [Functions for variable reordering by window permutation.]

  Description [Internal procedures included in this module:
                <ul>
                <li> cuddWindowReorder()
                </ul>
        Static procedures included in this module:
                <ul>
                <li> ddWindow2()
                <li> ddWindowConv2()
                <li> ddPermuteWindow3()
                <li> ddWindow3()
                <li> ddWindowConv3()
                <li> ddPermuteWindow4()
                <li> ddWindow4()
                <li> ddWindowConv4()
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
// static char rcsid[] DD_UNUSED = "$Id: cuddWindow.c,v 1.15 2012/02/05 01:07:19
// fabio Exp $"; #endif

#ifdef DD_STATS
extern int ddTotalNumberSwapping;
extern int ddTotalNISwaps;
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int ddWindow2(DdManager *table, int low, int high);
static int ddWindowConv2(DdManager *table, int low, int high);
static int ddPermuteWindow3(DdManager *table, int x);
static int ddWindow3(DdManager *table, int low, int high);
static int ddWindowConv3(DdManager *table, int low, int high);
static int ddPermuteWindow4(DdManager *table, int w);
static int ddWindow4(DdManager *table, int low, int high);
static int ddWindowConv4(DdManager *table, int low, int high);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Reorders by applying the method of the sliding window.]

  Description [Reorders by applying the method of the sliding window.
  Tries all possible permutations to the variables in a window that
  slides from low to high. The size of the window is determined by
  submethod.  Assumes that no dead nodes are present.  Returns 1 in
  case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
int cuddWindowReorder(
    DdManager *table /* DD table */, int low /* lowest index to reorder */,
    int high /* highest index to reorder */,
    Cudd_ReorderingType submethod /* window reordering option */) {

  int res;
#ifdef DD_DEBUG
  int supposedOpt;
#endif

  switch (submethod) {
  case CUDD_REORDER_WINDOW2:
    res = ddWindow2(table, low, high);
    break;
  case CUDD_REORDER_WINDOW3:
    res = ddWindow3(table, low, high);
    break;
  case CUDD_REORDER_WINDOW4:
    res = ddWindow4(table, low, high);
    break;
  case CUDD_REORDER_WINDOW2_CONV:
    res = ddWindowConv2(table, low, high);
    break;
  case CUDD_REORDER_WINDOW3_CONV:
    res = ddWindowConv3(table, low, high);
#ifdef DD_DEBUG
    supposedOpt = table->keys - table->isolated;
    res = ddWindow3(table, low, high);
    if (table->keys - table->isolated != (unsigned)supposedOpt) {
      (void)fprintf(table->err, "Convergence failed! (%d != %d)\n",
                    table->keys - table->isolated, supposedOpt);
    }
#endif
    break;
  case CUDD_REORDER_WINDOW4_CONV:
    res = ddWindowConv4(table, low, high);
#ifdef DD_DEBUG
    supposedOpt = table->keys - table->isolated;
    res = ddWindow4(table, low, high);
    if (table->keys - table->isolated != (unsigned)supposedOpt) {
      (void)fprintf(table->err, "Convergence failed! (%d != %d)\n",
                    table->keys - table->isolated, supposedOpt);
    }
#endif
    break;
  default:
    return (0);
  }

  return (res);

} /* end of cuddWindowReorder */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Reorders by applying a sliding window of width 2.]

  Description [Reorders by applying a sliding window of width 2.
  Tries both permutations of the variables in a window
  that slides from low to high.  Assumes that no dead nodes are
  present.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddWindow2(DdManager *table, int low, int high) {

  int x;
  int res;
  int size;

#ifdef DD_DEBUG
  assert(low >= 0 && high < table->size);
#endif

  if (high - low < 1)
    return (0);

  res = table->keys - table->isolated;
  for (x = low; x < high; x++) {
    size = res;
    res = cuddSwapInPlace(table, x, x + 1);
    if (res == 0)
      return (0);
    if (res >= size) { /* no improvement: undo permutation */
      res = cuddSwapInPlace(table, x, x + 1);
      if (res == 0)
        return (0);
    }
#ifdef DD_STATS
    if (res < size) {
      (void)fprintf(table->out, "-");
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

  return (1);

} /* end of ddWindow2 */

/**Function********************************************************************

  Synopsis    [Reorders by repeatedly applying a sliding window of width 2.]

  Description [Reorders by repeatedly applying a sliding window of width
  2. Tries both permutations of the variables in a window
  that slides from low to high.  Assumes that no dead nodes are
  present.  Uses an event-driven approach to determine convergence.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddWindowConv2(DdManager *table, int low, int high) {
  int x;
  int res;
  int nwin;
  int newevent;
  int *events;
  int size;

#ifdef DD_DEBUG
  assert(low >= 0 && high < table->size);
#endif

  if (high - low < 1)
    return (ddWindowConv2(table, low, high));

  nwin = high - low;
  events = ALLOC(int, nwin);
  if (events == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (x = 0; x < nwin; x++) {
    events[x] = 1;
  }

  res = table->keys - table->isolated;
  do {
    newevent = 0;
    for (x = 0; x < nwin; x++) {
      if (events[x]) {
        size = res;
        res = cuddSwapInPlace(table, x + low, x + low + 1);
        if (res == 0) {
          FREE(events);
          return (0);
        }
        if (res >= size) { /* no improvement: undo permutation */
          res = cuddSwapInPlace(table, x + low, x + low + 1);
          if (res == 0) {
            FREE(events);
            return (0);
          }
        }
        if (res < size) {
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 0)
            events[x - 1] = 1;
          newevent = 1;
        }
        events[x] = 0;
#ifdef DD_STATS
        if (res < size) {
          (void)fprintf(table->out, "-");
        } else {
          (void)fprintf(table->out, "=");
        }
        fflush(table->out);
#endif
      }
    }
#ifdef DD_STATS
    if (newevent) {
      (void)fprintf(table->out, "|");
      fflush(table->out);
    }
#endif
  } while (newevent);

  FREE(events);

  return (1);

} /* end of ddWindowConv3 */

/**Function********************************************************************

  Synopsis [Tries all the permutations of the three variables between
  x and x+2 and retains the best.]

  Description [Tries all the permutations of the three variables between
  x and x+2 and retains the best. Assumes that no dead nodes are
  present.  Returns the index of the best permutation (1-6) in case of
  success; 0 otherwise.Assumes that no dead nodes are present.  Returns
  the index of the best permutation (1-6) in case of success; 0
  otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddPermuteWindow3(DdManager *table, int x) {
  int y, z;
  int size, sizeNew;
  int best;

#ifdef DD_DEBUG
  assert(table->dead == 0);
  assert(x + 2 < table->size);
#endif

  size = table->keys - table->isolated;
  y = x + 1;
  z = y + 1;

/* The permutation pattern is:
** (x,y)(y,z)
** repeated three times to get all 3! = 6 permutations.
*/
#define ABC 1
  best = ABC;

#define BAC 2
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = BAC;
    size = sizeNew;
  }
#define BCA 3
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = BCA;
    size = sizeNew;
  }
#define CBA 4
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = CBA;
    size = sizeNew;
  }
#define CAB 5
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = CAB;
    size = sizeNew;
  }
#define ACB 6
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = ACB;
    size = sizeNew;
  }

  /* Now take the shortest route to the best permuytation.
  ** The initial permutation is ACB.
  */
  switch (best) {
  case BCA:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case CBA:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case ABC:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case ACB:
    break;
  case BAC:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case CAB:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
    break;
  default:
    return (0);
  }

#ifdef DD_DEBUG
  assert(table->keys - table->isolated == (unsigned)size);
#endif

  return (best);

} /* end of ddPermuteWindow3 */

/**Function********************************************************************

  Synopsis    [Reorders by applying a sliding window of width 3.]

  Description [Reorders by applying a sliding window of width 3.
  Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddWindow3(DdManager *table, int low, int high) {

  int x;
  int res;

#ifdef DD_DEBUG
  assert(low >= 0 && high < table->size);
#endif

  if (high - low < 2)
    return (ddWindow2(table, low, high));

  for (x = low; x + 1 < high; x++) {
    res = ddPermuteWindow3(table, x);
    if (res == 0)
      return (0);
#ifdef DD_STATS
    if (res == ABC) {
      (void)fprintf(table->out, "=");
    } else {
      (void)fprintf(table->out, "-");
    }
    fflush(table->out);
#endif
  }

  return (1);

} /* end of ddWindow3 */

/**Function********************************************************************

  Synopsis    [Reorders by repeatedly applying a sliding window of width 3.]

  Description [Reorders by repeatedly applying a sliding window of width
  3. Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Uses an event-driven approach to determine convergence.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddWindowConv3(DdManager *table, int low, int high) {
  int x;
  int res;
  int nwin;
  int newevent;
  int *events;

#ifdef DD_DEBUG
  assert(low >= 0 && high < table->size);
#endif

  if (high - low < 2)
    return (ddWindowConv2(table, low, high));

  nwin = high - low - 1;
  events = ALLOC(int, nwin);
  if (events == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (x = 0; x < nwin; x++) {
    events[x] = 1;
  }

  do {
    newevent = 0;
    for (x = 0; x < nwin; x++) {
      if (events[x]) {
        res = ddPermuteWindow3(table, x + low);
        switch (res) {
        case ABC:
          break;
        case BAC:
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 1)
            events[x - 2] = 1;
          newevent = 1;
          break;
        case BCA:
        case CBA:
        case CAB:
          if (x < nwin - 2)
            events[x + 2] = 1;
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 0)
            events[x - 1] = 1;
          if (x > 1)
            events[x - 2] = 1;
          newevent = 1;
          break;
        case ACB:
          if (x < nwin - 2)
            events[x + 2] = 1;
          if (x > 0)
            events[x - 1] = 1;
          newevent = 1;
          break;
        default:
          FREE(events);
          return (0);
        }
        events[x] = 0;
#ifdef DD_STATS
        if (res == ABC) {
          (void)fprintf(table->out, "=");
        } else {
          (void)fprintf(table->out, "-");
        }
        fflush(table->out);
#endif
      }
    }
#ifdef DD_STATS
    if (newevent) {
      (void)fprintf(table->out, "|");
      fflush(table->out);
    }
#endif
  } while (newevent);

  FREE(events);

  return (1);

} /* end of ddWindowConv3 */

/**Function********************************************************************

  Synopsis [Tries all the permutations of the four variables between w
  and w+3 and retains the best.]

  Description [Tries all the permutations of the four variables between
  w and w+3 and retains the best. Assumes that no dead nodes are
  present.  Returns the index of the best permutation (1-24) in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddPermuteWindow4(DdManager *table, int w) {
  int x, y, z;
  int size, sizeNew;
  int best;

#ifdef DD_DEBUG
  assert(table->dead == 0);
  assert(w + 3 < table->size);
#endif

  size = table->keys - table->isolated;
  x = w + 1;
  y = x + 1;
  z = y + 1;

/* The permutation pattern is:
 * (w,x)(y,z)(w,x)(x,y)
 * (y,z)(w,x)(y,z)(x,y)
 * repeated three times to get all 4! = 24 permutations.
 * This gives a hamiltonian circuit of Cayley's graph.
 * The codes to the permutation are assigned in topological order.
 * The permutations at lower distance from the final permutation are
 * assigned lower codes. This way we can choose, between
 * permutations that give the same size, one that requires the minimum
 * number of swaps from the final permutation of the hamiltonian circuit.
 * There is an exception to this rule: ABCD is given Code 1, to
 * avoid oscillation when convergence is sought.
 */
#define ABCD 1
  best = ABCD;

#define BACD 7
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = BACD;
    size = sizeNew;
  }
#define BADC 13
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = BADC;
    size = sizeNew;
  }
#define ABDC 8
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size || (sizeNew == size && ABDC < best)) {
    if (sizeNew == 0)
      return (0);
    best = ABDC;
    size = sizeNew;
  }
#define ADBC 14
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = ADBC;
    size = sizeNew;
  }
#define ADCB 9
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && ADCB < best)) {
    if (sizeNew == 0)
      return (0);
    best = ADCB;
    size = sizeNew;
  }
#define DACB 15
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = DACB;
    size = sizeNew;
  }
#define DABC 20
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = DABC;
    size = sizeNew;
  }
#define DBAC 23
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = DBAC;
    size = sizeNew;
  }
#define BDAC 19
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size || (sizeNew == size && BDAC < best)) {
    if (sizeNew == 0)
      return (0);
    best = BDAC;
    size = sizeNew;
  }
#define BDCA 21
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && BDCA < best)) {
    if (sizeNew == 0)
      return (0);
    best = BDCA;
    size = sizeNew;
  }
#define DBCA 24
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size) {
    if (sizeNew == 0)
      return (0);
    best = DBCA;
    size = sizeNew;
  }
#define DCBA 22
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size || (sizeNew == size && DCBA < best)) {
    if (sizeNew == 0)
      return (0);
    best = DCBA;
    size = sizeNew;
  }
#define DCAB 18
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && DCAB < best)) {
    if (sizeNew == 0)
      return (0);
    best = DCAB;
    size = sizeNew;
  }
#define CDAB 12
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size || (sizeNew == size && CDAB < best)) {
    if (sizeNew == 0)
      return (0);
    best = CDAB;
    size = sizeNew;
  }
#define CDBA 17
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && CDBA < best)) {
    if (sizeNew == 0)
      return (0);
    best = CDBA;
    size = sizeNew;
  }
#define CBDA 11
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size || (sizeNew == size && CBDA < best)) {
    if (sizeNew == 0)
      return (0);
    best = CBDA;
    size = sizeNew;
  }
#define BCDA 16
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size || (sizeNew == size && BCDA < best)) {
    if (sizeNew == 0)
      return (0);
    best = BCDA;
    size = sizeNew;
  }
#define BCAD 10
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && BCAD < best)) {
    if (sizeNew == 0)
      return (0);
    best = BCAD;
    size = sizeNew;
  }
#define CBAD 5
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size || (sizeNew == size && CBAD < best)) {
    if (sizeNew == 0)
      return (0);
    best = CBAD;
    size = sizeNew;
  }
#define CABD 3
  sizeNew = cuddSwapInPlace(table, x, y);
  if (sizeNew < size || (sizeNew == size && CABD < best)) {
    if (sizeNew == 0)
      return (0);
    best = CABD;
    size = sizeNew;
  }
#define CADB 6
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && CADB < best)) {
    if (sizeNew == 0)
      return (0);
    best = CADB;
    size = sizeNew;
  }
#define ACDB 4
  sizeNew = cuddSwapInPlace(table, w, x);
  if (sizeNew < size || (sizeNew == size && ACDB < best)) {
    if (sizeNew == 0)
      return (0);
    best = ACDB;
    size = sizeNew;
  }
#define ACBD 2
  sizeNew = cuddSwapInPlace(table, y, z);
  if (sizeNew < size || (sizeNew == size && ACBD < best)) {
    if (sizeNew == 0)
      return (0);
    best = ACBD;
    size = sizeNew;
  }

  /* Now take the shortest route to the best permutation.
  ** The initial permutation is ACBD.
  */
  switch (best) {
  case DBCA:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case BDCA:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case CDBA:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
  case ADBC:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case ABDC:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case ACDB:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case ACBD:
    break;
  case DCBA:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case BCDA:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case CBDA:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
    if (!cuddSwapInPlace(table, x, y))
      return (0);
    if (!cuddSwapInPlace(table, y, z))
      return (0);
    break;
  case DBAC:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case DCAB:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
  case DACB:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case BACD:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case CABD:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
    break;
  case DABC:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case BADC:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case CADB:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
    if (!cuddSwapInPlace(table, y, z))
      return (0);
    break;
  case BDAC:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case CDAB:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
  case ADCB:
    if (!cuddSwapInPlace(table, y, z))
      return (0);
  case ABCD:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
    break;
  case BCAD:
    if (!cuddSwapInPlace(table, x, y))
      return (0);
  case CBAD:
    if (!cuddSwapInPlace(table, w, x))
      return (0);
    if (!cuddSwapInPlace(table, x, y))
      return (0);
    break;
  default:
    return (0);
  }

#ifdef DD_DEBUG
  assert(table->keys - table->isolated == (unsigned)size);
#endif

  return (best);

} /* end of ddPermuteWindow4 */

/**Function********************************************************************

  Synopsis    [Reorders by applying a sliding window of width 4.]

  Description [Reorders by applying a sliding window of width 4.
  Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddWindow4(DdManager *table, int low, int high) {

  int w;
  int res;

#ifdef DD_DEBUG
  assert(low >= 0 && high < table->size);
#endif

  if (high - low < 3)
    return (ddWindow3(table, low, high));

  for (w = low; w + 2 < high; w++) {
    res = ddPermuteWindow4(table, w);
    if (res == 0)
      return (0);
#ifdef DD_STATS
    if (res == ABCD) {
      (void)fprintf(table->out, "=");
    } else {
      (void)fprintf(table->out, "-");
    }
    fflush(table->out);
#endif
  }

  return (1);

} /* end of ddWindow4 */

/**Function********************************************************************

  Synopsis    [Reorders by repeatedly applying a sliding window of width 4.]

  Description [Reorders by repeatedly applying a sliding window of width
  4. Tries all possible permutations to the variables in a
  window that slides from low to high.  Assumes that no dead nodes are
  present.  Uses an event-driven approach to determine convergence.
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int ddWindowConv4(DdManager *table, int low, int high) {
  int x;
  int res;
  int nwin;
  int newevent;
  int *events;

#ifdef DD_DEBUG
  assert(low >= 0 && high < table->size);
#endif

  if (high - low < 3)
    return (ddWindowConv3(table, low, high));

  nwin = high - low - 2;
  events = ALLOC(int, nwin);
  if (events == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (x = 0; x < nwin; x++) {
    events[x] = 1;
  }

  do {
    newevent = 0;
    for (x = 0; x < nwin; x++) {
      if (events[x]) {
        res = ddPermuteWindow4(table, x + low);
        switch (res) {
        case ABCD:
          break;
        case BACD:
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 2)
            events[x - 3] = 1;
          newevent = 1;
          break;
        case BADC:
          if (x < nwin - 3)
            events[x + 3] = 1;
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 0)
            events[x - 1] = 1;
          if (x > 2)
            events[x - 3] = 1;
          newevent = 1;
          break;
        case ABDC:
          if (x < nwin - 3)
            events[x + 3] = 1;
          if (x > 0)
            events[x - 1] = 1;
          newevent = 1;
          break;
        case ADBC:
        case ADCB:
        case ACDB:
          if (x < nwin - 3)
            events[x + 3] = 1;
          if (x < nwin - 2)
            events[x + 2] = 1;
          if (x > 0)
            events[x - 1] = 1;
          if (x > 1)
            events[x - 2] = 1;
          newevent = 1;
          break;
        case DACB:
        case DABC:
        case DBAC:
        case BDAC:
        case BDCA:
        case DBCA:
        case DCBA:
        case DCAB:
        case CDAB:
        case CDBA:
        case CBDA:
        case BCDA:
        case CADB:
          if (x < nwin - 3)
            events[x + 3] = 1;
          if (x < nwin - 2)
            events[x + 2] = 1;
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 0)
            events[x - 1] = 1;
          if (x > 1)
            events[x - 2] = 1;
          if (x > 2)
            events[x - 3] = 1;
          newevent = 1;
          break;
        case BCAD:
        case CBAD:
        case CABD:
          if (x < nwin - 2)
            events[x + 2] = 1;
          if (x < nwin - 1)
            events[x + 1] = 1;
          if (x > 1)
            events[x - 2] = 1;
          if (x > 2)
            events[x - 3] = 1;
          newevent = 1;
          break;
        case ACBD:
          if (x < nwin - 2)
            events[x + 2] = 1;
          if (x > 1)
            events[x - 2] = 1;
          newevent = 1;
          break;
        default:
          FREE(events);
          return (0);
        }
        events[x] = 0;
#ifdef DD_STATS
        if (res == ABCD) {
          (void)fprintf(table->out, "=");
        } else {
          (void)fprintf(table->out, "-");
        }
        fflush(table->out);
#endif
      }
    }
#ifdef DD_STATS
    if (newevent) {
      (void)fprintf(table->out, "|");
      fflush(table->out);
    }
#endif
  } while (newevent);

  FREE(events);

  return (1);

} /* end of ddWindowConv4 */
/**CFile***********************************************************************

  FileName    [cuddZddFuncs.c]

  PackageName [cudd]

  Synopsis    [Functions to manipulate covers represented as ZDDs.]

  Description [External procedures included in this module:
                    <ul>
                    <li> Cudd_zddProduct();
                    <li> Cudd_zddUnateProduct();
                    <li> Cudd_zddWeakDiv();
                    <li> Cudd_zddWeakDivF();
                    <li> Cudd_zddDivide();
                    <li> Cudd_zddDivideF();
                    <li> Cudd_zddComplement();
                    </ul>
               Internal procedures included in this module:
                    <ul>
                    <li> cuddZddProduct();
                    <li> cuddZddUnateProduct();
                    <li> cuddZddWeakDiv();
                    <li> cuddZddWeakDivF();
                    <li> cuddZddDivide();
                    <li> cuddZddDivideF();
                    <li> cuddZddGetCofactors3()
                    <li> cuddZddGetCofactors2()
                    <li> cuddZddComplement();
                    <li> cuddZddGetPosVarIndex();
                    <li> cuddZddGetNegVarIndex();
                    <li> cuddZddGetPosVarLevel();
                    <li> cuddZddGetNegVarLevel();
                    </ul>
               Static procedures included in this module:
                    <ul>
                    </ul>
              ]

  SeeAlso     []

  Author      [In-Ho Moon]

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
// static char rcsid[] DD_UNUSED = "$Id: cuddZddFuncs.c,v 1.17 2012/02/05
// 01:07:19 fabio Exp $"; #endif

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

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddProduct.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddProduct]

******************************************************************************/
DdNode *cuddZddProduct(DdManager *dd, DdNode *f, DdNode *g) {
  int v, top_f, top_g;
  DdNode *tmp, *term1, *term2, *term3;
  DdNode *f0, *f1, *fd, *g0, *g1, *gd;
  DdNode *R0, *R1, *Rd, *N0, *N1;
  DdNode *r;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = DD_ZERO(dd);
  int flag;
  int pv, nv;

  statLine(dd);
  if (f == zero || g == zero)
    return (zero);
  if (f == one)
    return (g);
  if (g == one)
    return (f);

  top_f = dd->permZ[f->index];
  top_g = dd->permZ[g->index];

  if (top_f > top_g)
    return (cuddZddProduct(dd, g, f));

  /* Check cache */
  r = cuddCacheLookup2Zdd(dd, cuddZddProduct, f, g);
  if (r)
    return (r);

  v = f->index; /* either yi or zi */
  flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
  if (flag == 1)
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  Cudd_Ref(fd);
  flag = cuddZddGetCofactors3(dd, g, v, &g1, &g0, &gd);
  if (flag == 1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    return (NULL);
  }
  Cudd_Ref(g1);
  Cudd_Ref(g0);
  Cudd_Ref(gd);
  pv = cuddZddGetPosVarIndex(dd, v);
  nv = cuddZddGetNegVarIndex(dd, v);

  Rd = cuddZddProduct(dd, fd, gd);
  if (Rd == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, gd);
    return (NULL);
  }
  Cudd_Ref(Rd);

  term1 = cuddZddProduct(dd, f0, g0);
  if (term1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, Rd);
    return (NULL);
  }
  Cudd_Ref(term1);
  term2 = cuddZddProduct(dd, f0, gd);
  if (term2 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, Rd);
    Cudd_RecursiveDerefZdd(dd, term1);
    return (NULL);
  }
  Cudd_Ref(term2);
  term3 = cuddZddProduct(dd, fd, g0);
  if (term3 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, Rd);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    return (NULL);
  }
  Cudd_Ref(term3);
  Cudd_RecursiveDerefZdd(dd, f0);
  Cudd_RecursiveDerefZdd(dd, g0);
  tmp = cuddZddUnion(dd, term1, term2);
  if (tmp == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, Rd);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    Cudd_RecursiveDerefZdd(dd, term3);
    return (NULL);
  }
  Cudd_Ref(tmp);
  Cudd_RecursiveDerefZdd(dd, term1);
  Cudd_RecursiveDerefZdd(dd, term2);
  R0 = cuddZddUnion(dd, tmp, term3);
  if (R0 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, Rd);
    Cudd_RecursiveDerefZdd(dd, term3);
    Cudd_RecursiveDerefZdd(dd, tmp);
    return (NULL);
  }
  Cudd_Ref(R0);
  Cudd_RecursiveDerefZdd(dd, tmp);
  Cudd_RecursiveDerefZdd(dd, term3);
  N0 = cuddZddGetNode(dd, nv, R0, Rd); /* nv = zi */
  if (N0 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, Rd);
    Cudd_RecursiveDerefZdd(dd, R0);
    return (NULL);
  }
  Cudd_Ref(N0);
  Cudd_RecursiveDerefZdd(dd, R0);
  Cudd_RecursiveDerefZdd(dd, Rd);

  term1 = cuddZddProduct(dd, f1, g1);
  if (term1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, N0);
    return (NULL);
  }
  Cudd_Ref(term1);
  term2 = cuddZddProduct(dd, f1, gd);
  if (term2 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, N0);
    Cudd_RecursiveDerefZdd(dd, term1);
    return (NULL);
  }
  Cudd_Ref(term2);
  term3 = cuddZddProduct(dd, fd, g1);
  if (term3 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, gd);
    Cudd_RecursiveDerefZdd(dd, N0);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    return (NULL);
  }
  Cudd_Ref(term3);
  Cudd_RecursiveDerefZdd(dd, f1);
  Cudd_RecursiveDerefZdd(dd, g1);
  Cudd_RecursiveDerefZdd(dd, fd);
  Cudd_RecursiveDerefZdd(dd, gd);
  tmp = cuddZddUnion(dd, term1, term2);
  if (tmp == NULL) {
    Cudd_RecursiveDerefZdd(dd, N0);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    Cudd_RecursiveDerefZdd(dd, term3);
    return (NULL);
  }
  Cudd_Ref(tmp);
  Cudd_RecursiveDerefZdd(dd, term1);
  Cudd_RecursiveDerefZdd(dd, term2);
  R1 = cuddZddUnion(dd, tmp, term3);
  if (R1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, N0);
    Cudd_RecursiveDerefZdd(dd, term3);
    Cudd_RecursiveDerefZdd(dd, tmp);
    return (NULL);
  }
  Cudd_Ref(R1);
  Cudd_RecursiveDerefZdd(dd, tmp);
  Cudd_RecursiveDerefZdd(dd, term3);
  N1 = cuddZddGetNode(dd, pv, R1, N0); /* pv = yi */
  if (N1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, N0);
    Cudd_RecursiveDerefZdd(dd, R1);
    return (NULL);
  }
  Cudd_Ref(N1);
  Cudd_RecursiveDerefZdd(dd, R1);
  Cudd_RecursiveDerefZdd(dd, N0);

  cuddCacheInsert2(dd, cuddZddProduct, f, g, N1);
  Cudd_Deref(N1);
  return (N1);

} /* end of cuddZddProduct */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddUnateProduct.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddUnateProduct]

******************************************************************************/
DdNode *cuddZddUnateProduct(DdManager *dd, DdNode *f, DdNode *g) {
  int v, top_f, top_g;
  DdNode *term1, *term2, *term3, *term4;
  DdNode *sum1, *sum2;
  DdNode *f0, *f1, *g0, *g1;
  DdNode *r;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = DD_ZERO(dd);
  int flag;

  statLine(dd);
  if (f == zero || g == zero)
    return (zero);
  if (f == one)
    return (g);
  if (g == one)
    return (f);

  top_f = dd->permZ[f->index];
  top_g = dd->permZ[g->index];

  if (top_f > top_g)
    return (cuddZddUnateProduct(dd, g, f));

  /* Check cache */
  r = cuddCacheLookup2Zdd(dd, cuddZddUnateProduct, f, g);
  if (r)
    return (r);

  v = f->index; /* either yi or zi */
  flag = cuddZddGetCofactors2(dd, f, v, &f1, &f0);
  if (flag == 1)
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  flag = cuddZddGetCofactors2(dd, g, v, &g1, &g0);
  if (flag == 1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    return (NULL);
  }
  Cudd_Ref(g1);
  Cudd_Ref(g0);

  term1 = cuddZddUnateProduct(dd, f1, g1);
  if (term1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    return (NULL);
  }
  Cudd_Ref(term1);
  term2 = cuddZddUnateProduct(dd, f1, g0);
  if (term2 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, term1);
    return (NULL);
  }
  Cudd_Ref(term2);
  term3 = cuddZddUnateProduct(dd, f0, g1);
  if (term3 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    return (NULL);
  }
  Cudd_Ref(term3);
  term4 = cuddZddUnateProduct(dd, f0, g0);
  if (term4 == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    Cudd_RecursiveDerefZdd(dd, term3);
    return (NULL);
  }
  Cudd_Ref(term4);
  Cudd_RecursiveDerefZdd(dd, f1);
  Cudd_RecursiveDerefZdd(dd, f0);
  Cudd_RecursiveDerefZdd(dd, g1);
  Cudd_RecursiveDerefZdd(dd, g0);
  sum1 = cuddZddUnion(dd, term1, term2);
  if (sum1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, term2);
    Cudd_RecursiveDerefZdd(dd, term3);
    Cudd_RecursiveDerefZdd(dd, term4);
    return (NULL);
  }
  Cudd_Ref(sum1);
  Cudd_RecursiveDerefZdd(dd, term1);
  Cudd_RecursiveDerefZdd(dd, term2);
  sum2 = cuddZddUnion(dd, sum1, term3);
  if (sum2 == NULL) {
    Cudd_RecursiveDerefZdd(dd, term3);
    Cudd_RecursiveDerefZdd(dd, term4);
    Cudd_RecursiveDerefZdd(dd, sum1);
    return (NULL);
  }
  Cudd_Ref(sum2);
  Cudd_RecursiveDerefZdd(dd, sum1);
  Cudd_RecursiveDerefZdd(dd, term3);
  r = cuddZddGetNode(dd, v, sum2, term4);
  if (r == NULL) {
    Cudd_RecursiveDerefZdd(dd, term4);
    Cudd_RecursiveDerefZdd(dd, sum2);
    return (NULL);
  }
  Cudd_Ref(r);
  Cudd_RecursiveDerefZdd(dd, sum2);
  Cudd_RecursiveDerefZdd(dd, term4);

  cuddCacheInsert2(dd, cuddZddUnateProduct, f, g, r);
  Cudd_Deref(r);
  return (r);

} /* end of cuddZddUnateProduct */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddWeakDiv.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddWeakDiv]

******************************************************************************/
DdNode *cuddZddWeakDiv(DdManager *dd, DdNode *f, DdNode *g) {
  int v;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = DD_ZERO(dd);
  DdNode *f0, *f1, *fd, *g0, *g1, *gd;
  DdNode *q, *tmp;
  DdNode *r;
  int flag;

  statLine(dd);
  if (g == one)
    return (f);
  if (f == zero || f == one)
    return (zero);
  if (f == g)
    return (one);

  /* Check cache. */
  r = cuddCacheLookup2Zdd(dd, cuddZddWeakDiv, f, g);
  if (r)
    return (r);

  v = g->index;

  flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
  if (flag == 1)
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  Cudd_Ref(fd);
  flag = cuddZddGetCofactors3(dd, g, v, &g1, &g0, &gd);
  if (flag == 1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    return (NULL);
  }
  Cudd_Ref(g1);
  Cudd_Ref(g0);
  Cudd_Ref(gd);

  q = g;

  if (g0 != zero) {
    q = cuddZddWeakDiv(dd, f0, g0);
    if (q == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, g0);
      Cudd_RecursiveDerefZdd(dd, gd);
      return (NULL);
    }
    Cudd_Ref(q);
  } else
    Cudd_Ref(q);
  Cudd_RecursiveDerefZdd(dd, f0);
  Cudd_RecursiveDerefZdd(dd, g0);

  if (q == zero) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    cuddCacheInsert2(dd, cuddZddWeakDiv, f, g, zero);
    Cudd_Deref(q);
    return (zero);
  }

  if (g1 != zero) {
    Cudd_RecursiveDerefZdd(dd, q);
    tmp = cuddZddWeakDiv(dd, f1, g1);
    if (tmp == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, gd);
      return (NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
    if (q == g)
      q = tmp;
    else {
      q = cuddZddIntersect(dd, q, tmp);
      if (q == NULL) {
        Cudd_RecursiveDerefZdd(dd, fd);
        Cudd_RecursiveDerefZdd(dd, gd);
        return (NULL);
      }
      Cudd_Ref(q);
      Cudd_RecursiveDerefZdd(dd, tmp);
    }
  } else {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
  }

  if (q == zero) {
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    cuddCacheInsert2(dd, cuddZddWeakDiv, f, g, zero);
    Cudd_Deref(q);
    return (zero);
  }

  if (gd != zero) {
    Cudd_RecursiveDerefZdd(dd, q);
    tmp = cuddZddWeakDiv(dd, fd, gd);
    if (tmp == NULL) {
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, gd);
      return (NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    if (q == g)
      q = tmp;
    else {
      q = cuddZddIntersect(dd, q, tmp);
      if (q == NULL) {
        Cudd_RecursiveDerefZdd(dd, tmp);
        return (NULL);
      }
      Cudd_Ref(q);
      Cudd_RecursiveDerefZdd(dd, tmp);
    }
  } else {
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
  }

  cuddCacheInsert2(dd, cuddZddWeakDiv, f, g, q);
  Cudd_Deref(q);
  return (q);

} /* end of cuddZddWeakDiv */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddWeakDivF.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddWeakDivF]

******************************************************************************/
DdNode *cuddZddWeakDivF(DdManager *dd, DdNode *f, DdNode *g) {
  int v, top_f, top_g, vf, vg;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = DD_ZERO(dd);
  DdNode *f0, *f1, *fd, *g0, *g1, *gd;
  DdNode *q, *tmp;
  DdNode *r;
  DdNode *term1, *term0, *termd;
  int flag;
  int pv, nv;

  statLine(dd);
  if (g == one)
    return (f);
  if (f == zero || f == one)
    return (zero);
  if (f == g)
    return (one);

  /* Check cache. */
  r = cuddCacheLookup2Zdd(dd, cuddZddWeakDivF, f, g);
  if (r)
    return (r);

  top_f = dd->permZ[f->index];
  top_g = dd->permZ[g->index];
  vf = top_f >> 1;
  vg = top_g >> 1;
  v = ddMin(top_f, top_g);

  if (v == top_f && vf < vg) {
    v = f->index;
    flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
    if (flag == 1)
      return (NULL);
    Cudd_Ref(f1);
    Cudd_Ref(f0);
    Cudd_Ref(fd);

    pv = cuddZddGetPosVarIndex(dd, v);
    nv = cuddZddGetNegVarIndex(dd, v);

    term1 = cuddZddWeakDivF(dd, f1, g);
    if (term1 == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, fd);
      return (NULL);
    }
    Cudd_Ref(term1);
    Cudd_RecursiveDerefZdd(dd, f1);
    term0 = cuddZddWeakDivF(dd, f0, g);
    if (term0 == NULL) {
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, term1);
      return (NULL);
    }
    Cudd_Ref(term0);
    Cudd_RecursiveDerefZdd(dd, f0);
    termd = cuddZddWeakDivF(dd, fd, g);
    if (termd == NULL) {
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, term1);
      Cudd_RecursiveDerefZdd(dd, term0);
      return (NULL);
    }
    Cudd_Ref(termd);
    Cudd_RecursiveDerefZdd(dd, fd);

    tmp = cuddZddGetNode(dd, nv, term0, termd); /* nv = zi */
    if (tmp == NULL) {
      Cudd_RecursiveDerefZdd(dd, term1);
      Cudd_RecursiveDerefZdd(dd, term0);
      Cudd_RecursiveDerefZdd(dd, termd);
      return (NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, term0);
    Cudd_RecursiveDerefZdd(dd, termd);
    q = cuddZddGetNode(dd, pv, term1, tmp); /* pv = yi */
    if (q == NULL) {
      Cudd_RecursiveDerefZdd(dd, term1);
      Cudd_RecursiveDerefZdd(dd, tmp);
      return (NULL);
    }
    Cudd_Ref(q);
    Cudd_RecursiveDerefZdd(dd, term1);
    Cudd_RecursiveDerefZdd(dd, tmp);

    cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, q);
    Cudd_Deref(q);
    return (q);
  }

  if (v == top_f)
    v = f->index;
  else
    v = g->index;

  flag = cuddZddGetCofactors3(dd, f, v, &f1, &f0, &fd);
  if (flag == 1)
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  Cudd_Ref(fd);
  flag = cuddZddGetCofactors3(dd, g, v, &g1, &g0, &gd);
  if (flag == 1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    return (NULL);
  }
  Cudd_Ref(g1);
  Cudd_Ref(g0);
  Cudd_Ref(gd);

  q = g;

  if (g0 != zero) {
    q = cuddZddWeakDivF(dd, f0, g0);
    if (q == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, g0);
      Cudd_RecursiveDerefZdd(dd, gd);
      return (NULL);
    }
    Cudd_Ref(q);
  } else
    Cudd_Ref(q);
  Cudd_RecursiveDerefZdd(dd, f0);
  Cudd_RecursiveDerefZdd(dd, g0);

  if (q == zero) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, zero);
    Cudd_Deref(q);
    return (zero);
  }

  if (g1 != zero) {
    Cudd_RecursiveDerefZdd(dd, q);
    tmp = cuddZddWeakDivF(dd, f1, g1);
    if (tmp == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, gd);
      return (NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
    if (q == g)
      q = tmp;
    else {
      q = cuddZddIntersect(dd, q, tmp);
      if (q == NULL) {
        Cudd_RecursiveDerefZdd(dd, fd);
        Cudd_RecursiveDerefZdd(dd, gd);
        return (NULL);
      }
      Cudd_Ref(q);
      Cudd_RecursiveDerefZdd(dd, tmp);
    }
  } else {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, g1);
  }

  if (q == zero) {
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, zero);
    Cudd_Deref(q);
    return (zero);
  }

  if (gd != zero) {
    Cudd_RecursiveDerefZdd(dd, q);
    tmp = cuddZddWeakDivF(dd, fd, gd);
    if (tmp == NULL) {
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDerefZdd(dd, gd);
      return (NULL);
    }
    Cudd_Ref(tmp);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
    if (q == g)
      q = tmp;
    else {
      q = cuddZddIntersect(dd, q, tmp);
      if (q == NULL) {
        Cudd_RecursiveDerefZdd(dd, tmp);
        return (NULL);
      }
      Cudd_Ref(q);
      Cudd_RecursiveDerefZdd(dd, tmp);
    }
  } else {
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDerefZdd(dd, gd);
  }

  cuddCacheInsert2(dd, cuddZddWeakDivF, f, g, q);
  Cudd_Deref(q);
  return (q);

} /* end of cuddZddWeakDivF */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddDivide.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddDivide]

******************************************************************************/
DdNode *cuddZddDivide(DdManager *dd, DdNode *f, DdNode *g) {
  int v;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = DD_ZERO(dd);
  DdNode *f0, *f1, *g0, *g1;
  DdNode *q, *r, *tmp;
  int flag;

  statLine(dd);
  if (g == one)
    return (f);
  if (f == zero || f == one)
    return (zero);
  if (f == g)
    return (one);

  /* Check cache. */
  r = cuddCacheLookup2Zdd(dd, cuddZddDivide, f, g);
  if (r)
    return (r);

  v = g->index;

  flag = cuddZddGetCofactors2(dd, f, v, &f1, &f0);
  if (flag == 1)
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  flag = cuddZddGetCofactors2(dd, g, v, &g1, &g0); /* g1 != zero */
  if (flag == 1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    return (NULL);
  }
  Cudd_Ref(g1);
  Cudd_Ref(g0);

  r = cuddZddDivide(dd, f1, g1);
  if (r == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    return (NULL);
  }
  Cudd_Ref(r);

  if (r != zero && g0 != zero) {
    tmp = r;
    q = cuddZddDivide(dd, f0, g0);
    if (q == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, g0);
      return (NULL);
    }
    Cudd_Ref(q);
    r = cuddZddIntersect(dd, r, q);
    if (r == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, g0);
      Cudd_RecursiveDerefZdd(dd, q);
      return (NULL);
    }
    Cudd_Ref(r);
    Cudd_RecursiveDerefZdd(dd, q);
    Cudd_RecursiveDerefZdd(dd, tmp);
  }

  Cudd_RecursiveDerefZdd(dd, f1);
  Cudd_RecursiveDerefZdd(dd, f0);
  Cudd_RecursiveDerefZdd(dd, g1);
  Cudd_RecursiveDerefZdd(dd, g0);

  cuddCacheInsert2(dd, cuddZddDivide, f, g, r);
  Cudd_Deref(r);
  return (r);

} /* end of cuddZddDivide */

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddDivideF.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddDivideF]

******************************************************************************/
DdNode *cuddZddDivideF(DdManager *dd, DdNode *f, DdNode *g) {
  int v;
  DdNode *one = DD_ONE(dd);
  DdNode *zero = DD_ZERO(dd);
  DdNode *f0, *f1, *g0, *g1;
  DdNode *q, *r, *tmp;
  int flag;

  statLine(dd);
  if (g == one)
    return (f);
  if (f == zero || f == one)
    return (zero);
  if (f == g)
    return (one);

  /* Check cache. */
  r = cuddCacheLookup2Zdd(dd, cuddZddDivideF, f, g);
  if (r)
    return (r);

  v = g->index;

  flag = cuddZddGetCofactors2(dd, f, v, &f1, &f0);
  if (flag == 1)
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  flag = cuddZddGetCofactors2(dd, g, v, &g1, &g0); /* g1 != zero */
  if (flag == 1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    return (NULL);
  }
  Cudd_Ref(g1);
  Cudd_Ref(g0);

  r = cuddZddDivideF(dd, f1, g1);
  if (r == NULL) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, g1);
    Cudd_RecursiveDerefZdd(dd, g0);
    return (NULL);
  }
  Cudd_Ref(r);

  if (r != zero && g0 != zero) {
    tmp = r;
    q = cuddZddDivideF(dd, f0, g0);
    if (q == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, g0);
      return (NULL);
    }
    Cudd_Ref(q);
    r = cuddZddIntersect(dd, r, q);
    if (r == NULL) {
      Cudd_RecursiveDerefZdd(dd, f1);
      Cudd_RecursiveDerefZdd(dd, f0);
      Cudd_RecursiveDerefZdd(dd, g1);
      Cudd_RecursiveDerefZdd(dd, g0);
      Cudd_RecursiveDerefZdd(dd, q);
      return (NULL);
    }
    Cudd_Ref(r);
    Cudd_RecursiveDerefZdd(dd, q);
    Cudd_RecursiveDerefZdd(dd, tmp);
  }

  Cudd_RecursiveDerefZdd(dd, f1);
  Cudd_RecursiveDerefZdd(dd, f0);
  Cudd_RecursiveDerefZdd(dd, g1);
  Cudd_RecursiveDerefZdd(dd, g0);

  cuddCacheInsert2(dd, cuddZddDivideF, f, g, r);
  Cudd_Deref(r);
  return (r);

} /* end of cuddZddDivideF */

/**Function********************************************************************

  Synopsis    [Computes the three-way decomposition of f w.r.t. v.]

  Description [Computes the three-way decomposition of function f (represented
  by a ZDD) wit respect to variable v.  Returns 0 if successful; 1 otherwise.]

  SideEffects [The results are returned in f1, f0, and fd.]

  SeeAlso     [cuddZddGetCofactors2]

******************************************************************************/
int cuddZddGetCofactors3(DdManager *dd, DdNode *f, int v, DdNode **f1,
                         DdNode **f0, DdNode **fd) {
  DdNode *pc, *nc;
  DdNode *zero = DD_ZERO(dd);
  int top, hv, ht, pv, nv;
  int level;

  top = dd->permZ[f->index];
  level = dd->permZ[v];
  hv = level >> 1;
  ht = top >> 1;

  if (hv < ht) {
    *f1 = zero;
    *f0 = zero;
    *fd = f;
  } else {
    pv = cuddZddGetPosVarIndex(dd, v);
    nv = cuddZddGetNegVarIndex(dd, v);

    /* not to create intermediate ZDD node */
    if (cuddZddGetPosVarLevel(dd, v) < cuddZddGetNegVarLevel(dd, v)) {
      pc = cuddZddSubset1(dd, f, pv);
      if (pc == NULL)
        return (1);
      Cudd_Ref(pc);
      nc = cuddZddSubset0(dd, f, pv);
      if (nc == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        return (1);
      }
      Cudd_Ref(nc);

      *f1 = cuddZddSubset0(dd, pc, nv);
      if (*f1 == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        Cudd_RecursiveDerefZdd(dd, nc);
        return (1);
      }
      Cudd_Ref(*f1);
      *f0 = cuddZddSubset1(dd, nc, nv);
      if (*f0 == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        Cudd_RecursiveDerefZdd(dd, nc);
        Cudd_RecursiveDerefZdd(dd, *f1);
        return (1);
      }
      Cudd_Ref(*f0);

      *fd = cuddZddSubset0(dd, nc, nv);
      if (*fd == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        Cudd_RecursiveDerefZdd(dd, nc);
        Cudd_RecursiveDerefZdd(dd, *f1);
        Cudd_RecursiveDerefZdd(dd, *f0);
        return (1);
      }
      Cudd_Ref(*fd);
    } else {
      pc = cuddZddSubset1(dd, f, nv);
      if (pc == NULL)
        return (1);
      Cudd_Ref(pc);
      nc = cuddZddSubset0(dd, f, nv);
      if (nc == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        return (1);
      }
      Cudd_Ref(nc);

      *f0 = cuddZddSubset0(dd, pc, pv);
      if (*f0 == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        Cudd_RecursiveDerefZdd(dd, nc);
        return (1);
      }
      Cudd_Ref(*f0);
      *f1 = cuddZddSubset1(dd, nc, pv);
      if (*f1 == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        Cudd_RecursiveDerefZdd(dd, nc);
        Cudd_RecursiveDerefZdd(dd, *f0);
        return (1);
      }
      Cudd_Ref(*f1);

      *fd = cuddZddSubset0(dd, nc, pv);
      if (*fd == NULL) {
        Cudd_RecursiveDerefZdd(dd, pc);
        Cudd_RecursiveDerefZdd(dd, nc);
        Cudd_RecursiveDerefZdd(dd, *f1);
        Cudd_RecursiveDerefZdd(dd, *f0);
        return (1);
      }
      Cudd_Ref(*fd);
    }

    Cudd_RecursiveDerefZdd(dd, pc);
    Cudd_RecursiveDerefZdd(dd, nc);
    Cudd_Deref(*f1);
    Cudd_Deref(*f0);
    Cudd_Deref(*fd);
  }
  return (0);

} /* end of cuddZddGetCofactors3 */

/**Function********************************************************************

  Synopsis    [Computes the two-way decomposition of f w.r.t. v.]

  Description []

  SideEffects [The results are returned in f1 and f0.]

  SeeAlso     [cuddZddGetCofactors3]

******************************************************************************/
int cuddZddGetCofactors2(DdManager *dd, DdNode *f, int v, DdNode **f1,
                         DdNode **f0) {
  *f1 = cuddZddSubset1(dd, f, v);
  if (*f1 == NULL)
    return (1);
  *f0 = cuddZddSubset0(dd, f, v);
  if (*f0 == NULL) {
    Cudd_RecursiveDerefZdd(dd, *f1);
    return (1);
  }
  return (0);

} /* end of cuddZddGetCofactors2 */

/**Function********************************************************************

  Synopsis    [Computes a complement of a ZDD node.]

  Description [Computes the complement of a ZDD node. So far, since we
  couldn't find a direct way to get the complement of a ZDD cover, we first
  convert a ZDD cover to a BDD, then make the complement of the ZDD cover
  from the complement of the BDD node by using ISOP.]

  SideEffects [The result depends on current variable order.]

  SeeAlso     []

******************************************************************************/
DdNode *cuddZddComplement(DdManager *dd, DdNode *node) {
  DdNode *b, *isop, *zdd_I;

  /* Check cache */
  zdd_I = cuddCacheLookup1Zdd(dd, cuddZddComplement, node);
  if (zdd_I)
    return (zdd_I);

  b = cuddMakeBddFromZddCover(dd, node);
  if (!b)
    return (NULL);
  cuddRef(b);
  isop = cuddZddIsop(dd, Cudd_Not(b), Cudd_Not(b), &zdd_I);
  if (!isop) {
    Cudd_RecursiveDeref(dd, b);
    return (NULL);
  }
  cuddRef(isop);
  cuddRef(zdd_I);
  Cudd_RecursiveDeref(dd, b);
  Cudd_RecursiveDeref(dd, isop);

  cuddCacheInsert1(dd, cuddZddComplement, node, zdd_I);
  cuddDeref(zdd_I);
  return (zdd_I);
} /* end of cuddZddComplement */

/**Function********************************************************************

  Synopsis    [Returns the index of positive ZDD variable.]

  Description [Returns the index of positive ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int cuddZddGetPosVarIndex(DdManager *dd, int index) {
  int pv = (index >> 1) << 1;
  return (pv);
} /* end of cuddZddGetPosVarIndex */

/**Function********************************************************************

  Synopsis    [Returns the index of negative ZDD variable.]

  Description [Returns the index of negative ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int cuddZddGetNegVarIndex(DdManager *dd, int index) {
  int nv = index | 0x1;
  return (nv);
} /* end of cuddZddGetPosVarIndex */

/**Function********************************************************************

  Synopsis    [Returns the level of positive ZDD variable.]

  Description [Returns the level of positive ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int cuddZddGetPosVarLevel(DdManager *dd, int index) {
  int pv = cuddZddGetPosVarIndex(dd, index);
  return (dd->permZ[pv]);
} /* end of cuddZddGetPosVarLevel */

/**Function********************************************************************

  Synopsis    [Returns the level of negative ZDD variable.]

  Description [Returns the level of negative ZDD variable.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
int cuddZddGetNegVarLevel(DdManager *dd, int index) {
  int nv = cuddZddGetNegVarIndex(dd, index);
  return (dd->permZ[nv]);
} /* end of cuddZddGetNegVarLevel */
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
// static char rcsid[] DD_UNUSED = "$Id: cuddZddGroup.c,v 1.22 2012/02/05
// 01:07:19 fabio Exp $"; #endif

static int *entry;
extern int zddTotalNumberSwapping;
#ifdef DD_STATS
static int extsymmcalls;
static int extsymm;
static int secdiffcalls;
static int secdiff;
static int secdiffmisfire;
#endif
#ifdef DD_DEBUG
static int pr = 0; /* flag to enable printing while debugging */
                   /* by depositing a 1 into it */
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int zddTreeSiftingAux(DdManager *table, MtrNode *treenode,
                             Cudd_ReorderingType method);
#ifdef DD_STATS
static int zddCountInternalMtrNodes(DdManager *table, MtrNode *treenode);
#endif
static int zddReorderChildren(DdManager *table, MtrNode *treenode,
                              Cudd_ReorderingType method);
static void zddFindNodeHiLo(DdManager *table, MtrNode *treenode, int *lower,
                            int *upper);
static int zddUniqueCompareGroup(int *ptrX, int *ptrY);
static int zddGroupSifting(DdManager *table, int lower, int upper);
static int zddGroupSiftingAux(DdManager *table, int x, int xLow, int xHigh);
static int zddGroupSiftingUp(DdManager *table, int y, int xLow, Move **moves);
static int zddGroupSiftingDown(DdManager *table, int x, int xHigh,
                               Move **moves);
static int zddGroupMove(DdManager *table, int x, int y, Move **moves);
static int zddGroupMoveBackward(DdManager *table, int x, int y);
static int zddGroupSiftingBackward(DdManager *table, Move *moves, int size);
static void zddMergeGroups(DdManager *table, MtrNode *treenode, int low,
                           int high);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

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
int cuddZddTreeSifting(
    DdManager *table /* DD table */,
    Cudd_ReorderingType
        method /* reordering method for the groups of leaves */) {
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
    table->treeZ = Mtr_InitGroupTree(0, table->sizeZ);
    table->treeZ->index = table->invpermZ[0];
  }
  nvars = table->sizeZ;

#ifdef DD_DEBUG
  if (pr > 0 && !tempTree)
    (void)fprintf(table->out, "cuddZddTreeSifting:");
  Mtr_PrintGroups(table->treeZ, pr <= 0);
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

  (void)fprintf(table->out, "\n");
  if (!tempTree)
    (void)fprintf(table->out, "#:IM_NODES  %8d: group tree nodes\n",
                  zddCountInternalMtrNodes(table, table->treeZ));
#endif

  /* Initialize the group of each subtable to itself. Initially
  ** there are no groups. Groups are created according to the tree
  ** structure in postorder fashion.
  */
  for (i = 0; i < nvars; i++)
    table->subtableZ[i].next = i;

  /* Reorder. */
  result = zddTreeSiftingAux(table, table->treeZ, method);

#ifdef DD_STATS /* print stats */
  if (!tempTree && method == CUDD_REORDER_GROUP_SIFT &&
      (table->groupcheck == CUDD_GROUP_CHECK7 ||
       table->groupcheck == CUDD_GROUP_CHECK5)) {
    (void)fprintf(table->out, "\nextsymmcalls = %d\n", extsymmcalls);
    (void)fprintf(table->out, "extsymm = %d", extsymm);
  }
  if (!tempTree && method == CUDD_REORDER_GROUP_SIFT &&
      table->groupcheck == CUDD_GROUP_CHECK7) {
    (void)fprintf(table->out, "\nsecdiffcalls = %d\n", secdiffcalls);
    (void)fprintf(table->out, "secdiff = %d\n", secdiff);
    (void)fprintf(table->out, "secdiffmisfire = %d", secdiffmisfire);
  }
#endif

  if (tempTree)
    Cudd_FreeZddTree(table);
  return (result);

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
static int zddTreeSiftingAux(DdManager *table, MtrNode *treenode,
                             Cudd_ReorderingType method) {
  MtrNode *auxnode;
  int res;

#ifdef DD_DEBUG
  Mtr_PrintGroups(treenode, 1);
#endif

  auxnode = treenode;
  while (auxnode != NULL) {
    if (auxnode->child != NULL) {
      if (!zddTreeSiftingAux(table, auxnode->child, method))
        return (0);
      res = zddReorderChildren(table, auxnode, CUDD_REORDER_GROUP_SIFT);
      if (res == 0)
        return (0);
    } else if (auxnode->size > 1) {
      if (!zddReorderChildren(table, auxnode, method))
        return (0);
    }
    auxnode = auxnode->younger;
  }

  return (1);

} /* end of zddTreeSiftingAux */

#ifdef DD_STATS
/**Function********************************************************************

  Synopsis    [Counts the number of internal nodes of the group tree.]

  Description [Counts the number of internal nodes of the group tree.
  Returns the count.]

  SideEffects [None]

******************************************************************************/
static int zddCountInternalMtrNodes(DdManager *table, MtrNode *treenode) {
  MtrNode *auxnode;
  int count, nodeCount;

  nodeCount = 0;
  auxnode = treenode;
  while (auxnode != NULL) {
    if (!(MTR_TEST(auxnode, MTR_TERMINAL))) {
      nodeCount++;
      count = zddCountInternalMtrNodes(table, auxnode->child);
      nodeCount += count;
    }
    auxnode = auxnode->younger;
  }

  return (nodeCount);

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
static int zddReorderChildren(DdManager *table, MtrNode *treenode,
                              Cudd_ReorderingType method) {
  int lower;
  int upper;
  int result;
  unsigned int initialSize;

  zddFindNodeHiLo(table, treenode, &lower, &upper);
  /* If upper == -1 these variables do not exist yet. */
  if (upper == -1)
    return (1);

  if (treenode->flags == MTR_FIXED) {
    result = 1;
  } else {
#ifdef DD_STATS
    (void)fprintf(table->out, " ");
#endif
    switch (method) {
    case CUDD_REORDER_RANDOM:
    case CUDD_REORDER_RANDOM_PIVOT:
      result = cuddZddSwapping(table, lower, upper, method);
      break;
    case CUDD_REORDER_SIFT:
      result = cuddZddSifting(table, lower, upper);
      break;
    case CUDD_REORDER_SIFT_CONVERGE:
      do {
        initialSize = table->keysZ;
        result = cuddZddSifting(table, lower, upper);
        if (initialSize <= table->keysZ)
          break;
#ifdef DD_STATS
        else
          (void)fprintf(table->out, "\n");
#endif
      } while (result != 0);
      break;
    case CUDD_REORDER_SYMM_SIFT:
      result = cuddZddSymmSifting(table, lower, upper);
      break;
    case CUDD_REORDER_SYMM_SIFT_CONV:
      result = cuddZddSymmSiftingConv(table, lower, upper);
      break;
    case CUDD_REORDER_GROUP_SIFT:
      result = zddGroupSifting(table, lower, upper);
      break;
    case CUDD_REORDER_LINEAR:
      result = cuddZddLinearSifting(table, lower, upper);
      break;
    case CUDD_REORDER_LINEAR_CONVERGE:
      do {
        initialSize = table->keysZ;
        result = cuddZddLinearSifting(table, lower, upper);
        if (initialSize <= table->keysZ)
          break;
#ifdef DD_STATS
        else
          (void)fprintf(table->out, "\n");
#endif
      } while (result != 0);
      break;
    default:
      return (0);
    }
  }

  /* Create a single group for all the variables that were sifted,
  ** so that they will be treated as a single block by successive
  ** invocations of zddGroupSifting.
  */
  zddMergeGroups(table, treenode, lower, upper);

#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "zddReorderChildren:");
#endif

  return (result);

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
static void zddFindNodeHiLo(DdManager *table, MtrNode *treenode, int *lower,
                            int *upper) {
  int low;
  int high;

  /* Check whether no variables in this group already exist.
  ** If so, return immediately. The calling procedure will know from
  ** the values of upper that no reordering is needed.
  */
  if ((int)treenode->low >= table->sizeZ) {
    *lower = table->sizeZ;
    *upper = -1;
    return;
  }

  *lower = low = (unsigned int)table->permZ[treenode->index];
  high = (int)(low + treenode->size - 1);

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
      *upper = (unsigned int)table->sizeZ - 1;
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
          *upper = (unsigned int)thisLower - 1;
        auxnode = auxnode->younger;
      }
    }
  } else {
    /* Normal case: All the variables of the group exist. */
    *upper = (unsigned int)high;
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
static int zddUniqueCompareGroup(int *ptrX, int *ptrY) {
#if 0
    if (entry[*ptrY] == entry[*ptrX]) {
	return((*ptrX) - (*ptrY));
    }
#endif
  return (entry[*ptrY] - entry[*ptrX]);

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
static int zddGroupSifting(DdManager *table, int lower, int upper) {
  int *var;
  int i, j, x, xInit;
  int nvars;
  int classes;
  int result;
  int *sifted;
#ifdef DD_STATS
  unsigned previousSize;
#endif
  int xindex;

  nvars = table->sizeZ;

  /* Order variables to sift. */
  entry = NULL;
  sifted = NULL;
  var = ALLOC(int, nvars);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto zddGroupSiftingOutOfMem;
  }
  entry = ALLOC(int, nvars);
  if (entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto zddGroupSiftingOutOfMem;
  }
  sifted = ALLOC(int, nvars);
  if (sifted == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto zddGroupSiftingOutOfMem;
  }

  /* Here we consider only one representative for each group. */
  for (i = 0, classes = 0; i < nvars; i++) {
    sifted[i] = 0;
    x = table->permZ[i];
    if ((unsigned)x >= table->subtableZ[x].next) {
      entry[i] = table->subtableZ[x].keys;
      var[classes] = i;
      classes++;
    }
  }

  qsort((void *)var, classes, sizeof(int), (DD_QSFP)zddUniqueCompareGroup);

  /* Now sift. */
  for (i = 0; i < ddMin(table->siftMaxVar, classes); i++) {
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
    assert((unsigned)x >= table->subtableZ[x].next);
#endif
    result = zddGroupSiftingAux(table, x, lower, upper);
    if (!result)
      goto zddGroupSiftingOutOfMem;

#ifdef DD_STATS
    if (table->keysZ < previousSize) {
      (void)fprintf(table->out, "-");
    } else if (table->keysZ > previousSize) {
      (void)fprintf(table->out, "+");
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif

    /* Mark variables in the group just sifted. */
    x = table->permZ[xindex];
    if ((unsigned)x != table->subtableZ[x].next) {
      xInit = x;
      do {
        j = table->invpermZ[x];
        sifted[j] = 1;
        x = table->subtableZ[x].next;
      } while (x != xInit);
    }

#ifdef DD_DEBUG
    if (pr > 0)
      (void)fprintf(table->out, "zddGroupSifting:");
#endif
  } /* for */

  FREE(sifted);
  FREE(var);
  FREE(entry);

  return (1);

zddGroupSiftingOutOfMem:
  if (entry != NULL)
    FREE(entry);
  if (var != NULL)
    FREE(var);
  if (sifted != NULL)
    FREE(sifted);

  return (0);

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
static int zddGroupSiftingAux(DdManager *table, int x, int xLow, int xHigh) {
  Move *move;
  Move *moves; /* list of moves */
  int initialSize;
  int result;

#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "zddGroupSiftingAux from %d to %d\n", xLow,
                  xHigh);
  assert((unsigned)x >= table->subtableZ[x].next); /* x is bottom of group */
#endif

  initialSize = table->keysZ;
  moves = NULL;

  if (x == xLow) { /* Sift down */
#ifdef DD_DEBUG
    /* x must be a singleton */
    assert((unsigned)x == table->subtableZ[x].next);
#endif
    if (x == xHigh)
      return (1); /* just one variable */

    if (!zddGroupSiftingDown(table, x, xHigh, &moves))
      goto zddGroupSiftingAuxOutOfMem;
    /* at this point x == xHigh, unless early term */

    /* move backward and stop at best position */
    result = zddGroupSiftingBackward(table, moves, initialSize);
#ifdef DD_DEBUG
    assert(table->keysZ <= (unsigned)initialSize);
#endif
    if (!result)
      goto zddGroupSiftingAuxOutOfMem;

  } else if (cuddZddNextHigh(table, x) > xHigh) { /* Sift up */
#ifdef DD_DEBUG
    /* x is bottom of group */
    assert((unsigned)x >= table->subtableZ[x].next);
#endif
    /* Find top of x's group */
    x = table->subtableZ[x].next;

    if (!zddGroupSiftingUp(table, x, xLow, &moves))
      goto zddGroupSiftingAuxOutOfMem;
    /* at this point x == xLow, unless early term */

    /* move backward and stop at best position */
    result = zddGroupSiftingBackward(table, moves, initialSize);
#ifdef DD_DEBUG
    assert(table->keysZ <= (unsigned)initialSize);
#endif
    if (!result)
      goto zddGroupSiftingAuxOutOfMem;

  } else if (x - xLow > xHigh - x) { /* must go down first: shorter */
    if (!zddGroupSiftingDown(table, x, xHigh, &moves))
      goto zddGroupSiftingAuxOutOfMem;
    /* at this point x == xHigh, unless early term */

    /* Find top of group */
    if (moves) {
      x = moves->y;
    }
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;
    x = table->subtableZ[x].next;
#ifdef DD_DEBUG
    /* x should be the top of a group */
    assert((unsigned)x <= table->subtableZ[x].next);
#endif

    if (!zddGroupSiftingUp(table, x, xLow, &moves))
      goto zddGroupSiftingAuxOutOfMem;

    /* move backward and stop at best position */
    result = zddGroupSiftingBackward(table, moves, initialSize);
#ifdef DD_DEBUG
    assert(table->keysZ <= (unsigned)initialSize);
#endif
    if (!result)
      goto zddGroupSiftingAuxOutOfMem;

  } else { /* moving up first: shorter */
    /* Find top of x's group */
    x = table->subtableZ[x].next;

    if (!zddGroupSiftingUp(table, x, xLow, &moves))
      goto zddGroupSiftingAuxOutOfMem;
    /* at this point x == xHigh, unless early term */

    if (moves) {
      x = moves->x;
    }
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;
#ifdef DD_DEBUG
    /* x is bottom of a group */
    assert((unsigned)x >= table->subtableZ[x].next);
#endif

    if (!zddGroupSiftingDown(table, x, xHigh, &moves))
      goto zddGroupSiftingAuxOutOfMem;

    /* move backward and stop at best position */
    result = zddGroupSiftingBackward(table, moves, initialSize);
#ifdef DD_DEBUG
    assert(table->keysZ <= (unsigned)initialSize);
#endif
    if (!result)
      goto zddGroupSiftingAuxOutOfMem;
  }

  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }

  return (1);

zddGroupSiftingAuxOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }

  return (0);

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
static int zddGroupSiftingUp(DdManager *table, int y, int xLow, Move **moves) {
  Move *move;
  int x;
  int size;
  int gxtop;
  int limitSize;

  limitSize = table->keysZ;

  x = cuddZddNextLow(table, y);
  while (x >= xLow) {
    gxtop = table->subtableZ[x].next;
    if (table->subtableZ[x].next == (unsigned)x &&
        table->subtableZ[y].next == (unsigned)y) {
      /* x and y are self groups */
      size = cuddZddSwapInPlace(table, x, y);
#ifdef DD_DEBUG
      assert(table->subtableZ[x].next == (unsigned)x);
      assert(table->subtableZ[y].next == (unsigned)y);
#endif
      if (size == 0)
        goto zddGroupSiftingUpOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddGroupSiftingUpOutOfMem;
      move->x = x;
      move->y = y;
      move->flags = MTR_DEFAULT;
      move->size = size;
      move->next = *moves;
      *moves = move;

#ifdef DD_DEBUG
      if (pr > 0)
        (void)fprintf(table->out, "zddGroupSiftingUp (2 single groups):\n");
#endif
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;
    } else { /* group move */
      size = zddGroupMove(table, x, y, moves);
      if (size == 0)
        goto zddGroupSiftingUpOutOfMem;
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;
    }
    y = gxtop;
    x = cuddZddNextLow(table, y);
  }

  return (1);

zddGroupSiftingUpOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }
  return (0);

} /* end of zddGroupSiftingUp */

/**Function********************************************************************

  Synopsis    [Sifts down a variable until it reaches position xHigh.]

  Description [Sifts down a variable until it reaches position xHigh.
  Assumes that x is the bottom of a group (or a singleton).  Records
  all the moves.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int zddGroupSiftingDown(DdManager *table, int x, int xHigh,
                               Move **moves) {
  Move *move;
  int y;
  int size;
  int limitSize;
  int gybot;

  /* Initialize R */
  limitSize = size = table->keysZ;
  y = cuddZddNextHigh(table, x);
  while (y <= xHigh) {
    /* Find bottom of y group. */
    gybot = table->subtableZ[y].next;
    while (table->subtableZ[gybot].next != (unsigned)y)
      gybot = table->subtableZ[gybot].next;

    if (table->subtableZ[x].next == (unsigned)x &&
        table->subtableZ[y].next == (unsigned)y) {
      /* x and y are self groups */
      size = cuddZddSwapInPlace(table, x, y);
#ifdef DD_DEBUG
      assert(table->subtableZ[x].next == (unsigned)x);
      assert(table->subtableZ[y].next == (unsigned)y);
#endif
      if (size == 0)
        goto zddGroupSiftingDownOutOfMem;

      /* Record move. */
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddGroupSiftingDownOutOfMem;
      move->x = x;
      move->y = y;
      move->flags = MTR_DEFAULT;
      move->size = size;
      move->next = *moves;
      *moves = move;

#ifdef DD_DEBUG
      if (pr > 0)
        (void)fprintf(table->out, "zddGroupSiftingDown (2 single groups):\n");
#endif
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;
      x = y;
      y = cuddZddNextHigh(table, x);
    } else { /* Group move */
      size = zddGroupMove(table, x, y, moves);
      if (size == 0)
        goto zddGroupSiftingDownOutOfMem;
      if ((double)size > (double)limitSize * table->maxGrowth)
        return (1);
      if (size < limitSize)
        limitSize = size;
    }
    x = gybot;
    y = cuddZddNextHigh(table, x);
  }

  return (1);

zddGroupSiftingDownOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }

  return (0);

} /* end of zddGroupSiftingDown */

/**Function********************************************************************

  Synopsis    [Swaps two groups and records the move.]

  Description [Swaps two groups and records the move. Returns the
  number of keys in the DD table in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int zddGroupMove(DdManager *table, int x, int y, Move **moves) {
  Move *move;
  int size;
  int i, j, xtop, xbot, xsize, ytop, ybot, ysize, newxtop;
  int swapx, swapy;
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  int initialSize, bestSize;
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
  while ((unsigned)ybot < table->subtableZ[ybot].next)
    ybot = table->subtableZ[ybot].next;
  ytop = y;
  ysize = ybot - ytop + 1;

#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  initialSize = bestSize = table->keysZ;
#endif
  /* Sift the variables of the second group up through the first group */
  for (i = 1; i <= ysize; i++) {
    for (j = 1; j <= xsize; j++) {
      size = cuddZddSwapInPlace(table, x, y);
      if (size == 0)
        goto zddGroupMoveOutOfMem;
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
      if (size < bestSize)
        bestSize = size;
#endif
      swapx = x;
      swapy = y;
      y = x;
      x = cuddZddNextLow(table, y);
    }
    y = ytop + i;
    x = cuddZddNextLow(table, y);
  }
#if defined(DD_DEBUG) && defined(DD_VERBOSE)
  if ((bestSize < initialSize) && (bestSize < size))
    (void)fprintf(
        table->out,
        "Missed local minimum: initialSize:%d  bestSize:%d  finalSize:%d\n",
        initialSize, bestSize, size);
#endif

  /* fix groups */
  y = xtop; /* ytop is now where xtop used to be */
  for (i = 0; i < ysize - 1; i++) {
    table->subtableZ[y].next = cuddZddNextHigh(table, y);
    y = cuddZddNextHigh(table, y);
  }
  table->subtableZ[y].next = xtop; /* y is bottom of its group, join */
  /* it to top of its group */
  x = cuddZddNextHigh(table, y);
  newxtop = x;
  for (i = 0; i < xsize - 1; i++) {
    table->subtableZ[x].next = cuddZddNextHigh(table, x);
    x = cuddZddNextHigh(table, x);
  }
  table->subtableZ[x].next = newxtop; /* x is bottom of its group, join */
                                      /* it to top of its group */
#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "zddGroupMove:\n");
#endif

  /* Store group move */
  move = (Move *)cuddDynamicAllocNode(table);
  if (move == NULL)
    goto zddGroupMoveOutOfMem;
  move->x = swapx;
  move->y = swapy;
  move->flags = MTR_DEFAULT;
  move->size = table->keysZ;
  move->next = *moves;
  *moves = move;

  return (table->keysZ);

zddGroupMoveOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }
  return (0);

} /* end of zddGroupMove */

/**Function********************************************************************

  Synopsis    [Undoes the swap two groups.]

  Description [Undoes the swap two groups.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int zddGroupMoveBackward(DdManager *table, int x, int y) {
  int size;
  int i, j, xtop, xbot, xsize, ytop, ybot, ysize, newxtop;

#ifdef DD_DEBUG
  /* We assume that x < y */
  assert(x < y);
#endif

  /* Find top, bottom, and size for the two groups. */
  xbot = x;
  xtop = table->subtableZ[x].next;
  xsize = xbot - xtop + 1;
  ybot = y;
  while ((unsigned)ybot < table->subtableZ[ybot].next)
    ybot = table->subtableZ[ybot].next;
  ytop = y;
  ysize = ybot - ytop + 1;

  /* Sift the variables of the second group up through the first group */
  for (i = 1; i <= ysize; i++) {
    for (j = 1; j <= xsize; j++) {
      size = cuddZddSwapInPlace(table, x, y);
      if (size == 0)
        return (0);
      y = x;
      x = cuddZddNextLow(table, y);
    }
    y = ytop + i;
    x = cuddZddNextLow(table, y);
  }

  /* fix groups */
  y = xtop;
  for (i = 0; i < ysize - 1; i++) {
    table->subtableZ[y].next = cuddZddNextHigh(table, y);
    y = cuddZddNextHigh(table, y);
  }
  table->subtableZ[y].next = xtop; /* y is bottom of its group, join */
  /* to its top */
  x = cuddZddNextHigh(table, y);
  newxtop = x;
  for (i = 0; i < xsize - 1; i++) {
    table->subtableZ[x].next = cuddZddNextHigh(table, x);
    x = cuddZddNextHigh(table, x);
  }
  table->subtableZ[x].next = newxtop; /* x is bottom of its group, join */
                                      /* to its top */
#ifdef DD_DEBUG
  if (pr > 0)
    (void)fprintf(table->out, "zddGroupMoveBackward:\n");
#endif

  return (1);

} /* end of zddGroupMoveBackward */

/**Function********************************************************************

  Synopsis    [Determines the best position for a variables and returns
  it there.]

  Description [Determines the best position for a variables and returns
  it there.  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int zddGroupSiftingBackward(DdManager *table, Move *moves, int size) {
  Move *move;
  int res;

  for (move = moves; move != NULL; move = move->next) {
    if (move->size < size) {
      size = move->size;
    }
  }

  for (move = moves; move != NULL; move = move->next) {
    if (move->size == size)
      return (1);
    if ((table->subtableZ[move->x].next == move->x) &&
        (table->subtableZ[move->y].next == move->y)) {
      res = cuddZddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
#ifdef DD_DEBUG
      if (pr > 0)
        (void)fprintf(table->out, "zddGroupSiftingBackward:\n");
      assert(table->subtableZ[move->x].next == move->x);
      assert(table->subtableZ[move->y].next == move->y);
#endif
    } else { /* Group move necessary */
      res = zddGroupMoveBackward(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
    }
  }

  return (1);

} /* end of zddGroupSiftingBackward */

/**Function********************************************************************

  Synopsis    [Merges groups in the DD table.]

  Description [Creates a single group from low to high and adjusts the
  idex field of the tree node.]

  SideEffects [None]

******************************************************************************/
static void zddMergeGroups(DdManager *table, MtrNode *treenode, int low,
                           int high) {
  int i;
  MtrNode *auxnode;
  int saveindex;
  int newindex;

  /* Merge all variables from low to high in one group, unless
  ** this is the topmost group. In such a case we do not merge lest
  ** we lose the symmetry information. */
  if (treenode != table->treeZ) {
    for (i = low; i < high; i++)
      table->subtableZ[i].next = i + 1;
    table->subtableZ[high].next = low;
  }

  /* Adjust the index fields of the tree nodes. If a node is the
  ** first child of its parent, then the parent may also need adjustment. */
  saveindex = treenode->index;
  newindex = table->invpermZ[low];
  auxnode = treenode;
  do {
    auxnode->index = newindex;
    if (auxnode->parent == NULL || (int)auxnode->parent->index != saveindex)
      break;
    auxnode = auxnode->parent;
  } while (1);
  return;

} /* end of zddMergeGroups */
/**CFile***********************************************************************

  FileName    [cuddZddIsop.c]

  PackageName [cudd]

  Synopsis    [Functions to find irredundant SOP covers as ZDDs from BDDs.]

  Description [External procedures included in this module:
                    <ul>
                    <li> Cudd_bddIsop()
                    <li> Cudd_zddIsop()
                    <li> Cudd_MakeBddFromZddCover()
                    </ul>
               Internal procedures included in this module:
                    <ul>
                    <li> cuddBddIsop()
                    <li> cuddZddIsop()
                    <li> cuddMakeBddFromZddCover()
                    </ul>
               Static procedures included in this module:
                    <ul>
                    </ul>
              ]

  SeeAlso     []

  Author      [In-Ho Moon]

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
// static char rcsid[] DD_UNUSED = "$Id: cuddZddIsop.c,v 1.22 2012/02/05
// 01:07:19 fabio Exp $"; #endif

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

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddIsop.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_zddIsop]

******************************************************************************/
DdNode *cuddZddIsop(DdManager *dd, DdNode *L, DdNode *U, DdNode **zdd_I) {
  DdNode *one = DD_ONE(dd);
  DdNode *zero = Cudd_Not(one);
  DdNode *zdd_one = DD_ONE(dd);
  DdNode *zdd_zero = DD_ZERO(dd);
  int v, top_l, top_u;
  DdNode *Lsub0, *Usub0, *Lsub1, *Usub1, *Ld, *Ud;
  DdNode *Lsuper0, *Usuper0, *Lsuper1, *Usuper1;
  DdNode *Isub0, *Isub1, *Id;
  DdNode *zdd_Isub0, *zdd_Isub1, *zdd_Id;
  DdNode *x;
  DdNode *term0, *term1, *sum;
  DdNode *Lv, *Uv, *Lnv, *Unv;
  DdNode *r, *y, *z;
  int index;
  DD_CTFP cacheOp;

  statLine(dd);
  if (L == zero) {
    *zdd_I = zdd_zero;
    return (zero);
  }
  if (U == one) {
    *zdd_I = zdd_one;
    return (one);
  }

  if (U == zero || L == one) {
    printf("*** ERROR : illegal condition for ISOP (U < L).\n");
    exit(1);
  }

  /* Check the cache. We store two results for each recursive call.
  ** One is the BDD, and the other is the ZDD. Both are needed.
  ** Hence we need a double hit in the cache to terminate the
  ** recursion. Clearly, collisions may evict only one of the two
  ** results. */
  cacheOp = (DD_CTFP)cuddZddIsop;
  r = cuddCacheLookup2(dd, cuddBddIsop, L, U);
  if (r) {
    *zdd_I = cuddCacheLookup2Zdd(dd, cacheOp, L, U);
    if (*zdd_I)
      return (r);
    else {
      /* The BDD result may have been dead. In that case
      ** cuddCacheLookup2 would have called cuddReclaim,
      ** whose effects we now have to undo. */
      cuddRef(r);
      Cudd_RecursiveDeref(dd, r);
    }
  }

  top_l = dd->perm[Cudd_Regular(L)->index];
  top_u = dd->perm[Cudd_Regular(U)->index];
  v = ddMin(top_l, top_u);

  /* Compute cofactors. */
  if (top_l == v) {
    index = Cudd_Regular(L)->index;
    Lv = Cudd_T(L);
    Lnv = Cudd_E(L);
    if (Cudd_IsComplement(L)) {
      Lv = Cudd_Not(Lv);
      Lnv = Cudd_Not(Lnv);
    }
  } else {
    index = Cudd_Regular(U)->index;
    Lv = Lnv = L;
  }

  if (top_u == v) {
    Uv = Cudd_T(U);
    Unv = Cudd_E(U);
    if (Cudd_IsComplement(U)) {
      Uv = Cudd_Not(Uv);
      Unv = Cudd_Not(Unv);
    }
  } else {
    Uv = Unv = U;
  }

  Lsub0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Uv));
  if (Lsub0 == NULL)
    return (NULL);
  Cudd_Ref(Lsub0);
  Usub0 = Unv;
  Lsub1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Unv));
  if (Lsub1 == NULL) {
    Cudd_RecursiveDeref(dd, Lsub0);
    return (NULL);
  }
  Cudd_Ref(Lsub1);
  Usub1 = Uv;

  Isub0 = cuddZddIsop(dd, Lsub0, Usub0, &zdd_Isub0);
  if (Isub0 == NULL) {
    Cudd_RecursiveDeref(dd, Lsub0);
    Cudd_RecursiveDeref(dd, Lsub1);
    return (NULL);
  }
  /*
  if ((!cuddIsConstant(Cudd_Regular(Isub0))) &&
      (Cudd_Regular(Isub0)->index != zdd_Isub0->index / 2 ||
      dd->permZ[index * 2] > dd->permZ[zdd_Isub0->index])) {
      printf("*** ERROR : illegal permutation in ZDD. ***\n");
  }
  */
  Cudd_Ref(Isub0);
  Cudd_Ref(zdd_Isub0);
  Isub1 = cuddZddIsop(dd, Lsub1, Usub1, &zdd_Isub1);
  if (Isub1 == NULL) {
    Cudd_RecursiveDeref(dd, Lsub0);
    Cudd_RecursiveDeref(dd, Lsub1);
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    return (NULL);
  }
  /*
  if ((!cuddIsConstant(Cudd_Regular(Isub1))) &&
      (Cudd_Regular(Isub1)->index != zdd_Isub1->index / 2 ||
      dd->permZ[index * 2] > dd->permZ[zdd_Isub1->index])) {
      printf("*** ERROR : illegal permutation in ZDD. ***\n");
  }
  */
  Cudd_Ref(Isub1);
  Cudd_Ref(zdd_Isub1);
  Cudd_RecursiveDeref(dd, Lsub0);
  Cudd_RecursiveDeref(dd, Lsub1);

  Lsuper0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Isub0));
  if (Lsuper0 == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    return (NULL);
  }
  Cudd_Ref(Lsuper0);
  Lsuper1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Isub1));
  if (Lsuper1 == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Lsuper0);
    return (NULL);
  }
  Cudd_Ref(Lsuper1);
  Usuper0 = Unv;
  Usuper1 = Uv;

  /* Ld = Lsuper0 + Lsuper1 */
  Ld = cuddBddAndRecur(dd, Cudd_Not(Lsuper0), Cudd_Not(Lsuper1));
  if (Ld == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Lsuper0);
    Cudd_RecursiveDeref(dd, Lsuper1);
    return (NULL);
  }
  Ld = Cudd_Not(Ld);
  Cudd_Ref(Ld);
  /* Ud = Usuper0 * Usuper1 */
  Ud = cuddBddAndRecur(dd, Usuper0, Usuper1);
  if (Ud == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Lsuper0);
    Cudd_RecursiveDeref(dd, Lsuper1);
    Cudd_RecursiveDeref(dd, Ld);
    return (NULL);
  }
  Cudd_Ref(Ud);
  Cudd_RecursiveDeref(dd, Lsuper0);
  Cudd_RecursiveDeref(dd, Lsuper1);

  Id = cuddZddIsop(dd, Ld, Ud, &zdd_Id);
  if (Id == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Ld);
    Cudd_RecursiveDeref(dd, Ud);
    return (NULL);
  }
  /*
  if ((!cuddIsConstant(Cudd_Regular(Id))) &&
      (Cudd_Regular(Id)->index != zdd_Id->index / 2 ||
      dd->permZ[index * 2] > dd->permZ[zdd_Id->index])) {
      printf("*** ERROR : illegal permutation in ZDD. ***\n");
  }
  */
  Cudd_Ref(Id);
  Cudd_Ref(zdd_Id);
  Cudd_RecursiveDeref(dd, Ld);
  Cudd_RecursiveDeref(dd, Ud);

  x = cuddUniqueInter(dd, index, one, zero);
  if (x == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDerefZdd(dd, zdd_Id);
    return (NULL);
  }
  Cudd_Ref(x);
  /* term0 = x * Isub0 */
  term0 = cuddBddAndRecur(dd, Cudd_Not(x), Isub0);
  if (term0 == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDerefZdd(dd, zdd_Id);
    Cudd_RecursiveDeref(dd, x);
    return (NULL);
  }
  Cudd_Ref(term0);
  Cudd_RecursiveDeref(dd, Isub0);
  /* term1 = x * Isub1 */
  term1 = cuddBddAndRecur(dd, x, Isub1);
  if (term1 == NULL) {
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDerefZdd(dd, zdd_Id);
    Cudd_RecursiveDeref(dd, x);
    Cudd_RecursiveDeref(dd, term0);
    return (NULL);
  }
  Cudd_Ref(term1);
  Cudd_RecursiveDeref(dd, x);
  Cudd_RecursiveDeref(dd, Isub1);
  /* sum = term0 + term1 */
  sum = cuddBddAndRecur(dd, Cudd_Not(term0), Cudd_Not(term1));
  if (sum == NULL) {
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDerefZdd(dd, zdd_Id);
    Cudd_RecursiveDeref(dd, term0);
    Cudd_RecursiveDeref(dd, term1);
    return (NULL);
  }
  sum = Cudd_Not(sum);
  Cudd_Ref(sum);
  Cudd_RecursiveDeref(dd, term0);
  Cudd_RecursiveDeref(dd, term1);
  /* r = sum + Id */
  r = cuddBddAndRecur(dd, Cudd_Not(sum), Cudd_Not(Id));
  r = Cudd_NotCond(r, r != NULL);
  if (r == NULL) {
    Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
    Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDerefZdd(dd, zdd_Id);
    Cudd_RecursiveDeref(dd, sum);
    return (NULL);
  }
  Cudd_Ref(r);
  Cudd_RecursiveDeref(dd, sum);
  Cudd_RecursiveDeref(dd, Id);

  if (zdd_Isub0 != zdd_zero) {
    z = cuddZddGetNodeIVO(dd, index * 2 + 1, zdd_Isub0, zdd_Id);
    if (z == NULL) {
      Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
      Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
      Cudd_RecursiveDerefZdd(dd, zdd_Id);
      Cudd_RecursiveDeref(dd, r);
      return (NULL);
    }
  } else {
    z = zdd_Id;
  }
  Cudd_Ref(z);
  if (zdd_Isub1 != zdd_zero) {
    y = cuddZddGetNodeIVO(dd, index * 2, zdd_Isub1, z);
    if (y == NULL) {
      Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
      Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
      Cudd_RecursiveDerefZdd(dd, zdd_Id);
      Cudd_RecursiveDeref(dd, r);
      Cudd_RecursiveDerefZdd(dd, z);
      return (NULL);
    }
  } else
    y = z;
  Cudd_Ref(y);

  Cudd_RecursiveDerefZdd(dd, zdd_Isub0);
  Cudd_RecursiveDerefZdd(dd, zdd_Isub1);
  Cudd_RecursiveDerefZdd(dd, zdd_Id);
  Cudd_RecursiveDerefZdd(dd, z);

  cuddCacheInsert2(dd, cuddBddIsop, L, U, r);
  cuddCacheInsert2(dd, cacheOp, L, U, y);

  Cudd_Deref(r);
  Cudd_Deref(y);
  *zdd_I = y;
  /*
  if (Cudd_Regular(r)->index != y->index / 2) {
      printf("*** ERROR : mismatch in indices between BDD and ZDD. ***\n");
  }
  */
  return (r);

} /* end of cuddZddIsop */

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_bddIsop.]

  Description []

  SideEffects [None]

  SeeAlso     [Cudd_bddIsop]

******************************************************************************/
DdNode *cuddBddIsop(DdManager *dd, DdNode *L, DdNode *U) {
  DdNode *one = DD_ONE(dd);
  DdNode *zero = Cudd_Not(one);
  int v, top_l, top_u;
  DdNode *Lsub0, *Usub0, *Lsub1, *Usub1, *Ld, *Ud;
  DdNode *Lsuper0, *Usuper0, *Lsuper1, *Usuper1;
  DdNode *Isub0, *Isub1, *Id;
  DdNode *x;
  DdNode *term0, *term1, *sum;
  DdNode *Lv, *Uv, *Lnv, *Unv;
  DdNode *r;
  int index;

  statLine(dd);
  if (L == zero)
    return (zero);
  if (U == one)
    return (one);

  /* Check cache */
  r = cuddCacheLookup2(dd, cuddBddIsop, L, U);
  if (r)
    return (r);

  top_l = dd->perm[Cudd_Regular(L)->index];
  top_u = dd->perm[Cudd_Regular(U)->index];
  v = ddMin(top_l, top_u);

  /* Compute cofactors */
  if (top_l == v) {
    index = Cudd_Regular(L)->index;
    Lv = Cudd_T(L);
    Lnv = Cudd_E(L);
    if (Cudd_IsComplement(L)) {
      Lv = Cudd_Not(Lv);
      Lnv = Cudd_Not(Lnv);
    }
  } else {
    index = Cudd_Regular(U)->index;
    Lv = Lnv = L;
  }

  if (top_u == v) {
    Uv = Cudd_T(U);
    Unv = Cudd_E(U);
    if (Cudd_IsComplement(U)) {
      Uv = Cudd_Not(Uv);
      Unv = Cudd_Not(Unv);
    }
  } else {
    Uv = Unv = U;
  }

  Lsub0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Uv));
  if (Lsub0 == NULL)
    return (NULL);
  Cudd_Ref(Lsub0);
  Usub0 = Unv;
  Lsub1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Unv));
  if (Lsub1 == NULL) {
    Cudd_RecursiveDeref(dd, Lsub0);
    return (NULL);
  }
  Cudd_Ref(Lsub1);
  Usub1 = Uv;

  Isub0 = cuddBddIsop(dd, Lsub0, Usub0);
  if (Isub0 == NULL) {
    Cudd_RecursiveDeref(dd, Lsub0);
    Cudd_RecursiveDeref(dd, Lsub1);
    return (NULL);
  }
  Cudd_Ref(Isub0);
  Isub1 = cuddBddIsop(dd, Lsub1, Usub1);
  if (Isub1 == NULL) {
    Cudd_RecursiveDeref(dd, Lsub0);
    Cudd_RecursiveDeref(dd, Lsub1);
    Cudd_RecursiveDeref(dd, Isub0);
    return (NULL);
  }
  Cudd_Ref(Isub1);
  Cudd_RecursiveDeref(dd, Lsub0);
  Cudd_RecursiveDeref(dd, Lsub1);

  Lsuper0 = cuddBddAndRecur(dd, Lnv, Cudd_Not(Isub0));
  if (Lsuper0 == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    return (NULL);
  }
  Cudd_Ref(Lsuper0);
  Lsuper1 = cuddBddAndRecur(dd, Lv, Cudd_Not(Isub1));
  if (Lsuper1 == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Lsuper0);
    return (NULL);
  }
  Cudd_Ref(Lsuper1);
  Usuper0 = Unv;
  Usuper1 = Uv;

  /* Ld = Lsuper0 + Lsuper1 */
  Ld = cuddBddAndRecur(dd, Cudd_Not(Lsuper0), Cudd_Not(Lsuper1));
  Ld = Cudd_NotCond(Ld, Ld != NULL);
  if (Ld == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Lsuper0);
    Cudd_RecursiveDeref(dd, Lsuper1);
    return (NULL);
  }
  Cudd_Ref(Ld);
  Ud = cuddBddAndRecur(dd, Usuper0, Usuper1);
  if (Ud == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Lsuper0);
    Cudd_RecursiveDeref(dd, Lsuper1);
    Cudd_RecursiveDeref(dd, Ld);
    return (NULL);
  }
  Cudd_Ref(Ud);
  Cudd_RecursiveDeref(dd, Lsuper0);
  Cudd_RecursiveDeref(dd, Lsuper1);

  Id = cuddBddIsop(dd, Ld, Ud);
  if (Id == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Ld);
    Cudd_RecursiveDeref(dd, Ud);
    return (NULL);
  }
  Cudd_Ref(Id);
  Cudd_RecursiveDeref(dd, Ld);
  Cudd_RecursiveDeref(dd, Ud);

  x = cuddUniqueInter(dd, index, one, zero);
  if (x == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Id);
    return (NULL);
  }
  Cudd_Ref(x);
  term0 = cuddBddAndRecur(dd, Cudd_Not(x), Isub0);
  if (term0 == NULL) {
    Cudd_RecursiveDeref(dd, Isub0);
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDeref(dd, x);
    return (NULL);
  }
  Cudd_Ref(term0);
  Cudd_RecursiveDeref(dd, Isub0);
  term1 = cuddBddAndRecur(dd, x, Isub1);
  if (term1 == NULL) {
    Cudd_RecursiveDeref(dd, Isub1);
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDeref(dd, x);
    Cudd_RecursiveDeref(dd, term0);
    return (NULL);
  }
  Cudd_Ref(term1);
  Cudd_RecursiveDeref(dd, x);
  Cudd_RecursiveDeref(dd, Isub1);
  /* sum = term0 + term1 */
  sum = cuddBddAndRecur(dd, Cudd_Not(term0), Cudd_Not(term1));
  sum = Cudd_NotCond(sum, sum != NULL);
  if (sum == NULL) {
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDeref(dd, term0);
    Cudd_RecursiveDeref(dd, term1);
    return (NULL);
  }
  Cudd_Ref(sum);
  Cudd_RecursiveDeref(dd, term0);
  Cudd_RecursiveDeref(dd, term1);
  /* r = sum + Id */
  r = cuddBddAndRecur(dd, Cudd_Not(sum), Cudd_Not(Id));
  r = Cudd_NotCond(r, r != NULL);
  if (r == NULL) {
    Cudd_RecursiveDeref(dd, Id);
    Cudd_RecursiveDeref(dd, sum);
    return (NULL);
  }
  Cudd_Ref(r);
  Cudd_RecursiveDeref(dd, sum);
  Cudd_RecursiveDeref(dd, Id);

  cuddCacheInsert2(dd, cuddBddIsop, L, U, r);

  Cudd_Deref(r);
  return (r);

} /* end of cuddBddIsop */

/**Function********************************************************************

  Synopsis [Converts a ZDD cover to a BDD.]

  Description [Converts a ZDD cover to a BDD. If successful, it returns
  a BDD node, otherwise it returns NULL. It is a recursive algorithm
  that works as follows. First it computes 3 cofactors of a ZDD cover:
  f1, f0 and fd. Second, it compute BDDs (b1, b0 and bd) of f1, f0 and fd.
  Third, it computes T=b1+bd and E=b0+bd. Fourth, it computes ITE(v,T,E) where
  v is the variable which has the index of the top node of the ZDD cover.
  In this case, since the index of v can be larger than either the one of T
  or the one of E, cuddUniqueInterIVO is called, where IVO stands for
  independent from variable ordering.]

  SideEffects []

  SeeAlso     [Cudd_MakeBddFromZddCover]

******************************************************************************/
DdNode *cuddMakeBddFromZddCover(DdManager *dd, DdNode *node) {
  DdNode *neW;
  int v;
  DdNode *f1, *f0, *fd;
  DdNode *b1, *b0, *bd;
  DdNode *T, *E;

  statLine(dd);
  if (node == dd->one)
    return (dd->one);
  if (node == dd->zero)
    return (Cudd_Not(dd->one));

  /* Check cache */
  neW = cuddCacheLookup1(dd, cuddMakeBddFromZddCover, node);
  if (neW)
    return (neW);

  v = Cudd_Regular(node)->index; /* either yi or zi */
  if (cuddZddGetCofactors3(dd, node, v, &f1, &f0, &fd))
    return (NULL);
  Cudd_Ref(f1);
  Cudd_Ref(f0);
  Cudd_Ref(fd);

  b1 = cuddMakeBddFromZddCover(dd, f1);
  if (!b1) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    return (NULL);
  }
  Cudd_Ref(b1);
  b0 = cuddMakeBddFromZddCover(dd, f0);
  if (!b0) {
    Cudd_RecursiveDerefZdd(dd, f1);
    Cudd_RecursiveDerefZdd(dd, f0);
    Cudd_RecursiveDerefZdd(dd, fd);
    Cudd_RecursiveDeref(dd, b1);
    return (NULL);
  }
  Cudd_Ref(b0);
  Cudd_RecursiveDerefZdd(dd, f1);
  Cudd_RecursiveDerefZdd(dd, f0);
  if (fd != dd->zero) {
    bd = cuddMakeBddFromZddCover(dd, fd);
    if (!bd) {
      Cudd_RecursiveDerefZdd(dd, fd);
      Cudd_RecursiveDeref(dd, b1);
      Cudd_RecursiveDeref(dd, b0);
      return (NULL);
    }
    Cudd_Ref(bd);
    Cudd_RecursiveDerefZdd(dd, fd);

    T = cuddBddAndRecur(dd, Cudd_Not(b1), Cudd_Not(bd));
    if (!T) {
      Cudd_RecursiveDeref(dd, b1);
      Cudd_RecursiveDeref(dd, b0);
      Cudd_RecursiveDeref(dd, bd);
      return (NULL);
    }
    T = Cudd_NotCond(T, T != NULL);
    Cudd_Ref(T);
    Cudd_RecursiveDeref(dd, b1);
    E = cuddBddAndRecur(dd, Cudd_Not(b0), Cudd_Not(bd));
    if (!E) {
      Cudd_RecursiveDeref(dd, b0);
      Cudd_RecursiveDeref(dd, bd);
      Cudd_RecursiveDeref(dd, T);
      return (NULL);
    }
    E = Cudd_NotCond(E, E != NULL);
    Cudd_Ref(E);
    Cudd_RecursiveDeref(dd, b0);
    Cudd_RecursiveDeref(dd, bd);
  } else {
    Cudd_RecursiveDerefZdd(dd, fd);
    T = b1;
    E = b0;
  }

  if (Cudd_IsComplement(T)) {
    neW = cuddUniqueInterIVO(dd, v / 2, Cudd_Not(T), Cudd_Not(E));
    if (!neW) {
      Cudd_RecursiveDeref(dd, T);
      Cudd_RecursiveDeref(dd, E);
      return (NULL);
    }
    neW = Cudd_Not(neW);
  } else {
    neW = cuddUniqueInterIVO(dd, v / 2, T, E);
    if (!neW) {
      Cudd_RecursiveDeref(dd, T);
      Cudd_RecursiveDeref(dd, E);
      return (NULL);
    }
  }
  Cudd_Ref(neW);
  Cudd_RecursiveDeref(dd, T);
  Cudd_RecursiveDeref(dd, E);

  cuddCacheInsert1(dd, cuddMakeBddFromZddCover, node, neW);
  Cudd_Deref(neW);
  return (neW);

} /* end of cuddMakeBddFromZddCover */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/
/**CFile***********************************************************************

  FileName    [cuddZddLin.c]

  PackageName [cudd]

  Synopsis    [Procedures for dynamic variable ordering of ZDDs.]

  Description [Internal procedures included in this module:
                    <ul>
                    <li> cuddZddLinearSifting()
                    </ul>
               Static procedures included in this module:
                    <ul>
                    <li> cuddZddLinearInPlace()
                    <li> cuddZddLinerAux()
                    <li> cuddZddLinearUp()
                    <li> cuddZddLinearDown()
                    <li> cuddZddLinearBackward()
                    <li> cuddZddUndoMoves()
                    </ul>
              ]

  SeeAlso     [cuddLinear.c cuddZddReord.c]

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

#define CUDD_SWAP_MOVE 0
#define CUDD_LINEAR_TRANSFORM_MOVE 1
#define CUDD_INVERSE_TRANSFORM_MOVE 2

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
// static char rcsid[] DD_UNUSED = "$Id: cuddZddLin.c,v 1.16 2012/02/05 01:07:19
// fabio Exp $"; #endif

extern int *zdd_entry;
extern int zddTotalNumberSwapping;
static int zddTotalNumberLinearTr;
static DdNode *empty;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int cuddZddLinearInPlace(DdManager *table, int x, int y);
static int cuddZddLinearAux(DdManager *table, int x, int xLow, int xHigh);
static Move *cuddZddLinearUp(DdManager *table, int y, int xLow,
                             Move *prevMoves);
static Move *cuddZddLinearDown(DdManager *table, int x, int xHigh,
                               Move *prevMoves);
static int cuddZddLinearBackward(DdManager *table, int size, Move *moves);
static Move *cuddZddUndoMoves(DdManager *table, Move *moves);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Implementation of the linear sifting algorithm for ZDDs.]

  Description [Implementation of the linear sifting algorithm for ZDDs.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries
    in each unique table.
    <li> Sift the variable up and down and applies the XOR transformation,
    remembering each time the total size of the DD heap.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddLinearSifting(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int size;
  int x;
  int result;
#ifdef DD_STATS
  int previousSize;
#endif

  size = table->sizeZ;
  empty = table->zero;

  /* Find order in which to sift variables. */
  var = NULL;
  zdd_entry = ALLOC(int, size);
  if (zdd_entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddZddSiftingOutOfMem;
  }
  var = ALLOC(int, size);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddZddSiftingOutOfMem;
  }

  for (i = 0; i < size; i++) {
    x = table->permZ[i];
    zdd_entry[i] = table->subtableZ[x].keys;
    var[i] = i;
  }

  qsort((void *)var, size, sizeof(int), (DD_QSFP)cuddZddUniqueCompare);

  /* Now sift. */
  for (i = 0; i < ddMin(table->siftMaxVar, size); i++) {
    if (zddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (util_cpu_time() - table->startTime > table->timeLimit) {
      table->autoDynZ = 0; /* prevent further reordering */
      break;
    }
    x = table->permZ[var[i]];
    if (x < lower || x > upper)
      continue;
#ifdef DD_STATS
    previousSize = table->keysZ;
#endif
    result = cuddZddLinearAux(table, x, lower, upper);
    if (!result)
      goto cuddZddSiftingOutOfMem;
#ifdef DD_STATS
    if (table->keysZ < (unsigned)previousSize) {
      (void)fprintf(table->out, "-");
    } else if (table->keysZ > (unsigned)previousSize) {
      (void)fprintf(table->out, "+"); /* should never happen */
      (void)fprintf(
          table->out,
          "\nSize increased from %d to %d while sifting variable %d\n",
          previousSize, table->keysZ, var[i]);
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

  FREE(var);
  FREE(zdd_entry);

  return (1);

cuddZddSiftingOutOfMem:

  if (zdd_entry != NULL)
    FREE(zdd_entry);
  if (var != NULL)
    FREE(var);

  return (0);

} /* end of cuddZddLinearSifting */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Linearly combines two adjacent variables.]

  Description [Linearly combines two adjacent variables. It assumes
  that no dead nodes are present on entry to this procedure.  The
  procedure then guarantees that no dead nodes will be present when it
  terminates.  cuddZddLinearInPlace assumes that x &lt; y.  Returns the
  number of keys in the table if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [cuddZddSwapInPlace cuddLinearInPlace]

******************************************************************************/
static int cuddZddLinearInPlace(DdManager *table, int x, int y) {
  DdNodePtr *xlist, *ylist;
  int xindex, yindex;
  int xslots, yslots;
  int xshift, yshift;
  int oldxkeys, oldykeys;
  int newxkeys, newykeys;
  int i;
  int posn;
  DdNode *f, *f1, *f0, *f11, *f10, *f01, *f00;
  DdNode *newf1, *newf0, *g, *next, *previous;
  DdNode *special;

#ifdef DD_DEBUG
  assert(x < y);
  assert(cuddZddNextHigh(table, x) == y);
  assert(table->subtableZ[x].keys != 0);
  assert(table->subtableZ[y].keys != 0);
  assert(table->subtableZ[x].dead == 0);
  assert(table->subtableZ[y].dead == 0);
#endif

  zddTotalNumberLinearTr++;

  /* Get parameters of x subtable. */
  xindex = table->invpermZ[x];
  xlist = table->subtableZ[x].nodelist;
  oldxkeys = table->subtableZ[x].keys;
  xslots = table->subtableZ[x].slots;
  xshift = table->subtableZ[x].shift;
  newxkeys = 0;

  /* Get parameters of y subtable. */
  yindex = table->invpermZ[y];
  ylist = table->subtableZ[y].nodelist;
  oldykeys = table->subtableZ[y].keys;
  yslots = table->subtableZ[y].slots;
  yshift = table->subtableZ[y].shift;
  newykeys = oldykeys;

  /* The nodes in the x layer are put in two chains.  The chain
  ** pointed by g holds the normal nodes. When re-expressed they stay
  ** in the x list. The chain pointed by special holds the elements
  ** that will move to the y list.
  */
  g = special = NULL;
  for (i = 0; i < xslots; i++) {
    f = xlist[i];
    if (f == NULL)
      continue;
    xlist[i] = NULL;
    while (f != NULL) {
      next = f->next;
      f1 = cuddT(f);
      /* if (f1->index == yindex) */ cuddSatDec(f1->ref);
      f0 = cuddE(f);
      /* if (f0->index == yindex) */ cuddSatDec(f0->ref);
      if ((int)f1->index == yindex && cuddE(f1) == empty &&
          (int)f0->index != yindex) {
        f->next = special;
        special = f;
      } else {
        f->next = g;
        g = f;
      }
      f = next;
    } /* while there are elements in the collision chain */
  }   /* for each slot of the x subtable */

  /* Mark y nodes with pointers from above x. We mark them by
  **  changing their index to x.
  */
  for (i = 0; i < yslots; i++) {
    f = ylist[i];
    while (f != NULL) {
      if (f->ref != 0) {
        f->index = xindex;
      }
      f = f->next;
    } /* while there are elements in the collision chain */
  }   /* for each slot of the y subtable */

  /* Move special nodes to the y list. */
  f = special;
  while (f != NULL) {
    next = f->next;
    f1 = cuddT(f);
    f11 = cuddT(f1);
    cuddT(f) = f11;
    cuddSatInc(f11->ref);
    f0 = cuddE(f);
    cuddSatInc(f0->ref);
    f->index = yindex;
    /* Insert at the beginning of the list so that it will be
    ** found first if there is a duplicate. The duplicate will
    ** eventually be moved or garbage collected. No node
    ** re-expression will add a pointer to it.
    */
    posn = ddHash(f11, f0, yshift);
    f->next = ylist[posn];
    ylist[posn] = f;
    newykeys++;
    f = next;
  }

  /* Take care of the remaining x nodes that must be re-expressed.
  ** They form a linked list pointed by g.
  */
  f = g;
  while (f != NULL) {
#ifdef DD_COUNT
    table->swapSteps++;
#endif
    next = f->next;
    /* Find f1, f0, f11, f10, f01, f00. */
    f1 = cuddT(f);
    if ((int)f1->index == yindex || (int)f1->index == xindex) {
      f11 = cuddT(f1);
      f10 = cuddE(f1);
    } else {
      f11 = empty;
      f10 = f1;
    }
    f0 = cuddE(f);
    if ((int)f0->index == yindex || (int)f0->index == xindex) {
      f01 = cuddT(f0);
      f00 = cuddE(f0);
    } else {
      f01 = empty;
      f00 = f0;
    }
    /* Create the new T child. */
    if (f01 == empty) {
      newf1 = f10;
      cuddSatInc(newf1->ref);
    } else {
      /* Check ylist for triple (yindex, f01, f10). */
      posn = ddHash(f01, f10, yshift);
      /* For each element newf1 in collision list ylist[posn]. */
      newf1 = ylist[posn];
      /* Search the collision chain skipping the marked nodes. */
      while (newf1 != NULL) {
        if (cuddT(newf1) == f01 && cuddE(newf1) == f10 &&
            (int)newf1->index == yindex) {
          cuddSatInc(newf1->ref);
          break; /* match */
        }
        newf1 = newf1->next;
      }                    /* while newf1 */
      if (newf1 == NULL) { /* no match */
        newf1 = cuddDynamicAllocNode(table);
        if (newf1 == NULL)
          goto zddSwapOutOfMem;
        newf1->index = yindex;
        newf1->ref = 1;
        cuddT(newf1) = f01;
        cuddE(newf1) = f10;
        /* Insert newf1 in the collision list ylist[pos];
        ** increase the ref counts of f01 and f10
        */
        newykeys++;
        newf1->next = ylist[posn];
        ylist[posn] = newf1;
        cuddSatInc(f01->ref);
        cuddSatInc(f10->ref);
      }
    }
    cuddT(f) = newf1;

    /* Do the same for f0. */
    /* Create the new E child. */
    if (f11 == empty) {
      newf0 = f00;
      cuddSatInc(newf0->ref);
    } else {
      /* Check ylist for triple (yindex, f11, f00). */
      posn = ddHash(f11, f00, yshift);
      /* For each element newf0 in collision list ylist[posn]. */
      newf0 = ylist[posn];
      while (newf0 != NULL) {
        if (cuddT(newf0) == f11 && cuddE(newf0) == f00 &&
            (int)newf0->index == yindex) {
          cuddSatInc(newf0->ref);
          break; /* match */
        }
        newf0 = newf0->next;
      }                    /* while newf0 */
      if (newf0 == NULL) { /* no match */
        newf0 = cuddDynamicAllocNode(table);
        if (newf0 == NULL)
          goto zddSwapOutOfMem;
        newf0->index = yindex;
        newf0->ref = 1;
        cuddT(newf0) = f11;
        cuddE(newf0) = f00;
        /* Insert newf0 in the collision list ylist[posn];
        ** increase the ref counts of f11 and f00.
        */
        newykeys++;
        newf0->next = ylist[posn];
        ylist[posn] = newf0;
        cuddSatInc(f11->ref);
        cuddSatInc(f00->ref);
      }
    }
    cuddE(f) = newf0;

    /* Re-insert the modified f in xlist.
    ** The modified f does not already exists in xlist.
    ** (Because of the uniqueness of the cofactors.)
    */
    posn = ddHash(newf1, newf0, xshift);
    newxkeys++;
    f->next = xlist[posn];
    xlist[posn] = f;
    f = next;
  } /* while f != NULL */

  /* GC the y layer and move the marked nodes to the x list. */

  /* For each node f in ylist. */
  for (i = 0; i < yslots; i++) {
    previous = NULL;
    f = ylist[i];
    while (f != NULL) {
      next = f->next;
      if (f->ref == 0) {
        cuddSatDec(cuddT(f)->ref);
        cuddSatDec(cuddE(f)->ref);
        cuddDeallocNode(table, f);
        newykeys--;
        if (previous == NULL)
          ylist[i] = next;
        else
          previous->next = next;
      } else if ((int)f->index == xindex) { /* move marked node */
        if (previous == NULL)
          ylist[i] = next;
        else
          previous->next = next;
        f1 = cuddT(f);
        cuddSatDec(f1->ref);
        /* Check ylist for triple (yindex, f1, empty). */
        posn = ddHash(f1, empty, yshift);
        /* For each element newf1 in collision list ylist[posn]. */
        newf1 = ylist[posn];
        while (newf1 != NULL) {
          if (cuddT(newf1) == f1 && cuddE(newf1) == empty &&
              (int)newf1->index == yindex) {
            cuddSatInc(newf1->ref);
            break; /* match */
          }
          newf1 = newf1->next;
        }                    /* while newf1 */
        if (newf1 == NULL) { /* no match */
          newf1 = cuddDynamicAllocNode(table);
          if (newf1 == NULL)
            goto zddSwapOutOfMem;
          newf1->index = yindex;
          newf1->ref = 1;
          cuddT(newf1) = f1;
          cuddE(newf1) = empty;
          /* Insert newf1 in the collision list ylist[posn];
          ** increase the ref counts of f1 and empty.
          */
          newykeys++;
          newf1->next = ylist[posn];
          ylist[posn] = newf1;
          if (posn == i && previous == NULL)
            previous = newf1;
          cuddSatInc(f1->ref);
          cuddSatInc(empty->ref);
        }
        cuddT(f) = newf1;
        f0 = cuddE(f);
        /* Insert f in x list. */
        posn = ddHash(newf1, f0, xshift);
        newxkeys++;
        newykeys--;
        f->next = xlist[posn];
        xlist[posn] = f;
      } else {
        previous = f;
      }
      f = next;
    } /* while f */
  }   /* for i */

  /* Set the appropriate fields in table. */
  table->subtableZ[x].keys = newxkeys;
  table->subtableZ[y].keys = newykeys;

  table->keysZ += newxkeys + newykeys - oldxkeys - oldykeys;

  /* Update univ section; univ[x] remains the same. */
  table->univ[y] = cuddT(table->univ[x]);

#if 0
    (void) fprintf(table->out,"x = %d  y = %d\n", x, y);
    (void) Cudd_DebugCheck(table);
    (void) Cudd_CheckKeys(table);
#endif

  return (table->keysZ);

zddSwapOutOfMem:
  (void)fprintf(table->err, "Error: cuddZddSwapInPlace out of memory\n");

  return (0);

} /* end of cuddZddLinearInPlace */

/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddZddLinearAux(DdManager *table, int x, int xLow, int xHigh) {
  Move *move;
  Move *moveUp;   /* list of up move */
  Move *moveDown; /* list of down move */

  int initial_size;
  int result;

  initial_size = table->keysZ;

#ifdef DD_DEBUG
  assert(table->subtableZ[x].keys > 0);
#endif

  moveDown = NULL;
  moveUp = NULL;

  if (x == xLow) {
    moveDown = cuddZddLinearDown(table, x, xHigh, NULL);
    /* At this point x --> xHigh. */
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto cuddZddLinearAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = cuddZddLinearBackward(table, initial_size, moveDown);
    if (!result)
      goto cuddZddLinearAuxOutOfMem;

  } else if (x == xHigh) {
    moveUp = cuddZddLinearUp(table, x, xLow, NULL);
    /* At this point x --> xLow. */
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto cuddZddLinearAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = cuddZddLinearBackward(table, initial_size, moveUp);
    if (!result)
      goto cuddZddLinearAuxOutOfMem;

  } else if ((x - xLow) > (xHigh - x)) { /* must go down first: shorter */
    moveDown = cuddZddLinearDown(table, x, xHigh, NULL);
    /* At this point x --> xHigh. */
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto cuddZddLinearAuxOutOfMem;
    moveUp = cuddZddUndoMoves(table, moveDown);
#ifdef DD_DEBUG
    assert(moveUp == NULL || moveUp->x == x);
#endif
    moveUp = cuddZddLinearUp(table, x, xLow, moveUp);
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto cuddZddLinearAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = cuddZddLinearBackward(table, initial_size, moveUp);
    if (!result)
      goto cuddZddLinearAuxOutOfMem;

  } else {
    moveUp = cuddZddLinearUp(table, x, xLow, NULL);
    /* At this point x --> xHigh. */
    if (moveUp == (Move *)CUDD_OUT_OF_MEM)
      goto cuddZddLinearAuxOutOfMem;
    /* Then move up. */
    moveDown = cuddZddUndoMoves(table, moveUp);
#ifdef DD_DEBUG
    assert(moveDown == NULL || moveDown->y == x);
#endif
    moveDown = cuddZddLinearDown(table, x, xHigh, moveDown);
    if (moveDown == (Move *)CUDD_OUT_OF_MEM)
      goto cuddZddLinearAuxOutOfMem;
    /* Move backward and stop at best position. */
    result = cuddZddLinearBackward(table, initial_size, moveDown);
    if (!result)
      goto cuddZddLinearAuxOutOfMem;
  }

  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (1);

cuddZddLinearAuxOutOfMem:
  if (moveDown != (Move *)CUDD_OUT_OF_MEM) {
    while (moveDown != NULL) {
      move = moveDown->next;
      cuddDeallocMove(table, moveDown);
      moveDown = move;
    }
  }
  if (moveUp != (Move *)CUDD_OUT_OF_MEM) {
    while (moveUp != NULL) {
      move = moveUp->next;
      cuddDeallocMove(table, moveUp);
      moveUp = move;
    }
  }

  return (0);

} /* end of cuddZddLinearAux */

/**Function********************************************************************

  Synopsis    [Sifts a variable up applying the XOR transformation.]

  Description [Sifts a variable up applying the XOR
  transformation. Moves y up until either it reaches the bound (xLow)
  or the size of the ZDD heap increases too much.  Returns the set of
  moves in case of success; NULL if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *cuddZddLinearUp(DdManager *table, int y, int xLow,
                             Move *prevMoves) {
  Move *moves;
  Move *move;
  int x;
  int size, newsize;
  int limitSize;

  moves = prevMoves;
  limitSize = table->keysZ;

  x = cuddZddNextLow(table, y);
  while (x >= xLow) {
    size = cuddZddSwapInPlace(table, x, y);
    if (size == 0)
      goto cuddZddLinearUpOutOfMem;
    newsize = cuddZddLinearInPlace(table, x, y);
    if (newsize == 0)
      goto cuddZddLinearUpOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto cuddZddLinearUpOutOfMem;
    move->x = x;
    move->y = y;
    move->next = moves;
    moves = move;
    move->flags = CUDD_SWAP_MOVE;
    if (newsize > size) {
      /* Undo transformation. The transformation we apply is
      ** its own inverse. Hence, we just apply the transformation
      ** again.
      */
      newsize = cuddZddLinearInPlace(table, x, y);
      if (newsize == 0)
        goto cuddZddLinearUpOutOfMem;
#ifdef DD_DEBUG
      if (newsize != size) {
        (void)fprintf(
            table->err,
            "Change in size after identity transformation! From %d to %d\n",
            size, newsize);
      }
#endif
    } else {
      size = newsize;
      move->flags = CUDD_LINEAR_TRANSFORM_MOVE;
    }
    move->size = size;

    if ((double)size > (double)limitSize * table->maxGrowth)
      break;
    if (size < limitSize)
      limitSize = size;

    y = x;
    x = cuddZddNextLow(table, y);
  }
  return (moves);

cuddZddLinearUpOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of cuddZddLinearUp */

/**Function********************************************************************

  Synopsis    [Sifts a variable down and applies the XOR transformation.]

  Description [Sifts a variable down. Moves x down until either it
  reaches the bound (xHigh) or the size of the ZDD heap increases too
  much. Returns the set of moves in case of success; NULL if memory is
  full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *cuddZddLinearDown(DdManager *table, int x, int xHigh,
                               Move *prevMoves) {
  Move *moves;
  Move *move;
  int y;
  int size, newsize;
  int limitSize;

  moves = prevMoves;
  limitSize = table->keysZ;

  y = cuddZddNextHigh(table, x);
  while (y <= xHigh) {
    size = cuddZddSwapInPlace(table, x, y);
    if (size == 0)
      goto cuddZddLinearDownOutOfMem;
    newsize = cuddZddLinearInPlace(table, x, y);
    if (newsize == 0)
      goto cuddZddLinearDownOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto cuddZddLinearDownOutOfMem;
    move->x = x;
    move->y = y;
    move->next = moves;
    moves = move;
    move->flags = CUDD_SWAP_MOVE;
    if (newsize > size) {
      /* Undo transformation. The transformation we apply is
      ** its own inverse. Hence, we just apply the transformation
      ** again.
      */
      newsize = cuddZddLinearInPlace(table, x, y);
      if (newsize == 0)
        goto cuddZddLinearDownOutOfMem;
      if (newsize != size) {
        (void)fprintf(
            table->err,
            "Change in size after identity transformation! From %d to %d\n",
            size, newsize);
      }
    } else {
      size = newsize;
      move->flags = CUDD_LINEAR_TRANSFORM_MOVE;
    }
    move->size = size;

    if ((double)size > (double)limitSize * table->maxGrowth)
      break;
    if (size < limitSize)
      limitSize = size;

    x = y;
    y = cuddZddNextHigh(table, x);
  }
  return (moves);

cuddZddLinearDownOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of cuddZddLinearDown */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the ZDD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the ZDD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddZddLinearBackward(DdManager *table, int size, Move *moves) {
  Move *move;
  int res;

  /* Find the minimum size among moves. */
  for (move = moves; move != NULL; move = move->next) {
    if (move->size < size) {
      size = move->size;
    }
  }

  for (move = moves; move != NULL; move = move->next) {
    if (move->size == size)
      return (1);
    if (move->flags == CUDD_LINEAR_TRANSFORM_MOVE) {
      res = cuddZddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
    }
    res = cuddZddSwapInPlace(table, move->x, move->y);
    if (!res)
      return (0);
    if (move->flags == CUDD_INVERSE_TRANSFORM_MOVE) {
      res = cuddZddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!res)
        return (0);
    }
  }

  return (1);

} /* end of cuddZddLinearBackward */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the ZDD heap to the order
  in effect before the moves.]

  Description [Given a set of moves, returns the ZDD heap to the
  order in effect before the moves.  Returns 1 in case of success;
  0 otherwise.]

  SideEffects [None]

******************************************************************************/
static Move *cuddZddUndoMoves(DdManager *table, Move *moves) {
  Move *invmoves = NULL;
  Move *move;
  Move *invmove;
  int size;

  for (move = moves; move != NULL; move = move->next) {
    invmove = (Move *)cuddDynamicAllocNode(table);
    if (invmove == NULL)
      goto cuddZddUndoMovesOutOfMem;
    invmove->x = move->x;
    invmove->y = move->y;
    invmove->next = invmoves;
    invmoves = invmove;
    if (move->flags == CUDD_SWAP_MOVE) {
      invmove->flags = CUDD_SWAP_MOVE;
      size = cuddZddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto cuddZddUndoMovesOutOfMem;
    } else if (move->flags == CUDD_LINEAR_TRANSFORM_MOVE) {
      invmove->flags = CUDD_INVERSE_TRANSFORM_MOVE;
      size = cuddZddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto cuddZddUndoMovesOutOfMem;
      size = cuddZddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto cuddZddUndoMovesOutOfMem;
    } else { /* must be CUDD_INVERSE_TRANSFORM_MOVE */
#ifdef DD_DEBUG
      (void)fprintf(table->err, "Unforseen event in ddUndoMoves!\n");
#endif
      invmove->flags = CUDD_LINEAR_TRANSFORM_MOVE;
      size = cuddZddSwapInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto cuddZddUndoMovesOutOfMem;
      size = cuddZddLinearInPlace(table, (int)move->x, (int)move->y);
      if (!size)
        goto cuddZddUndoMovesOutOfMem;
    }
    invmove->size = size;
  }

  return (invmoves);

cuddZddUndoMovesOutOfMem:
  while (invmoves != NULL) {
    move = invmoves->next;
    cuddDeallocMove(table, invmoves);
    invmoves = move;
  }
  return ((Move *)CUDD_OUT_OF_MEM);

} /* end of cuddZddUndoMoves */

/**CFile***********************************************************************

  FileName    [cuddZddReord.c]

  PackageName [cudd]

  Synopsis    [Procedures for dynamic variable ordering of ZDDs.]

  Description [External procedures included in this module:
                    <ul>
                    <li> Cudd_zddReduceHeap()
                    <li> Cudd_zddShuffleHeap()
                    </ul>
               Internal procedures included in this module:
                    <ul>
                    <li> cuddZddAlignToBdd()
                    <li> cuddZddNextHigh()
                    <li> cuddZddNextLow()
                    <li> cuddZddUniqueCompare()
                    <li> cuddZddSwapInPlace()
                    <li> cuddZddSwapping()
                    <li> cuddZddSifting()
                    </ul>
               Static procedures included in this module:
                    <ul>
                    <li> zddSwapAny()
                    <li> cuddZddSiftingAux()
                    <li> cuddZddSiftingUp()
                    <li> cuddZddSiftingDown()
                    <li> cuddZddSiftingBackward()
                    <li> zddReorderPreprocess()
                    <li> zddReorderPostprocess()
                    <li> zddShuffle()
                    <li> zddSiftUp()
                    </ul>
              ]

  SeeAlso     []

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

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DD_MAX_SUBTABLE_SPARSITY 8

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
// static char rcsid[] DD_UNUSED = "$Id: cuddZddReord.c,v 1.49 2012/02/05
// 01:07:19 fabio Exp $"; #endif

int *zdd_entry;

int zddTotalNumberSwapping;

static DdNode *empty;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static Move *zddSwapAny(DdManager *table, int x, int y);
static int cuddZddSiftingAux(DdManager *table, int x, int x_low, int x_high);
static Move *cuddZddSiftingUp(DdManager *table, int x, int x_low,
                              int initial_size);
static Move *cuddZddSiftingDown(DdManager *table, int x, int x_high,
                                int initial_size);
static int cuddZddSiftingBackward(DdManager *table, Move *moves, int size);
static void zddReorderPreprocess(DdManager *table);
static int zddReorderPostprocess(DdManager *table);
static int zddShuffle(DdManager *table, int *permutation);
static int zddSiftUp(DdManager *table, int x, int xLow);
static void zddFixTree(DdManager *table, MtrNode *treenode);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Main dynamic reordering routine for ZDDs.]

  Description [Main dynamic reordering routine for ZDDs.
  Calls one of the possible reordering procedures:
  <ul>
  <li>Swapping
  <li>Sifting
  <li>Symmetric Sifting
  </ul>

  For sifting and symmetric sifting it is possible to request reordering
  to convergence.<p>

  The core of all methods is the reordering procedure
  cuddZddSwapInPlace() which swaps two adjacent variables.
  Returns 1 in case of success; 0 otherwise. In the case of symmetric
  sifting (with and without convergence) returns 1 plus the number of
  symmetric variables, in case of success.]

  SideEffects [Changes the variable order for all ZDDs and clears
  the cache.]

******************************************************************************/
int Cudd_zddReduceHeap(
    DdManager *table /* DD manager */,
    Cudd_ReorderingType heuristic /* method used for reordering */,
    int minsize /* bound below which no reordering occurs */) {
  DdHook *hook;
  int result;
  unsigned int nextDyn;
#ifdef DD_STATS
  unsigned int initialSize;
  unsigned int finalSize;
#endif
  unsigned long localTime;

  /* Don't reorder if there are too many dead nodes. */
  if (table->keysZ - table->deadZ < (unsigned)minsize)
    return (1);

  if (heuristic == CUDD_REORDER_SAME) {
    heuristic = table->autoMethodZ;
  }
  if (heuristic == CUDD_REORDER_NONE) {
    return (1);
  }

  /* This call to Cudd_zddReduceHeap does initiate reordering. Therefore
  ** we count it.
  */
  table->reorderings++;
  empty = table->zero;

  localTime = util_cpu_time();

  /* Run the hook functions. */
  hook = table->preReorderingHook;
  while (hook != NULL) {
    int res = (hook->f)(table, "ZDD", (void *)heuristic);
    if (res == 0)
      return (0);
    hook = hook->next;
  }

  /* Clear the cache and collect garbage. */
  zddReorderPreprocess(table);
  zddTotalNumberSwapping = 0;

#ifdef DD_STATS
  initialSize = table->keysZ;

  switch (heuristic) {
  case CUDD_REORDER_RANDOM:
  case CUDD_REORDER_RANDOM_PIVOT:
    (void)fprintf(table->out, "#:I_RANDOM  ");
    break;
  case CUDD_REORDER_SIFT:
  case CUDD_REORDER_SIFT_CONVERGE:
  case CUDD_REORDER_SYMM_SIFT:
  case CUDD_REORDER_SYMM_SIFT_CONV:
    (void)fprintf(table->out, "#:I_SIFTING ");
    break;
  case CUDD_REORDER_LINEAR:
  case CUDD_REORDER_LINEAR_CONVERGE:
    (void)fprintf(table->out, "#:I_LINSIFT ");
    break;
  default:
    (void)fprintf(table->err, "Unsupported ZDD reordering method\n");
    return (0);
  }
  (void)fprintf(table->out, "%8d: initial size", initialSize);
#endif

  result = cuddZddTreeSifting(table, heuristic);

#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
  finalSize = table->keysZ;
  (void)fprintf(table->out, "#:F_REORDER %8d: final size\n", finalSize);
  (void)fprintf(table->out, "#:T_REORDER %8g: total time (sec)\n",
                ((double)(util_cpu_time() - localTime) / 1000.0));
  (void)fprintf(table->out, "#:N_REORDER %8d: total swaps\n",
                zddTotalNumberSwapping);
#endif

  if (result == 0)
    return (0);

  if (!zddReorderPostprocess(table))
    return (0);

  if (table->realignZ) {
    if (!cuddBddAlignToZdd(table))
      return (0);
  }

  nextDyn = table->keysZ * DD_DYN_RATIO;
  if (table->reorderings < 20 || nextDyn > table->nextDyn)
    table->nextDyn = nextDyn;
  else
    table->nextDyn += 20;

  table->reordered = 1;

  /* Run hook functions. */
  hook = table->postReorderingHook;
  while (hook != NULL) {
    int res = (hook->f)(table, "ZDD", (void *)localTime);
    if (res == 0)
      return (0);
    hook = hook->next;
  }
  /* Update cumulative reordering time. */
  table->reordTime += util_cpu_time() - localTime;

  return (result);

} /* end of Cudd_zddReduceHeap */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Reorders ZDD variables according to the order of the BDD
  variables.]

  Description [Reorders ZDD variables according to the order of the
  BDD variables. This function can be called at the end of BDD
  reordering to insure that the order of the ZDD variables is
  consistent with the order of the BDD variables. The number of ZDD
  variables must be a multiple of the number of BDD variables. Let
  <code>M</code> be the ratio of the two numbers. cuddZddAlignToBdd
  then considers the ZDD variables from <code>M*i</code> to
  <code>(M+1)*i-1</code> as corresponding to BDD variable
  <code>i</code>.  This function should be normally called from
  Cudd_ReduceHeap, which clears the cache.  Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [Changes the ZDD variable order for all diagrams and performs
  garbage collection of the ZDD unique table.]

  SeeAlso [Cudd_zddShuffleHeap Cudd_ReduceHeap]

******************************************************************************/
int cuddZddAlignToBdd(DdManager *table /* DD manager */) {
  int *invpermZ; /* permutation array */
  int M;         /* ratio of ZDD variables to BDD variables */
  int i, j;      /* loop indices */
  int result;    /* return value */

  /* We assume that a ratio of 0 is OK. */
  if (table->sizeZ == 0)
    return (1);

  empty = table->zero;
  M = table->sizeZ / table->size;
  /* Check whether the number of ZDD variables is a multiple of the
  ** number of BDD variables.
  */
  if (M * table->size != table->sizeZ)
    return (0);
  /* Create and initialize the inverse permutation array. */
  invpermZ = ALLOC(int, table->sizeZ);
  if (invpermZ == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    return (0);
  }
  for (i = 0; i < table->size; i++) {
    int index = table->invperm[i];
    int indexZ = index * M;
    int levelZ = table->permZ[indexZ];
    levelZ = (levelZ / M) * M;
    for (j = 0; j < M; j++) {
      invpermZ[M * i + j] = table->invpermZ[levelZ + j];
    }
  }
  /* Eliminate dead nodes. Do not scan the cache again, because we
  ** assume that Cudd_ReduceHeap has already cleared it.
  */
  cuddGarbageCollect(table, 0);

  result = zddShuffle(table, invpermZ);
  FREE(invpermZ);
  /* Fix the ZDD variable group tree. */
  zddFixTree(table, table->treeZ);
  return (result);

} /* end of cuddZddAlignToBdd */

/**Function********************************************************************

  Synopsis    [Finds the next subtable with a larger index.]

  Description [Finds the next subtable with a larger index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddNextHigh(DdManager *table, int x) {
  return (x + 1);

} /* end of cuddZddNextHigh */

/**Function********************************************************************

  Synopsis    [Finds the next subtable with a smaller index.]

  Description [Finds the next subtable with a smaller index. Returns the
  index.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddNextLow(DdManager *table, int x) {
  return (x - 1);

} /* end of cuddZddNextLow */

/**Function********************************************************************

  Synopsis [Comparison function used by qsort.]

  Description [Comparison function used by qsort to order the
  variables according to the number of keys in the subtables.
  Returns the difference in number of keys between the two
  variables being compared.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddUniqueCompare(int *ptr_x, int *ptr_y) {
  return (zdd_entry[*ptr_y] - zdd_entry[*ptr_x]);

} /* end of cuddZddUniqueCompare */

/**Function********************************************************************

  Synopsis    [Swaps two adjacent variables.]

  Description [Swaps two adjacent variables. It assumes that no dead
  nodes are present on entry to this procedure.  The procedure then
  guarantees that no dead nodes will be present when it terminates.
  cuddZddSwapInPlace assumes that x &lt; y.  Returns the number of keys in
  the table if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddSwapInPlace(DdManager *table, int x, int y) {
  DdNodePtr *xlist, *ylist;
  int xindex, yindex;
  int xslots, yslots;
  int xshift, yshift;
  int oldxkeys, oldykeys;
  int newxkeys, newykeys;
  int i;
  int posn;
  DdNode *f, *f1, *f0, *f11, *f10, *f01, *f00;
  DdNode *newf1, *newf0, *next;
  DdNodePtr g, *lastP, *previousP;

#ifdef DD_DEBUG
  assert(x < y);
  assert(cuddZddNextHigh(table, x) == y);
  assert(table->subtableZ[x].keys != 0);
  assert(table->subtableZ[y].keys != 0);
  assert(table->subtableZ[x].dead == 0);
  assert(table->subtableZ[y].dead == 0);
#endif

  zddTotalNumberSwapping++;

  /* Get parameters of x subtable. */
  xindex = table->invpermZ[x];
  xlist = table->subtableZ[x].nodelist;
  oldxkeys = table->subtableZ[x].keys;
  xslots = table->subtableZ[x].slots;
  xshift = table->subtableZ[x].shift;
  newxkeys = 0;

  yindex = table->invpermZ[y];
  ylist = table->subtableZ[y].nodelist;
  oldykeys = table->subtableZ[y].keys;
  yslots = table->subtableZ[y].slots;
  yshift = table->subtableZ[y].shift;
  newykeys = oldykeys;

  /* The nodes in the x layer that don't depend on y directly
  ** will stay there; the others are put in a chain.
  ** The chain is handled as a FIFO; g points to the beginning and
  ** last points to the end.
  */

  g = NULL;
  lastP = &g;
  for (i = 0; i < xslots; i++) {
    previousP = &(xlist[i]);
    f = *previousP;
    while (f != NULL) {
      next = f->next;
      f1 = cuddT(f);
      f0 = cuddE(f);
      if ((f1->index != (DdHalfWord)yindex) &&
          (f0->index != (DdHalfWord)yindex)) { /* stays */
        newxkeys++;
        *previousP = f;
        previousP = &(f->next);
      } else {
        f->index = yindex;
        *lastP = f;
        lastP = &(f->next);
      }
      f = next;
    } /* while there are elements in the collision chain */
    *previousP = NULL;
  } /* for each slot of the x subtable */
  *lastP = NULL;

#ifdef DD_COUNT
  table->swapSteps += oldxkeys - newxkeys;
#endif
  /* Take care of the x nodes that must be re-expressed.
  ** They form a linked list pointed by g. Their index has been
  ** changed to yindex already.
  */
  f = g;
  while (f != NULL) {
    next = f->next;
    /* Find f1, f0, f11, f10, f01, f00. */
    f1 = cuddT(f);
    if ((int)f1->index == yindex) {
      f11 = cuddT(f1);
      f10 = cuddE(f1);
    } else {
      f11 = empty;
      f10 = f1;
    }
    f0 = cuddE(f);
    if ((int)f0->index == yindex) {
      f01 = cuddT(f0);
      f00 = cuddE(f0);
    } else {
      f01 = empty;
      f00 = f0;
    }

    /* Decrease ref count of f1. */
    cuddSatDec(f1->ref);
    /* Create the new T child. */
    if (f11 == empty) {
      if (f01 != empty) {
        newf1 = f01;
        cuddSatInc(newf1->ref);
      }
      /* else case was already handled when finding nodes
      ** with both children below level y
      */
    } else {
      /* Check xlist for triple (xindex, f11, f01). */
      posn = ddHash(f11, f01, xshift);
      /* For each element newf1 in collision list xlist[posn]. */
      newf1 = xlist[posn];
      while (newf1 != NULL) {
        if (cuddT(newf1) == f11 && cuddE(newf1) == f01) {
          cuddSatInc(newf1->ref);
          break; /* match */
        }
        newf1 = newf1->next;
      }                    /* while newf1 */
      if (newf1 == NULL) { /* no match */
        newf1 = cuddDynamicAllocNode(table);
        if (newf1 == NULL)
          goto zddSwapOutOfMem;
        newf1->index = xindex;
        newf1->ref = 1;
        cuddT(newf1) = f11;
        cuddE(newf1) = f01;
        /* Insert newf1 in the collision list xlist[pos];
        ** increase the ref counts of f11 and f01
        */
        newxkeys++;
        newf1->next = xlist[posn];
        xlist[posn] = newf1;
        cuddSatInc(f11->ref);
        cuddSatInc(f01->ref);
      }
    }
    cuddT(f) = newf1;

    /* Do the same for f0. */
    /* Decrease ref count of f0. */
    cuddSatDec(f0->ref);
    /* Create the new E child. */
    if (f10 == empty) {
      newf0 = f00;
      cuddSatInc(newf0->ref);
    } else {
      /* Check xlist for triple (xindex, f10, f00). */
      posn = ddHash(f10, f00, xshift);
      /* For each element newf0 in collision list xlist[posn]. */
      newf0 = xlist[posn];
      while (newf0 != NULL) {
        if (cuddT(newf0) == f10 && cuddE(newf0) == f00) {
          cuddSatInc(newf0->ref);
          break; /* match */
        }
        newf0 = newf0->next;
      }                    /* while newf0 */
      if (newf0 == NULL) { /* no match */
        newf0 = cuddDynamicAllocNode(table);
        if (newf0 == NULL)
          goto zddSwapOutOfMem;
        newf0->index = xindex;
        newf0->ref = 1;
        cuddT(newf0) = f10;
        cuddE(newf0) = f00;
        /* Insert newf0 in the collision list xlist[posn];
        ** increase the ref counts of f10 and f00.
        */
        newxkeys++;
        newf0->next = xlist[posn];
        xlist[posn] = newf0;
        cuddSatInc(f10->ref);
        cuddSatInc(f00->ref);
      }
    }
    cuddE(f) = newf0;

    /* Insert the modified f in ylist.
    ** The modified f does not already exists in ylist.
    ** (Because of the uniqueness of the cofactors.)
    */
    posn = ddHash(newf1, newf0, yshift);
    newykeys++;
    f->next = ylist[posn];
    ylist[posn] = f;
    f = next;
  } /* while f != NULL */

  /* GC the y layer. */

  /* For each node f in ylist. */
  for (i = 0; i < yslots; i++) {
    previousP = &(ylist[i]);
    f = *previousP;
    while (f != NULL) {
      next = f->next;
      if (f->ref == 0) {
        cuddSatDec(cuddT(f)->ref);
        cuddSatDec(cuddE(f)->ref);
        cuddDeallocNode(table, f);
        newykeys--;
      } else {
        *previousP = f;
        previousP = &(f->next);
      }
      f = next;
    } /* while f */
    *previousP = NULL;
  } /* for i */

  /* Set the appropriate fields in table. */
  table->subtableZ[x].nodelist = ylist;
  table->subtableZ[x].slots = yslots;
  table->subtableZ[x].shift = yshift;
  table->subtableZ[x].keys = newykeys;
  table->subtableZ[x].maxKeys = yslots * DD_MAX_SUBTABLE_DENSITY;

  table->subtableZ[y].nodelist = xlist;
  table->subtableZ[y].slots = xslots;
  table->subtableZ[y].shift = xshift;
  table->subtableZ[y].keys = newxkeys;
  table->subtableZ[y].maxKeys = xslots * DD_MAX_SUBTABLE_DENSITY;

  table->permZ[xindex] = y;
  table->permZ[yindex] = x;
  table->invpermZ[x] = yindex;
  table->invpermZ[y] = xindex;

  table->keysZ += newxkeys + newykeys - oldxkeys - oldykeys;

  /* Update univ section; univ[x] remains the same. */
  table->univ[y] = cuddT(table->univ[x]);

  return (table->keysZ);

zddSwapOutOfMem:
  (void)fprintf(table->err, "Error: cuddZddSwapInPlace out of memory\n");

  return (0);

} /* end of cuddZddSwapInPlace */

/**Function********************************************************************

  Synopsis    [Reorders variables by a sequence of (non-adjacent) swaps.]

  Description [Implementation of Plessier's algorithm that reorders
  variables by a sequence of (non-adjacent) swaps.
    <ol>
    <li> Select two variables (RANDOM or HEURISTIC).
    <li> Permute these variables.
    <li> If the nodes have decreased accept the permutation.
    <li> Otherwise reconstruct the original heap.
    <li> Loop.
    </ol>
  Returns 1 in case of success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddSwapping(DdManager *table, int lower, int upper,
                    Cudd_ReorderingType heuristic) {
  int i, j;
  int max, keys;
  int nvars;
  int x, y;
  int iterate;
  int previousSize;
  Move *moves, *move;
  int pivot;
  int modulo;
  int result;

#ifdef DD_DEBUG
  /* Sanity check */
  assert(lower >= 0 && upper < table->sizeZ && lower <= upper);
#endif

  nvars = upper - lower + 1;
  iterate = nvars;

  for (i = 0; i < iterate; i++) {
    if (heuristic == CUDD_REORDER_RANDOM_PIVOT) {
      /* Find pivot <= id with maximum keys. */
      for (max = -1, j = lower; j <= upper; j++) {
        if ((keys = table->subtableZ[j].keys) > max) {
          max = keys;
          pivot = j;
        }
      }

      modulo = upper - pivot;
      if (modulo == 0) {
        y = pivot; /* y = nvars-1 */
      } else {
        /* y = random # from {pivot+1 .. nvars-1} */
        y = pivot + 1 + (int)(Cudd_Random() % modulo);
      }

      modulo = pivot - lower - 1;
      if (modulo < 1) { /* if pivot = 1 or 0 */
        x = lower;
      } else {
        do { /* x = random # from {0 .. pivot-2} */
          x = (int)Cudd_Random() % modulo;
        } while (x == y);
        /* Is this condition really needed, since x and y
           are in regions separated by pivot? */
      }
    } else {
      x = (int)(Cudd_Random() % nvars) + lower;
      do {
        y = (int)(Cudd_Random() % nvars) + lower;
      } while (x == y);
    }

    previousSize = table->keysZ;
    moves = zddSwapAny(table, x, y);
    if (moves == NULL)
      goto cuddZddSwappingOutOfMem;

    result = cuddZddSiftingBackward(table, moves, previousSize);
    if (!result)
      goto cuddZddSwappingOutOfMem;

    while (moves != NULL) {
      move = moves->next;
      cuddDeallocMove(table, moves);
      moves = move;
    }
#ifdef DD_STATS
    if (table->keysZ < (unsigned)previousSize) {
      (void)fprintf(table->out, "-");
    } else if (table->keysZ > (unsigned)previousSize) {
      (void)fprintf(table->out, "+"); /* should never happen */
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

  return (1);

cuddZddSwappingOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (0);

} /* end of cuddZddSwapping */

/**Function********************************************************************

  Synopsis    [Implementation of Rudell's sifting algorithm.]

  Description [Implementation of Rudell's sifting algorithm.
  Assumes that no dead nodes are present.
    <ol>
    <li> Order all the variables according to the number of entries
    in each unique table.
    <li> Sift the variable up and down, remembering each time the
    total size of the DD heap.
    <li> Select the best permutation.
    <li> Repeat 3 and 4 for all variables.
    </ol>
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int cuddZddSifting(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int size;
  int x;
  int result;
#ifdef DD_STATS
  int previousSize;
#endif

  size = table->sizeZ;

  /* Find order in which to sift variables. */
  var = NULL;
  zdd_entry = ALLOC(int, size);
  if (zdd_entry == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddZddSiftingOutOfMem;
  }
  var = ALLOC(int, size);
  if (var == NULL) {
    table->errorCode = CUDD_MEMORY_OUT;
    goto cuddZddSiftingOutOfMem;
  }

  for (i = 0; i < size; i++) {
    x = table->permZ[i];
    zdd_entry[i] = table->subtableZ[x].keys;
    var[i] = i;
  }

  qsort((void *)var, size, sizeof(int), (DD_QSFP)cuddZddUniqueCompare);

  /* Now sift. */
  for (i = 0; i < ddMin(table->siftMaxVar, size); i++) {
    if (zddTotalNumberSwapping >= table->siftMaxSwap)
      break;
    if (util_cpu_time() - table->startTime > table->timeLimit) {
      table->autoDynZ = 0; /* prevent further reordering */
      break;
    }
    x = table->permZ[var[i]];
    if (x < lower || x > upper)
      continue;
#ifdef DD_STATS
    previousSize = table->keysZ;
#endif
    result = cuddZddSiftingAux(table, x, lower, upper);
    if (!result)
      goto cuddZddSiftingOutOfMem;
#ifdef DD_STATS
    if (table->keysZ < (unsigned)previousSize) {
      (void)fprintf(table->out, "-");
    } else if (table->keysZ > (unsigned)previousSize) {
      (void)fprintf(table->out, "+"); /* should never happen */
      (void)fprintf(
          table->out,
          "\nSize increased from %d to %d while sifting variable %d\n",
          previousSize, table->keysZ, var[i]);
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

  FREE(var);
  FREE(zdd_entry);

  return (1);

cuddZddSiftingOutOfMem:

  if (zdd_entry != NULL)
    FREE(zdd_entry);
  if (var != NULL)
    FREE(var);

  return (0);

} /* end of cuddZddSifting */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Swaps any two variables.]

  Description [Swaps any two variables. Returns the set of moves.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *zddSwapAny(DdManager *table, int x, int y) {
  Move *move, *moves;
  int tmp, size;
  int x_ref, y_ref;
  int x_next, y_next;
  int limit_size;

  if (x > y) { /* make x precede y */
    tmp = x;
    x = y;
    y = tmp;
  }

  x_ref = x;
  y_ref = y;

  x_next = cuddZddNextHigh(table, x);
  y_next = cuddZddNextLow(table, y);
  moves = NULL;
  limit_size = table->keysZ;

  for (;;) {
    if (x_next == y_next) { /* x < x_next = y_next < y */
      size = cuddZddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto zddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddSwapAnyOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;

      size = cuddZddSwapInPlace(table, y_next, y);
      if (size == 0)
        goto zddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddSwapAnyOutOfMem;
      move->x = y_next;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;

      size = cuddZddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto zddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddSwapAnyOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;

      tmp = x;
      x = y;
      y = tmp;

    } else if (x == y_next) { /* x = y_next < y = x_next */
      size = cuddZddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto zddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddSwapAnyOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;

      tmp = x;
      x = y;
      y = tmp;
    } else {
      size = cuddZddSwapInPlace(table, x, x_next);
      if (size == 0)
        goto zddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddSwapAnyOutOfMem;
      move->x = x;
      move->y = x_next;
      move->size = size;
      move->next = moves;
      moves = move;

      size = cuddZddSwapInPlace(table, y_next, y);
      if (size == 0)
        goto zddSwapAnyOutOfMem;
      move = (Move *)cuddDynamicAllocNode(table);
      if (move == NULL)
        goto zddSwapAnyOutOfMem;
      move->x = y_next;
      move->y = y;
      move->size = size;
      move->next = moves;
      moves = move;

      x = x_next;
      y = y_next;
    }

    x_next = cuddZddNextHigh(table, x);
    y_next = cuddZddNextLow(table, y);
    if (x_next > y_ref)
      break; /* if x == y_ref */

    if ((double)size > table->maxGrowth * (double)limit_size)
      break;
    if (size < limit_size)
      limit_size = size;
  }
  if (y_next >= x_ref) {
    size = cuddZddSwapInPlace(table, y_next, y);
    if (size == 0)
      goto zddSwapAnyOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto zddSwapAnyOutOfMem;
    move->x = y_next;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;
  }

  return (moves);

zddSwapAnyOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (NULL);

} /* end of zddSwapAny */

/**Function********************************************************************

  Synopsis    [Given xLow <= x <= xHigh moves x up and down between the
  boundaries.]

  Description [Given xLow <= x <= xHigh moves x up and down between the
  boundaries. Finds the best position and does the required changes.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddZddSiftingAux(DdManager *table, int x, int x_low, int x_high) {
  Move *move;
  Move *moveUp;   /* list of up move */
  Move *moveDown; /* list of down move */

  int initial_size;
  int result;

  initial_size = table->keysZ;

#ifdef DD_DEBUG
  assert(table->subtableZ[x].keys > 0);
#endif

  moveDown = NULL;
  moveUp = NULL;

  if (x == x_low) {
    moveDown = cuddZddSiftingDown(table, x, x_high, initial_size);
    /* after that point x --> x_high */
    if (moveDown == NULL)
      goto cuddZddSiftingAuxOutOfMem;
    result = cuddZddSiftingBackward(table, moveDown, initial_size);
    /* move backward and stop at best position */
    if (!result)
      goto cuddZddSiftingAuxOutOfMem;

  } else if (x == x_high) {
    moveUp = cuddZddSiftingUp(table, x, x_low, initial_size);
    /* after that point x --> x_low */
    if (moveUp == NULL)
      goto cuddZddSiftingAuxOutOfMem;
    result = cuddZddSiftingBackward(table, moveUp, initial_size);
    /* move backward and stop at best position */
    if (!result)
      goto cuddZddSiftingAuxOutOfMem;
  } else if ((x - x_low) > (x_high - x)) {
    /* must go down first:shorter */
    moveDown = cuddZddSiftingDown(table, x, x_high, initial_size);
    /* after that point x --> x_high */
    if (moveDown == NULL)
      goto cuddZddSiftingAuxOutOfMem;
    moveUp = cuddZddSiftingUp(table, moveDown->y, x_low, initial_size);
    if (moveUp == NULL)
      goto cuddZddSiftingAuxOutOfMem;
    result = cuddZddSiftingBackward(table, moveUp, initial_size);
    /* move backward and stop at best position */
    if (!result)
      goto cuddZddSiftingAuxOutOfMem;
  } else {
    moveUp = cuddZddSiftingUp(table, x, x_low, initial_size);
    /* after that point x --> x_high */
    if (moveUp == NULL)
      goto cuddZddSiftingAuxOutOfMem;
    moveDown = cuddZddSiftingDown(table, moveUp->x, x_high, initial_size);
    /* then move up */
    if (moveDown == NULL)
      goto cuddZddSiftingAuxOutOfMem;
    result = cuddZddSiftingBackward(table, moveDown, initial_size);
    /* move backward and stop at best position */
    if (!result)
      goto cuddZddSiftingAuxOutOfMem;
  }

  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (1);

cuddZddSiftingAuxOutOfMem:
  while (moveDown != NULL) {
    move = moveDown->next;
    cuddDeallocMove(table, moveDown);
    moveDown = move;
  }
  while (moveUp != NULL) {
    move = moveUp->next;
    cuddDeallocMove(table, moveUp);
    moveUp = move;
  }

  return (0);

} /* end of cuddZddSiftingAux */

/**Function********************************************************************

  Synopsis    [Sifts a variable up.]

  Description [Sifts a variable up. Moves y up until either it reaches
  the bound (x_low) or the size of the ZDD heap increases too much.
  Returns the set of moves in case of success; NULL if memory is full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *cuddZddSiftingUp(DdManager *table, int x, int x_low,
                              int initial_size) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limit_size = initial_size;

  moves = NULL;
  y = cuddZddNextLow(table, x);
  while (y >= x_low) {
    size = cuddZddSwapInPlace(table, y, x);
    if (size == 0)
      goto cuddZddSiftingUpOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto cuddZddSiftingUpOutOfMem;
    move->x = y;
    move->y = x;
    move->size = size;
    move->next = moves;
    moves = move;

    if ((double)size > (double)limit_size * table->maxGrowth)
      break;
    if (size < limit_size)
      limit_size = size;

    x = y;
    y = cuddZddNextLow(table, x);
  }
  return (moves);

cuddZddSiftingUpOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (NULL);

} /* end of cuddZddSiftingUp */

/**Function********************************************************************

  Synopsis    [Sifts a variable down.]

  Description [Sifts a variable down. Moves x down until either it
  reaches the bound (x_high) or the size of the ZDD heap increases too
  much. Returns the set of moves in case of success; NULL if memory is
  full.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static Move *cuddZddSiftingDown(DdManager *table, int x, int x_high,
                                int initial_size) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limit_size = initial_size;

  moves = NULL;
  y = cuddZddNextHigh(table, x);
  while (y <= x_high) {
    size = cuddZddSwapInPlace(table, x, y);
    if (size == 0)
      goto cuddZddSiftingDownOutOfMem;
    move = (Move *)cuddDynamicAllocNode(table);
    if (move == NULL)
      goto cuddZddSiftingDownOutOfMem;
    move->x = x;
    move->y = y;
    move->size = size;
    move->next = moves;
    moves = move;

    if ((double)size > (double)limit_size * table->maxGrowth)
      break;
    if (size < limit_size)
      limit_size = size;

    x = y;
    y = cuddZddNextHigh(table, x);
  }
  return (moves);

cuddZddSiftingDownOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (NULL);

} /* end of cuddZddSiftingDown */

/**Function********************************************************************

  Synopsis    [Given a set of moves, returns the ZDD heap to the position
  giving the minimum size.]

  Description [Given a set of moves, returns the ZDD heap to the
  position giving the minimum size. In case of ties, returns to the
  closest position giving the minimum size. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int cuddZddSiftingBackward(DdManager *table, Move *moves, int size) {
  int i;
  int i_best;
  Move *move;
  int res;

  /* Find the minimum size among moves. */
  i_best = -1;
  for (move = moves, i = 0; move != NULL; move = move->next, i++) {
    if (move->size < size) {
      i_best = i;
      size = move->size;
    }
  }

  for (move = moves, i = 0; move != NULL; move = move->next, i++) {
    if (i == i_best)
      break;
    res = cuddZddSwapInPlace(table, move->x, move->y);
    if (!res)
      return (0);
    if (i_best == -1 && res == size)
      break;
  }

  return (1);

} /* end of cuddZddSiftingBackward */

/**Function********************************************************************

  Synopsis    [Prepares the ZDD heap for dynamic reordering.]

  Description [Prepares the ZDD heap for dynamic reordering. Does
  garbage collection, to guarantee that there are no dead nodes;
  and clears the cache, which is invalidated by dynamic reordering.]

  SideEffects [None]

******************************************************************************/
static void zddReorderPreprocess(DdManager *table) {

  /* Clear the cache. */
  cuddCacheFlush(table);

  /* Eliminate dead nodes. Do not scan the cache again. */
  cuddGarbageCollect(table, 0);

  return;

} /* end of ddReorderPreprocess */

/**Function********************************************************************

  Synopsis    [Shrinks almost empty ZDD subtables at the end of reordering
  to guarantee that they have a reasonable load factor.]

  Description [Shrinks almost empty subtables at the end of reordering to
  guarantee that they have a reasonable load factor. However, if there many
  nodes are being reclaimed, then no resizing occurs. Returns 1 in case of
  success; 0 otherwise.]

  SideEffects [None]

******************************************************************************/
static int zddReorderPostprocess(DdManager *table) {
  int i, j, posn;
  DdNodePtr *nodelist, *oldnodelist;
  DdNode *node, *next;
  unsigned int slots, oldslots;
  extern DD_OOMFP MMoutOfMemory;
  DD_OOMFP saveHandler;

#ifdef DD_VERBOSE
  (void)fflush(table->out);
#endif

  /* If we have very many reclaimed nodes, we do not want to shrink
  ** the subtables, because this will lead to more garbage
  ** collections. More garbage collections mean shorter mean life for
  ** nodes with zero reference count; hence lower probability of finding
  ** a result in the cache.
  */
  if (table->reclaimed > table->allocated * 0.5)
    return (1);

  /* Resize subtables. */
  for (i = 0; i < table->sizeZ; i++) {
    int shift;
    oldslots = table->subtableZ[i].slots;
    if (oldslots < table->subtableZ[i].keys * DD_MAX_SUBTABLE_SPARSITY ||
        oldslots <= table->initSlots)
      continue;
    oldnodelist = table->subtableZ[i].nodelist;
    slots = oldslots >> 1;
    saveHandler = MMoutOfMemory;
    MMoutOfMemory = Cudd_OutOfMem;
    nodelist = ALLOC(DdNodePtr, slots);
    MMoutOfMemory = saveHandler;
    if (nodelist == NULL) {
      return (1);
    }
    table->subtableZ[i].nodelist = nodelist;
    table->subtableZ[i].slots = slots;
    table->subtableZ[i].shift++;
    table->subtableZ[i].maxKeys = slots * DD_MAX_SUBTABLE_DENSITY;
#ifdef DD_VERBOSE
    (void)fprintf(table->err, "shrunk layer %d (%d keys) from %d to %d slots\n",
                  i, table->subtableZ[i].keys, oldslots, slots);
#endif

    for (j = 0; (unsigned)j < slots; j++) {
      nodelist[j] = NULL;
    }
    shift = table->subtableZ[i].shift;
    for (j = 0; (unsigned)j < oldslots; j++) {
      node = oldnodelist[j];
      while (node != NULL) {
        next = node->next;
        posn = ddHash(cuddT(node), cuddE(node), shift);
        node->next = nodelist[posn];
        nodelist[posn] = node;
        node = next;
      }
    }
    FREE(oldnodelist);

    table->memused += (slots - oldslots) * sizeof(DdNode *);
    table->slots += slots - oldslots;
    table->minDead = (unsigned)(table->gcFrac * (double)table->slots);
    table->cacheSlack = (int)ddMin(table->maxCacheHard,
                                   DD_MAX_CACHE_TO_SLOTS_RATIO * table->slots) -
                        2 * (int)table->cacheSlots;
  }
  /* We don't look at the constant subtable, because it is not
  ** affected by reordering.
  */

  return (1);

} /* end of zddReorderPostprocess */

/**Function********************************************************************

  Synopsis    [Reorders ZDD variables according to a given permutation.]

  Description [Reorders ZDD variables according to a given permutation.
  The i-th permutation array contains the index of the variable that
  should be brought to the i-th level. zddShuffle assumes that no
  dead nodes are present.  The reordering is achieved by a series of
  upward sifts.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso []

******************************************************************************/
static int zddShuffle(DdManager *table, int *permutation) {
  int index;
  int level;
  int position;
  int numvars;
  int result;
#ifdef DD_STATS
  unsigned long localTime;
  int initialSize;
  int finalSize;
  int previousSize;
#endif

  zddTotalNumberSwapping = 0;
#ifdef DD_STATS
  localTime = util_cpu_time();
  initialSize = table->keysZ;
  (void)fprintf(table->out, "#:I_SHUFFLE %8d: initial size\n", initialSize);
#endif

  numvars = table->sizeZ;

  for (level = 0; level < numvars; level++) {
    index = permutation[level];
    position = table->permZ[index];
#ifdef DD_STATS
    previousSize = table->keysZ;
#endif
    result = zddSiftUp(table, position, level);
    if (!result)
      return (0);
#ifdef DD_STATS
    if (table->keysZ < (unsigned)previousSize) {
      (void)fprintf(table->out, "-");
    } else if (table->keysZ > (unsigned)previousSize) {
      (void)fprintf(table->out, "+"); /* should never happen */
    } else {
      (void)fprintf(table->out, "=");
    }
    fflush(table->out);
#endif
  }

#ifdef DD_STATS
  (void)fprintf(table->out, "\n");
  finalSize = table->keysZ;
  (void)fprintf(table->out, "#:F_SHUFFLE %8d: final size\n", finalSize);
  (void)fprintf(table->out, "#:T_SHUFFLE %8g: total time (sec)\n",
                ((double)(util_cpu_time() - localTime) / 1000.0));
  (void)fprintf(table->out, "#:N_SHUFFLE %8d: total swaps\n",
                zddTotalNumberSwapping);
#endif

  return (1);

} /* end of zddShuffle */

/**Function********************************************************************

  Synopsis    [Moves one ZDD variable up.]

  Description [Takes a ZDD variable from position x and sifts it up to
  position xLow;  xLow should be less than or equal to x.
  Returns 1 if successful; 0 otherwise]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int zddSiftUp(DdManager *table, int x, int xLow) {
  int y;
  int size;

  y = cuddZddNextLow(table, x);
  while (y >= xLow) {
    size = cuddZddSwapInPlace(table, y, x);
    if (size == 0) {
      return (0);
    }
    x = y;
    y = cuddZddNextLow(table, x);
  }
  return (1);

} /* end of zddSiftUp */

/**Function********************************************************************

  Synopsis    [Fixes the ZDD variable group tree after a shuffle.]

  Description [Fixes the ZDD variable group tree after a
  shuffle. Assumes that the order of the variables in a terminal node
  has not been changed.]

  SideEffects [Changes the ZDD variable group tree.]

  SeeAlso     []

******************************************************************************/
static void zddFixTree(DdManager *table, MtrNode *treenode) {
  if (treenode == NULL)
    return;
  treenode->low = ((int)treenode->index < table->sizeZ)
                      ? table->permZ[treenode->index]
                      : treenode->index;
  if (treenode->child != NULL) {
    zddFixTree(table, treenode->child);
  }
  if (treenode->younger != NULL)
    zddFixTree(table, treenode->younger);
  if (treenode->parent != NULL && treenode->low < treenode->parent->low) {
    treenode->parent->low = treenode->low;
    treenode->parent->index = treenode->index;
  }
  return;

} /* end of zddFixTree */

/**CFile***********************************************************************

  FileName    [cuddZddSetop.c]

  PackageName [cudd]

  Synopsis    [Set operations on ZDDs.]

  Description [External procedures included in this module:
                    <ul>
                    <li> Cudd_zddIte()
                    <li> Cudd_zddUnion()
                    <li> Cudd_zddIntersect()
                    <li> Cudd_zddDiff()
                    <li> Cudd_zddDiffConst()
                    <li> Cudd_zddSubset1()
                    <li> Cudd_zddSubset0()
                    <li> Cudd_zddChange()
                    </ul>
               Internal procedures included in this module:
                    <ul>
                    <li> cuddZddIte()
                    <li> cuddZddUnion()
                    <li> cuddZddIntersect()
                    <li> cuddZddDiff()
                    <li> cuddZddChangeAux()
                    <li> cuddZddSubset1()
                    <li> cuddZddSubset0()
                    </ul>
               Static procedures included in this module:
                    <ul>
                    <li> zdd_subset1_aux()
                    <li> zdd_subset0_aux()
                    <li> zddVarToConst()
                    </ul>
              ]

  SeeAlso     []

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
// static char rcsid[] DD_UNUSED = "$Id: cuddZddSetop.c,v 1.26 2012/02/05
// 01:07:19 fabio Exp $"; #endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdNode *zdd_subset1_aux(DdManager *zdd, DdNode *P, DdNode *zvar);
static DdNode *zdd_subset0_aux(DdManager *zdd, DdNode *P, DdNode *zvar);
static void zddVarToConst(DdNode *f, DdNode **gp, DdNode **hp, DdNode *base,
                          DdNode *empty);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Performs the inclusion test for ZDDs (P implies Q).]

  Description [Inclusion test for ZDDs (P implies Q). No new nodes are
  generated by this procedure. Returns empty if true;
  a valid pointer different from empty or DD_NON_CONSTANT otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_zddDiff]

******************************************************************************/
DdNode *Cudd_zddDiffConst(DdManager *zdd, DdNode *P, DdNode *Q) {
  int p_top, q_top;
  DdNode *empty = DD_ZERO(zdd), *t, *res;
  DdManager *table = zdd;

  statLine(zdd);
  if (P == empty)
    return (empty);
  if (Q == empty)
    return (P);
  if (P == Q)
    return (empty);

  /* Check cache.  The cache is shared by cuddZddDiff(). */
  res = cuddCacheLookup2Zdd(table, cuddZddDiff, P, Q);
  if (res != NULL)
    return (res);

  if (cuddIsConstant(P))
    p_top = P->index;
  else
    p_top = zdd->permZ[P->index];
  if (cuddIsConstant(Q))
    q_top = Q->index;
  else
    q_top = zdd->permZ[Q->index];
  if (p_top < q_top) {
    res = DD_NON_CONSTANT;
  } else if (p_top > q_top) {
    res = Cudd_zddDiffConst(zdd, P, cuddE(Q));
  } else {
    t = Cudd_zddDiffConst(zdd, cuddT(P), cuddT(Q));
    if (t != empty)
      res = DD_NON_CONSTANT;
    else
      res = Cudd_zddDiffConst(zdd, cuddE(P), cuddE(Q));
  }

  cuddCacheInsert2(table, cuddZddDiff, P, Q, res);

  return (res);

} /* end of Cudd_zddDiffConst */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_zddIte.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddZddIte(DdManager *dd, DdNode *f, DdNode *g, DdNode *h) {
  DdNode *tautology, *empty;
  DdNode *r, *Gv, *Gvn, *Hv, *Hvn, *t, *e;
  unsigned int topf, topg, toph, v, top;
  int index;

  statLine(dd);
  /* Trivial cases. */
  /* One variable cases. */
  if (f == (empty = DD_ZERO(dd))) { /* ITE(0,G,H) = H */
    return (h);
  }
  topf = cuddIZ(dd, f->index);
  topg = cuddIZ(dd, g->index);
  toph = cuddIZ(dd, h->index);
  v = ddMin(topg, toph);
  top = ddMin(topf, v);

  tautology = (top == CUDD_MAXINDEX) ? DD_ONE(dd) : dd->univ[top];
  if (f == tautology) { /* ITE(1,G,H) = G */
    return (g);
  }

  /* From now on, f is known to not be a constant. */
  zddVarToConst(f, &g, &h, tautology, empty);

  /* Check remaining one variable cases. */
  if (g == h) { /* ITE(F,G,G) = G */
    return (g);
  }

  if (g == tautology) { /* ITE(F,1,0) = F */
    if (h == empty)
      return (f);
  }

  /* Check cache. */
  r = cuddCacheLookupZdd(dd, DD_ZDD_ITE_TAG, f, g, h);
  if (r != NULL) {
    return (r);
  }

  /* Recompute these because they may have changed in zddVarToConst. */
  topg = cuddIZ(dd, g->index);
  toph = cuddIZ(dd, h->index);
  v = ddMin(topg, toph);

  if (topf < v) {
    r = cuddZddIte(dd, cuddE(f), g, h);
    if (r == NULL)
      return (NULL);
  } else if (topf > v) {
    if (topg > v) {
      Gvn = g;
      index = h->index;
    } else {
      Gvn = cuddE(g);
      index = g->index;
    }
    if (toph > v) {
      Hv = empty;
      Hvn = h;
    } else {
      Hv = cuddT(h);
      Hvn = cuddE(h);
    }
    e = cuddZddIte(dd, f, Gvn, Hvn);
    if (e == NULL)
      return (NULL);
    cuddRef(e);
    r = cuddZddGetNode(dd, index, Hv, e);
    if (r == NULL) {
      Cudd_RecursiveDerefZdd(dd, e);
      return (NULL);
    }
    cuddDeref(e);
  } else {
    index = f->index;
    if (topg > v) {
      Gv = empty;
      Gvn = g;
    } else {
      Gv = cuddT(g);
      Gvn = cuddE(g);
    }
    if (toph > v) {
      Hv = empty;
      Hvn = h;
    } else {
      Hv = cuddT(h);
      Hvn = cuddE(h);
    }
    e = cuddZddIte(dd, cuddE(f), Gvn, Hvn);
    if (e == NULL)
      return (NULL);
    cuddRef(e);
    t = cuddZddIte(dd, cuddT(f), Gv, Hv);
    if (t == NULL) {
      Cudd_RecursiveDerefZdd(dd, e);
      return (NULL);
    }
    cuddRef(t);
    r = cuddZddGetNode(dd, index, t, e);
    if (r == NULL) {
      Cudd_RecursiveDerefZdd(dd, e);
      Cudd_RecursiveDerefZdd(dd, t);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert(dd, DD_ZDD_ITE_TAG, f, g, h, r);

  return (r);

} /* end of cuddZddIte */

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddUnion.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddZddUnion(DdManager *zdd, DdNode *P, DdNode *Q) {
  int p_top, q_top;
  DdNode *empty = DD_ZERO(zdd), *t, *e, *res;
  DdManager *table = zdd;

  statLine(zdd);
  if (P == empty)
    return (Q);
  if (Q == empty)
    return (P);
  if (P == Q)
    return (P);

  /* Check cache */
  res = cuddCacheLookup2Zdd(table, cuddZddUnion, P, Q);
  if (res != NULL)
    return (res);

  if (cuddIsConstant(P))
    p_top = P->index;
  else
    p_top = zdd->permZ[P->index];
  if (cuddIsConstant(Q))
    q_top = Q->index;
  else
    q_top = zdd->permZ[Q->index];
  if (p_top < q_top) {
    e = cuddZddUnion(zdd, cuddE(P), Q);
    if (e == NULL)
      return (NULL);
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, cuddT(P), e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(table, e);
      return (NULL);
    }
    cuddDeref(e);
  } else if (p_top > q_top) {
    e = cuddZddUnion(zdd, P, cuddE(Q));
    if (e == NULL)
      return (NULL);
    cuddRef(e);
    res = cuddZddGetNode(zdd, Q->index, cuddT(Q), e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(table, e);
      return (NULL);
    }
    cuddDeref(e);
  } else {
    t = cuddZddUnion(zdd, cuddT(P), cuddT(Q));
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = cuddZddUnion(zdd, cuddE(P), cuddE(Q));
    if (e == NULL) {
      Cudd_RecursiveDerefZdd(table, t);
      return (NULL);
    }
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, t, e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(table, t);
      Cudd_RecursiveDerefZdd(table, e);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(table, cuddZddUnion, P, Q, res);

  return (res);

} /* end of cuddZddUnion */

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddIntersect.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddZddIntersect(DdManager *zdd, DdNode *P, DdNode *Q) {
  int p_top, q_top;
  DdNode *empty = DD_ZERO(zdd), *t, *e, *res;
  DdManager *table = zdd;

  statLine(zdd);
  if (P == empty)
    return (empty);
  if (Q == empty)
    return (empty);
  if (P == Q)
    return (P);

  /* Check cache. */
  res = cuddCacheLookup2Zdd(table, cuddZddIntersect, P, Q);
  if (res != NULL)
    return (res);

  if (cuddIsConstant(P))
    p_top = P->index;
  else
    p_top = zdd->permZ[P->index];
  if (cuddIsConstant(Q))
    q_top = Q->index;
  else
    q_top = zdd->permZ[Q->index];
  if (p_top < q_top) {
    res = cuddZddIntersect(zdd, cuddE(P), Q);
    if (res == NULL)
      return (NULL);
  } else if (p_top > q_top) {
    res = cuddZddIntersect(zdd, P, cuddE(Q));
    if (res == NULL)
      return (NULL);
  } else {
    t = cuddZddIntersect(zdd, cuddT(P), cuddT(Q));
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = cuddZddIntersect(zdd, cuddE(P), cuddE(Q));
    if (e == NULL) {
      Cudd_RecursiveDerefZdd(table, t);
      return (NULL);
    }
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, t, e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(table, t);
      Cudd_RecursiveDerefZdd(table, e);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(table, cuddZddIntersect, P, Q, res);

  return (res);

} /* end of cuddZddIntersect */

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddDiff.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddZddDiff(DdManager *zdd, DdNode *P, DdNode *Q) {
  int p_top, q_top;
  DdNode *empty = DD_ZERO(zdd), *t, *e, *res;
  DdManager *table = zdd;

  statLine(zdd);
  if (P == empty)
    return (empty);
  if (Q == empty)
    return (P);
  if (P == Q)
    return (empty);

  /* Check cache.  The cache is shared by Cudd_zddDiffConst(). */
  res = cuddCacheLookup2Zdd(table, cuddZddDiff, P, Q);
  if (res != NULL && res != DD_NON_CONSTANT)
    return (res);

  if (cuddIsConstant(P))
    p_top = P->index;
  else
    p_top = zdd->permZ[P->index];
  if (cuddIsConstant(Q))
    q_top = Q->index;
  else
    q_top = zdd->permZ[Q->index];
  if (p_top < q_top) {
    e = cuddZddDiff(zdd, cuddE(P), Q);
    if (e == NULL)
      return (NULL);
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, cuddT(P), e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(table, e);
      return (NULL);
    }
    cuddDeref(e);
  } else if (p_top > q_top) {
    res = cuddZddDiff(zdd, P, cuddE(Q));
    if (res == NULL)
      return (NULL);
  } else {
    t = cuddZddDiff(zdd, cuddT(P), cuddT(Q));
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = cuddZddDiff(zdd, cuddE(P), cuddE(Q));
    if (e == NULL) {
      Cudd_RecursiveDerefZdd(table, t);
      return (NULL);
    }
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, t, e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(table, t);
      Cudd_RecursiveDerefZdd(table, e);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(table, cuddZddDiff, P, Q, res);

  return (res);

} /* end of cuddZddDiff */

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddChange.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdNode *cuddZddChangeAux(DdManager *zdd, DdNode *P, DdNode *zvar) {
  int top_var, level;
  DdNode *res, *t, *e;
  DdNode *base = DD_ONE(zdd);
  DdNode *empty = DD_ZERO(zdd);

  statLine(zdd);
  if (P == empty)
    return (empty);
  if (P == base)
    return (zvar);

  /* Check cache. */
  res = cuddCacheLookup2Zdd(zdd, cuddZddChangeAux, P, zvar);
  if (res != NULL)
    return (res);

  top_var = zdd->permZ[P->index];
  level = zdd->permZ[zvar->index];

  if (top_var > level) {
    res = cuddZddGetNode(zdd, zvar->index, P, DD_ZERO(zdd));
    if (res == NULL)
      return (NULL);
  } else if (top_var == level) {
    res = cuddZddGetNode(zdd, zvar->index, cuddE(P), cuddT(P));
    if (res == NULL)
      return (NULL);
  } else {
    t = cuddZddChangeAux(zdd, cuddT(P), zvar);
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = cuddZddChangeAux(zdd, cuddE(P), zvar);
    if (e == NULL) {
      Cudd_RecursiveDerefZdd(zdd, t);
      return (NULL);
    }
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, t, e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(zdd, t);
      Cudd_RecursiveDerefZdd(zdd, e);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(zdd, cuddZddChangeAux, P, zvar, res);

  return (res);

} /* end of cuddZddChangeAux */

/**Function********************************************************************

  Synopsis [Computes the positive cofactor of a ZDD w.r.t. a variable.]

  Description [Computes the positive cofactor of a ZDD w.r.t. a
  variable. In terms of combinations, the result is the set of all
  combinations in which the variable is asserted. Returns a pointer to
  the result if successful; NULL otherwise. cuddZddSubset1 performs
  the same function as Cudd_zddSubset1, but does not restart if
  reordering has taken place. Therefore it can be called from within a
  recursive procedure.]

  SideEffects [None]

  SeeAlso     [cuddZddSubset0 Cudd_zddSubset1]

******************************************************************************/
DdNode *cuddZddSubset1(DdManager *dd, DdNode *P, int var) {
  DdNode *zvar, *r;
  DdNode *base, *empty;

  base = DD_ONE(dd);
  empty = DD_ZERO(dd);

  zvar = cuddUniqueInterZdd(dd, var, base, empty);
  if (zvar == NULL) {
    return (NULL);
  } else {
    cuddRef(zvar);
    r = zdd_subset1_aux(dd, P, zvar);
    if (r == NULL) {
      Cudd_RecursiveDerefZdd(dd, zvar);
      return (NULL);
    }
    cuddRef(r);
    Cudd_RecursiveDerefZdd(dd, zvar);
  }

  cuddDeref(r);
  return (r);

} /* end of cuddZddSubset1 */

/**Function********************************************************************

  Synopsis [Computes the negative cofactor of a ZDD w.r.t. a variable.]

  Description [Computes the negative cofactor of a ZDD w.r.t. a
  variable. In terms of combinations, the result is the set of all
  combinations in which the variable is negated. Returns a pointer to
  the result if successful; NULL otherwise. cuddZddSubset0 performs
  the same function as Cudd_zddSubset0, but does not restart if
  reordering has taken place. Therefore it can be called from within a
  recursive procedure.]

  SideEffects [None]

  SeeAlso     [cuddZddSubset1 Cudd_zddSubset0]

******************************************************************************/
DdNode *cuddZddSubset0(DdManager *dd, DdNode *P, int var) {
  DdNode *zvar, *r;
  DdNode *base, *empty;

  base = DD_ONE(dd);
  empty = DD_ZERO(dd);

  zvar = cuddUniqueInterZdd(dd, var, base, empty);
  if (zvar == NULL) {
    return (NULL);
  } else {
    cuddRef(zvar);
    r = zdd_subset0_aux(dd, P, zvar);
    if (r == NULL) {
      Cudd_RecursiveDerefZdd(dd, zvar);
      return (NULL);
    }
    cuddRef(r);
    Cudd_RecursiveDerefZdd(dd, zvar);
  }

  cuddDeref(r);
  return (r);

} /* end of cuddZddSubset0 */

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddSubset1.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *zdd_subset1_aux(DdManager *zdd, DdNode *P, DdNode *zvar) {
  int top_var, level;
  DdNode *res, *t, *e;
  DdNode *empty;

  statLine(zdd);
  empty = DD_ZERO(zdd);

  /* Check cache. */
  res = cuddCacheLookup2Zdd(zdd, zdd_subset1_aux, P, zvar);
  if (res != NULL)
    return (res);

  if (cuddIsConstant(P)) {
    res = empty;
    cuddCacheInsert2(zdd, zdd_subset1_aux, P, zvar, res);
    return (res);
  }

  top_var = zdd->permZ[P->index];
  level = zdd->permZ[zvar->index];

  if (top_var > level) {
    res = empty;
  } else if (top_var == level) {
    res = cuddT(P);
  } else {
    t = zdd_subset1_aux(zdd, cuddT(P), zvar);
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = zdd_subset1_aux(zdd, cuddE(P), zvar);
    if (e == NULL) {
      Cudd_RecursiveDerefZdd(zdd, t);
      return (NULL);
    }
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, t, e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(zdd, t);
      Cudd_RecursiveDerefZdd(zdd, e);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(zdd, zdd_subset1_aux, P, zvar, res);

  return (res);

} /* end of zdd_subset1_aux */

/**Function********************************************************************

  Synopsis [Performs the recursive step of Cudd_zddSubset0.]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static DdNode *zdd_subset0_aux(DdManager *zdd, DdNode *P, DdNode *zvar) {
  int top_var, level;
  DdNode *res, *t, *e;

  statLine(zdd);

  /* Check cache. */
  res = cuddCacheLookup2Zdd(zdd, zdd_subset0_aux, P, zvar);
  if (res != NULL)
    return (res);

  if (cuddIsConstant(P)) {
    res = P;
    cuddCacheInsert2(zdd, zdd_subset0_aux, P, zvar, res);
    return (res);
  }

  top_var = zdd->permZ[P->index];
  level = zdd->permZ[zvar->index];

  if (top_var > level) {
    res = P;
  } else if (top_var == level) {
    res = cuddE(P);
  } else {
    t = zdd_subset0_aux(zdd, cuddT(P), zvar);
    if (t == NULL)
      return (NULL);
    cuddRef(t);
    e = zdd_subset0_aux(zdd, cuddE(P), zvar);
    if (e == NULL) {
      Cudd_RecursiveDerefZdd(zdd, t);
      return (NULL);
    }
    cuddRef(e);
    res = cuddZddGetNode(zdd, P->index, t, e);
    if (res == NULL) {
      Cudd_RecursiveDerefZdd(zdd, t);
      Cudd_RecursiveDerefZdd(zdd, e);
      return (NULL);
    }
    cuddDeref(t);
    cuddDeref(e);
  }

  cuddCacheInsert2(zdd, zdd_subset0_aux, P, zvar, res);

  return (res);

} /* end of zdd_subset0_aux */

/**Function********************************************************************

  Synopsis    [Replaces variables with constants if possible (part of
  canonical form).]

  Description []

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static void zddVarToConst(DdNode *f, DdNode **gp, DdNode **hp, DdNode *base,
                          DdNode *empty) {
  DdNode *g = *gp;
  DdNode *h = *hp;

  if (f == g) { /* ITE(F,F,H) = ITE(F,1,H) = F + H */
    *gp = base;
  }

  if (f == h) { /* ITE(F,G,F) = ITE(F,G,0) = F * G */
    *hp = empty;
  }

} /* end of zddVarToConst */

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

//#ifndef lint
// static char rcsid[] DD_UNUSED = "$Id: cuddZddSymm.c,v 1.31 2012/02/05
// 01:07:19 fabio Exp $"; #endif

extern int *zdd_entry;

extern int zddTotalNumberSwapping;

static DdNode *empty;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int cuddZddSymmSiftingAux(DdManager *table, int x, int x_low,
                                 int x_high);
static int cuddZddSymmSiftingConvAux(DdManager *table, int x, int x_low,
                                     int x_high);
static Move *cuddZddSymmSifting_up(DdManager *table, int x, int x_low,
                                   int initial_size);
static Move *cuddZddSymmSifting_down(DdManager *table, int x, int x_high,
                                     int initial_size);
static int cuddZddSymmSiftingBackward(DdManager *table, Move *moves, int size);
static int zdd_group_move(DdManager *table, int x, int y, Move **moves);
static int zdd_group_move_backward(DdManager *table, int x, int y);
static void cuddZddSymmSummary(DdManager *table, int lower, int upper,
                               int *symvars, int *symgroups);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

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
int cuddZddSymmCheck(DdManager *table, int x, int y) {
  int i;
  DdNode *f, *f0, *f1, *f01, *f00, *f11, *f10;
  int yindex;
  int xsymmy = 1;
  int xsymmyp = 1;
  int arccount = 0;
  int TotalRefCount = 0;
  int symm_found;

  empty = table->zero;

  yindex = table->invpermZ[y];
  for (i = table->subtableZ[x].slots - 1; i >= 0; i--) {
    f = table->subtableZ[x].nodelist[i];
    while (f != NULL) {
      /* Find f1, f0, f11, f10, f01, f00 */
      f1 = cuddT(f);
      f0 = cuddE(f);
      if ((int)f1->index == yindex) {
        f11 = cuddT(f1);
        f10 = cuddE(f1);
        if (f10 != empty)
          arccount++;
      } else {
        if ((int)f0->index != yindex) {
          return (0); /* f bypasses layer y */
        }
        f11 = empty;
        f10 = f1;
      }
      if ((int)f0->index == yindex) {
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
        return (0);

      f = f->next;
    } /* for each element of the collision list */
  }   /* for each slot of the subtable */

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
    (void)fprintf(table->out, "Found symmetry! x =%d\ty = %d\tPos(%d,%d)\n",
                  xindex, yindex, x, y);
  }
#endif

  return (symm_found);

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
int cuddZddSymmSifting(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int nvars;
  int x;
  int result;
  int symvars;
  int symgroups;
  int iteration;
#ifdef DD_STATS
  int previousSize;
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
    if (x < lower || x > upper)
      continue;
    if (table->subtableZ[x].next == (unsigned)x) {
      result = cuddZddSymmSiftingAux(table, x, lower, upper);
      if (!result)
        goto cuddZddSymmSiftingOutOfMem;
#ifdef DD_STATS
      if (table->keysZ < (unsigned)previousSize) {
        (void)fprintf(table->out, "-");
      } else if (table->keysZ > (unsigned)previousSize) {
        (void)fprintf(table->out, "+");
#ifdef DD_VERBOSE
        (void)fprintf(
            table->out,
            "\nSize increased from %d to %d while sifting variable %d\n",
            previousSize, table->keysZ, var[i]);
#endif
      } else {
        (void)fprintf(table->out, "=");
      }
      fflush(table->out);
#endif
    }
  }

  FREE(var);
  FREE(zdd_entry);

  cuddZddSymmSummary(table, lower, upper, &symvars, &symgroups);

#ifdef DD_STATS
  (void)fprintf(table->out, "\n#:S_SIFTING %8d: symmetric variables\n",
                symvars);
  (void)fprintf(table->out, "#:G_SIFTING %8d: symmetric groups\n", symgroups);
#endif

  return (1 + symvars);

cuddZddSymmSiftingOutOfMem:

  if (zdd_entry != NULL)
    FREE(zdd_entry);
  if (var != NULL)
    FREE(var);

  return (0);

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
int cuddZddSymmSiftingConv(DdManager *table, int lower, int upper) {
  int i;
  int *var;
  int nvars;
  int initialSize;
  int x;
  int result;
  int symvars;
  int symgroups;
  int classes;
  int iteration;
#ifdef DD_STATS
  int previousSize;
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
    if (x < lower || x > upper)
      continue;
    /* Only sift if not in symmetry group already. */
    if (table->subtableZ[x].next == (unsigned)x) {
#ifdef DD_STATS
      previousSize = table->keysZ;
#endif
      result = cuddZddSymmSiftingAux(table, x, lower, upper);
      if (!result)
        goto cuddZddSymmSiftingConvOutOfMem;
#ifdef DD_STATS
      if (table->keysZ < (unsigned)previousSize) {
        (void)fprintf(table->out, "-");
      } else if (table->keysZ > (unsigned)previousSize) {
        (void)fprintf(table->out, "+");
#ifdef DD_VERBOSE
        (void)fprintf(
            table->out,
            "\nSize increased from %d to %d while sifting variable %d\n",
            previousSize, table->keysZ, var[i]);
#endif
      } else {
        (void)fprintf(table->out, "=");
      }
      fflush(table->out);
#endif
    }
  }

  /* Sifting now until convergence. */
  while ((unsigned)initialSize > table->keysZ) {
    initialSize = table->keysZ;
#ifdef DD_STATS
    (void)fprintf(table->out, "\n");
#endif
    /* Here we consider only one representative for each symmetry class. */
    for (x = lower, classes = 0; x <= upper; x++, classes++) {
      while ((unsigned)x < table->subtableZ[x].next)
        x = table->subtableZ[x].next;
      /* Here x is the largest index in a group.
      ** Groups consists of adjacent variables.
      ** Hence, the next increment of x will move it to a new group.
      */
      i = table->invpermZ[x];
      zdd_entry[i] = table->subtableZ[x].keys;
      var[classes] = i;
    }

    qsort((void *)var, classes, sizeof(int), (DD_QSFP)cuddZddUniqueCompare);

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
      if ((unsigned)x >= table->subtableZ[x].next) {
#ifdef DD_STATS
        previousSize = table->keysZ;
#endif
        result = cuddZddSymmSiftingConvAux(table, x, lower, upper);
        if (!result)
          goto cuddZddSymmSiftingConvOutOfMem;
#ifdef DD_STATS
        if (table->keysZ < (unsigned)previousSize) {
          (void)fprintf(table->out, "-");
        } else if (table->keysZ > (unsigned)previousSize) {
          (void)fprintf(table->out, "+");
#ifdef DD_VERBOSE
          (void)fprintf(
              table->out,
              "\nSize increased from %d to %d while sifting variable %d\n",
              previousSize, table->keysZ, var[i]);
#endif
        } else {
          (void)fprintf(table->out, "=");
        }
        fflush(table->out);
#endif
      }
    } /* for */
  }

  cuddZddSymmSummary(table, lower, upper, &symvars, &symgroups);

#ifdef DD_STATS
  (void)fprintf(table->out, "\n#:S_SIFTING %8d: symmetric variables\n",
                symvars);
  (void)fprintf(table->out, "#:G_SIFTING %8d: symmetric groups\n", symgroups);
#endif

  FREE(var);
  FREE(zdd_entry);

  return (1 + symvars);

cuddZddSymmSiftingConvOutOfMem:

  if (zdd_entry != NULL)
    FREE(zdd_entry);
  if (var != NULL)
    FREE(var);

  return (0);

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
static int cuddZddSymmSiftingAux(DdManager *table, int x, int x_low,
                                 int x_high) {
  Move *move;
  Move *move_up;   /* list of up move */
  Move *move_down; /* list of down move */
  int initial_size;
  int result;
  int i;
  int topbot; /* index to either top or bottom of symmetry group */
  int init_group_size, final_group_size;

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
    while ((unsigned)topbot < table->subtableZ[topbot].next)
      topbot = table->subtableZ[topbot].next;

    table->subtableZ[topbot].next = table->subtableZ[i].next;
    table->subtableZ[i].next = i + 1;
    i = topbot - 1; /* add 1 for i++,
                       new i is bottom of symm group */
  }

  /* Now x maybe in the middle of a symmetry group. */
  if (x == x_low) { /* Sift down */
    /* Find bottom of x's symm group */
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;

    i = table->subtableZ[x].next;
    init_group_size = x - i + 1;

    move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
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
      while ((unsigned)i < table->subtableZ[i].next) {
        i = table->subtableZ[i].next;
      }
      final_group_size = i - x + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetry groups detected,
           return to best position */
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      } else {
        initial_size = table->keysZ;
        move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      /* move backward and stop at best position */
    }
    if (!result)
      goto cuddZddSymmSiftingAuxOutOfMem;
  } else if (x == x_high) { /* Sift up */
    /* Find top of x's symm group */
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;
    x = table->subtableZ[x].next;

    i = x;
    while ((unsigned)i < table->subtableZ[i].next) {
      i = table->subtableZ[i].next;
    }
    init_group_size = i - x + 1;

    move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
    /* after that point x --> x_low, unless early term */
    if (move_up == ZDD_MV_OOM)
      goto cuddZddSymmSiftingAuxOutOfMem;

    if (move_up == NULL || table->subtableZ[move_up->x].next != move_up->x) {
      /* symmetry detected may have to make another complete
      pass */
      if (move_up != NULL)
        x = move_up->x;
      else {
        while ((unsigned)x < table->subtableZ[x].next)
          x = table->subtableZ[x].next;
      }
      i = table->subtableZ[x].next;
      final_group_size = x - i + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetry groups detected,
           return to best position */
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      } else {
        initial_size = table->keysZ;
        move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      /* move backward and stop at best position */
    }
    if (!result)
      goto cuddZddSymmSiftingAuxOutOfMem;
  } else if ((x - x_low) > (x_high - x)) { /* must go down first:
                                                shorter */
    /* Find bottom of x's symm group */
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;

    move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
    /* after that point x --> x_high, unless early term */
    if (move_down == ZDD_MV_OOM)
      goto cuddZddSymmSiftingAuxOutOfMem;

    if (move_down != NULL) {
      x = move_down->y;
    } else {
      x = table->subtableZ[x].next;
    }
    i = x;
    while ((unsigned)i < table->subtableZ[i].next) {
      i = table->subtableZ[i].next;
    }
    init_group_size = i - x + 1;

    move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
    if (move_up == ZDD_MV_OOM)
      goto cuddZddSymmSiftingAuxOutOfMem;

    if (move_up == NULL || table->subtableZ[move_up->x].next != move_up->x) {
      /* symmetry detected may have to make another complete
         pass */
      if (move_up != NULL) {
        x = move_up->x;
      } else {
        while ((unsigned)x < table->subtableZ[x].next)
          x = table->subtableZ[x].next;
      }
      i = table->subtableZ[x].next;
      final_group_size = x - i + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetry groups detected,
           return to best position */
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      } else {
        while (move_down != NULL) {
          move = move_down->next;
          cuddDeallocMove(table, move_down);
          move_down = move;
        }
        initial_size = table->keysZ;
        move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      /* move backward and stop at best position */
    }
    if (!result)
      goto cuddZddSymmSiftingAuxOutOfMem;
  } else { /* moving up first:shorter */
    /* Find top of x's symmetry group */
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;
    x = table->subtableZ[x].next;

    move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
    /* after that point x --> x_high, unless early term */
    if (move_up == ZDD_MV_OOM)
      goto cuddZddSymmSiftingAuxOutOfMem;

    if (move_up != NULL) {
      x = move_up->x;
    } else {
      while ((unsigned)x < table->subtableZ[x].next)
        x = table->subtableZ[x].next;
    }
    i = table->subtableZ[x].next;
    init_group_size = x - i + 1;

    move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
    if (move_down == ZDD_MV_OOM)
      goto cuddZddSymmSiftingAuxOutOfMem;

    if (move_down == NULL ||
        table->subtableZ[move_down->y].next != move_down->y) {
      /* symmetry detected may have to make another complete
         pass */
      if (move_down != NULL) {
        x = move_down->y;
      } else {
        x = table->subtableZ[x].next;
      }
      i = x;
      while ((unsigned)i < table->subtableZ[i].next) {
        i = table->subtableZ[i].next;
      }
      final_group_size = i - x + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetries detected,
           go back to best position */
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      } else {
        while (move_up != NULL) {
          move = move_up->next;
          cuddDeallocMove(table, move_up);
          move_up = move;
        }
        initial_size = table->keysZ;
        move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
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

  return (1);

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

  return (0);

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
static int cuddZddSymmSiftingConvAux(DdManager *table, int x, int x_low,
                                     int x_high) {
  Move *move;
  Move *move_up;   /* list of up move */
  Move *move_down; /* list of down move */
  int initial_size;
  int result;
  int i;
  int init_group_size, final_group_size;

  initial_size = table->keysZ;

  move_down = NULL;
  move_up = NULL;

  if (x == x_low) { /* Sift down */
    i = table->subtableZ[x].next;
    init_group_size = x - i + 1;

    move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
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
        while ((unsigned)x < table->subtableZ[x].next)
          x = table->subtableZ[x].next;
        x = table->subtableZ[x].next;
      }
      i = x;
      while ((unsigned)i < table->subtableZ[i].next) {
        i = table->subtableZ[i].next;
      }
      final_group_size = i - x + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetries detected,
           go back to best position */
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      } else {
        initial_size = table->keysZ;
        move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      /* move backward and stop at best position */
    }
    if (!result)
      goto cuddZddSymmSiftingConvAuxOutOfMem;
  } else if (x == x_high) { /* Sift up */
    /* Find top of x's symm group */
    while ((unsigned)x < table->subtableZ[x].next)
      x = table->subtableZ[x].next;
    x = table->subtableZ[x].next;

    i = x;
    while ((unsigned)i < table->subtableZ[i].next) {
      i = table->subtableZ[i].next;
    }
    init_group_size = i - x + 1;

    move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
    /* after that point x --> x_low, unless early term */
    if (move_up == ZDD_MV_OOM)
      goto cuddZddSymmSiftingConvAuxOutOfMem;

    if (move_up == NULL || table->subtableZ[move_up->x].next != move_up->x) {
      /* symmetry detected may have to make another complete
         pass */
      if (move_up != NULL)
        x = move_up->x;
      else {
        while ((unsigned)x < table->subtableZ[x].next)
          x = table->subtableZ[x].next;
      }
      i = table->subtableZ[x].next;
      final_group_size = x - i + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetry groups detected,
           return to best position */
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      } else {
        initial_size = table->keysZ;
        move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      /* move backward and stop at best position */
    }
    if (!result)
      goto cuddZddSymmSiftingConvAuxOutOfMem;
  } else if ((x - x_low) > (x_high - x)) { /* must go down first:
                                                shorter */
    move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
    /* after that point x --> x_high */
    if (move_down == ZDD_MV_OOM)
      goto cuddZddSymmSiftingConvAuxOutOfMem;

    if (move_down != NULL) {
      x = move_down->y;
    } else {
      while ((unsigned)x < table->subtableZ[x].next)
        x = table->subtableZ[x].next;
      x = table->subtableZ[x].next;
    }
    i = x;
    while ((unsigned)i < table->subtableZ[i].next) {
      i = table->subtableZ[i].next;
    }
    init_group_size = i - x + 1;

    move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
    if (move_up == ZDD_MV_OOM)
      goto cuddZddSymmSiftingConvAuxOutOfMem;

    if (move_up == NULL || table->subtableZ[move_up->x].next != move_up->x) {
      /* symmetry detected may have to make another complete
         pass */
      if (move_up != NULL) {
        x = move_up->x;
      } else {
        while ((unsigned)x < table->subtableZ[x].next)
          x = table->subtableZ[x].next;
      }
      i = table->subtableZ[x].next;
      final_group_size = x - i + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetry groups detected,
           return to best position */
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      } else {
        while (move_down != NULL) {
          move = move_down->next;
          cuddDeallocMove(table, move_down);
          move_down = move;
        }
        initial_size = table->keysZ;
        move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      /* move backward and stop at best position */
    }
    if (!result)
      goto cuddZddSymmSiftingConvAuxOutOfMem;
  } else { /* moving up first:shorter */
    /* Find top of x's symmetry group */
    x = table->subtableZ[x].next;

    move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
    /* after that point x --> x_high, unless early term */
    if (move_up == ZDD_MV_OOM)
      goto cuddZddSymmSiftingConvAuxOutOfMem;

    if (move_up != NULL) {
      x = move_up->x;
    } else {
      while ((unsigned)x < table->subtableZ[x].next)
        x = table->subtableZ[x].next;
    }
    i = table->subtableZ[x].next;
    init_group_size = x - i + 1;

    move_down = cuddZddSymmSifting_down(table, x, x_high, initial_size);
    if (move_down == ZDD_MV_OOM)
      goto cuddZddSymmSiftingConvAuxOutOfMem;

    if (move_down == NULL ||
        table->subtableZ[move_down->y].next != move_down->y) {
      /* symmetry detected may have to make another complete
         pass */
      if (move_down != NULL) {
        x = move_down->y;
      } else {
        while ((unsigned)x < table->subtableZ[x].next)
          x = table->subtableZ[x].next;
        x = table->subtableZ[x].next;
      }
      i = x;
      while ((unsigned)i < table->subtableZ[i].next) {
        i = table->subtableZ[i].next;
      }
      final_group_size = i - x + 1;

      if (init_group_size == final_group_size) {
        /* No new symmetries detected,
           go back to best position */
        result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
      } else {
        while (move_up != NULL) {
          move = move_up->next;
          cuddDeallocMove(table, move_up);
          move_up = move;
        }
        initial_size = table->keysZ;
        move_up = cuddZddSymmSifting_up(table, x, x_low, initial_size);
        result = cuddZddSymmSiftingBackward(table, move_up, initial_size);
      }
    } else {
      result = cuddZddSymmSiftingBackward(table, move_down, initial_size);
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

  return (1);

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

  return (0);

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
static Move *cuddZddSymmSifting_up(DdManager *table, int x, int x_low,
                                   int initial_size) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limit_size = initial_size;
  int i, gytop;

  moves = NULL;
  y = cuddZddNextLow(table, x);
  while (y >= x_low) {
    gytop = table->subtableZ[y].next;
    if (cuddZddSymmCheck(table, y, x)) {
      /* Symmetry found, attach symm groups */
      table->subtableZ[y].next = x;
      i = table->subtableZ[x].next;
      while (table->subtableZ[i].next != (unsigned)x)
        i = table->subtableZ[i].next;
      table->subtableZ[i].next = gytop;
    } else if ((table->subtableZ[x].next == (unsigned)x) &&
               (table->subtableZ[y].next == (unsigned)y)) {
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
      if ((double)size > (double)limit_size * table->maxGrowth)
        return (moves);
      if (size < limit_size)
        limit_size = size;
    } else { /* Group move */
      size = zdd_group_move(table, y, x, &moves);
      if ((double)size > (double)limit_size * table->maxGrowth)
        return (moves);
      if (size < limit_size)
        limit_size = size;
    }
    x = gytop;
    y = cuddZddNextLow(table, x);
  }

  return (moves);

cuddZddSymmSifting_upOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (ZDD_MV_OOM);

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
static Move *cuddZddSymmSifting_down(DdManager *table, int x, int x_high,
                                     int initial_size) {
  Move *moves;
  Move *move;
  int y;
  int size;
  int limit_size = initial_size;
  int i, gxtop, gybot;

  moves = NULL;
  y = cuddZddNextHigh(table, x);
  while (y <= x_high) {
    gybot = table->subtableZ[y].next;
    while (table->subtableZ[gybot].next != (unsigned)y)
      gybot = table->subtableZ[gybot].next;
    if (cuddZddSymmCheck(table, x, y)) {
      /* Symmetry found, attach symm groups */
      gxtop = table->subtableZ[x].next;
      table->subtableZ[x].next = y;
      i = table->subtableZ[y].next;
      while (table->subtableZ[i].next != (unsigned)y)
        i = table->subtableZ[i].next;
      table->subtableZ[i].next = gxtop;
    } else if ((table->subtableZ[x].next == (unsigned)x) &&
               (table->subtableZ[y].next == (unsigned)y)) {
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
      if ((double)size > (double)limit_size * table->maxGrowth)
        return (moves);
      if (size < limit_size)
        limit_size = size;
      x = y;
      y = cuddZddNextHigh(table, x);
    } else { /* Group move */
      size = zdd_group_move(table, x, y, &moves);
      if ((double)size > (double)limit_size * table->maxGrowth)
        return (moves);
      if (size < limit_size)
        limit_size = size;
    }
    x = gybot;
    y = cuddZddNextHigh(table, x);
  }

  return (moves);

cuddZddSymmSifting_downOutOfMem:
  while (moves != NULL) {
    move = moves->next;
    cuddDeallocMove(table, moves);
    moves = move;
  }
  return (ZDD_MV_OOM);

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
static int cuddZddSymmSiftingBackward(DdManager *table, Move *moves, int size) {
  int i;
  int i_best;
  Move *move;
  int res;

  i_best = -1;
  for (move = moves, i = 0; move != NULL; move = move->next, i++) {
    if (move->size < size) {
      i_best = i;
      size = move->size;
    }
  }

  for (move = moves, i = 0; move != NULL; move = move->next, i++) {
    if (i == i_best)
      break;
    if ((table->subtableZ[move->x].next == move->x) &&
        (table->subtableZ[move->y].next == move->y)) {
      res = cuddZddSwapInPlace(table, move->x, move->y);
      if (!res)
        return (0);
    } else { /* Group move necessary */
      res = zdd_group_move_backward(table, move->x, move->y);
    }
    if (i_best == -1 && res == size)
      break;
  }

  return (1);

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
static int zdd_group_move(DdManager *table, int x, int y, Move **moves) {
  Move *move;
  int size;
  int i, temp, gxtop, gxbot, gybot, yprev;
  int swapx, swapy;

#ifdef DD_DEBUG
  assert(x < y); /* we assume that x < y */
#endif
  /* Find top and bottom for the two groups. */
  gxtop = table->subtableZ[x].next;
  gxbot = x;
  gybot = table->subtableZ[y].next;
  while (table->subtableZ[gybot].next != (unsigned)y)
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
        if (table->subtableZ[i].next == (unsigned)x) {
          table->subtableZ[i].next = y;
          break;
        } else {
          i = table->subtableZ[i].next;
        }
      }
      if (table->subtableZ[y].next != (unsigned)y) {
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
    if (table->subtableZ[y].next <= (unsigned)y) {
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

  return (table->keysZ);

zdd_group_moveOutOfMem:
  while (*moves != NULL) {
    move = (*moves)->next;
    cuddDeallocMove(table, *moves);
    *moves = move;
  }
  return (0);

} /* end of zdd_group_move */

/**Function********************************************************************

  Synopsis [Undoes the swap of two groups.]

  Description [Undoes the swap of two groups. x is assumed to be the
  bottom variable of the first group. y is assumed to be the top
  variable of the second group.  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
static int zdd_group_move_backward(DdManager *table, int x, int y) {
  int size;
  int i, temp, gxtop, gxbot, gybot, yprev;

#ifdef DD_DEBUG
  assert(x < y); /* we assume that x < y */
#endif
  /* Find top and bottom of the two groups. */
  gxtop = table->subtableZ[x].next;
  gxbot = x;
  gybot = table->subtableZ[y].next;
  while (table->subtableZ[gybot].next != (unsigned)y)
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
        if (table->subtableZ[i].next == (unsigned)x) {
          table->subtableZ[i].next = y;
          break;
        } else {
          i = table->subtableZ[i].next;
        }
      }
      if (table->subtableZ[y].next != (unsigned)y) {
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
        return (0);
      y = x;
      x--;
    } /* while y > gxtop */

    /* Trying to find the next y. */
    if (table->subtableZ[y].next <= (unsigned)y) {
      gybot = y;
    } else {
      y = table->subtableZ[y].next;
    }

    yprev = gxtop;
    gxtop++;
    gxbot++;
    x = gxbot;
  } /* while x <= y, end of group movement backward */

  return (size);

} /* end of zdd_group_move_backward */

/**Function********************************************************************

  Synopsis    [Counts numbers of symmetric variables and symmetry
  groups.]

  Description []

  SideEffects [None]

******************************************************************************/
static void cuddZddSymmSummary(DdManager *table, int lower, int upper,
                               int *symvars, int *symgroups) {
  int i, x, gbot;
  int TotalSymm = 0;
  int TotalSymmGroups = 0;

  for (i = lower; i <= upper; i++) {
    if (table->subtableZ[i].next != (unsigned)i) {
      TotalSymmGroups++;
      x = i;
      do {
        TotalSymm++;
        gbot = x;
        x = table->subtableZ[x].next;
      } while (x != i);
#ifdef DD_DEBUG
      assert(table->subtableZ[gbot].next == (unsigned)i);
#endif
      i = gbot;
    }
  }
  *symvars = TotalSymm;
  *symgroups = TotalSymmGroups;

  return;

} /* end of cuddZddSymmSummary */
