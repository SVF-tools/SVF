/**CFile**********************************************************************

  FileName     [dddmpNodeCnf.c]

  PackageName  [dddmp]

  Synopsis     [Functions to handle BDD node infos and numbering
    while storing a CNF formula from a BDD or an array of BDDs]

  Description  [Functions to handle BDD node infos and numbering
    while storing a CNF formula from a BDD or an array of BDDs.
    ]

  Author       [Gianpiero Cabodi and Stefano Quer]

  Copyright    [
    Copyright (c) 2004 by Politecnico di Torino.
    All Rights Reserved. This software is for educational purposes only.
    Permission is given to academic institutions to use, copy, and modify
    this software and its documentation provided that this introductory
    message is not removed, that this software and its documentation is
    used for the institutions' internal research and educational purposes,
    and that no monies are exchanged. No guarantee is expressed or implied
    by the distribution of this code.
    Send bug-reports and/or questions to:
    {gianpiero.cabodi,stefano.quer}@polito.it.
    ]

******************************************************************************/

#include "CUDD/dddmpInt.h"

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DDDMP_DEBUG_CNF 0

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int DddmpWriteNodeIndexCnfWithTerminalCheck(DdNode *f, int *cnfIds, int id);
static int DddmpClearVisitedCnfRecur(DdNode *f);
static void DddmpClearVisitedCnf(DdNode *f);
static int NumberNodeRecurCnf(DdNode *f, int *cnfIds, int id);
static void DddmpDdNodesCheckIncomingAndScanPath(DdNode *f, int pathLengthCurrent, int edgeInTh, int pathLengthTh);
static int DddmpDdNodesNumberEdgesRecur(DdNode *f, int *cnfIds, int id);
static int DddmpDdNodesResetCountRecur(DdNode *f);
static int DddmpDdNodesCountEdgesRecur(DdNode *f);
static void RemoveFromUniqueRecurCnf(DdManager *ddMgr, DdNode *f);
static void RestoreInUniqueRecurCnf(DdManager *ddMgr, DdNode *f);
static int DddmpPrintBddAndNextRecur(DdManager *ddMgr, DdNode *f);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Removes nodes from unique table and numbers them]

  Description [Node numbering is required to convert pointers to integers.
    Since nodes are removed from unique table, no new nodes should 
    be generated before re-inserting nodes in the unique table
    (DddmpUnnumberDdNodesCnf()).
    ]

  SideEffects [Nodes are temporarily removed from unique table]

  SeeAlso     [RemoveFromUniqueRecurCnf(), NumberNodeRecurCnf(), 
    DddmpUnnumberDdNodesCnf()]

******************************************************************************/

int
DddmpNumberDdNodesCnf (
  DdManager *ddMgr  /*  IN: DD Manager */,
  DdNode **f        /*  IN: array of BDDs */,
  int rootN         /*  IN: number of BDD roots in the array of BDDs */,
  int *cnfIds       /* OUT: CNF identifiers for variables */,
  int id            /* OUT: number of Temporary Variables Introduced */
  )
{
  int i;

  for (i=0; i<rootN; i++) {
    RemoveFromUniqueRecurCnf (ddMgr, f[i]);
  }

  for (i=0; i<rootN; i++) {
    id = NumberNodeRecurCnf (f[i], cnfIds, id);
  }

  return (id);
}

/**Function********************************************************************

  Synopsis    [Removes nodes from unique table and numbers each node according
    to the number of its incoming BDD edges.
    ]

  Description [Removes nodes from unique table and numbers each node according
    to the number of its incoming BDD edges.
    ]

  SideEffects [Nodes are temporarily removed from unique table]

  SeeAlso     [RemoveFromUniqueRecurCnf()]

******************************************************************************/

int
DddmpDdNodesCountEdgesAndNumber (
  DdManager *ddMgr    /*  IN: DD Manager */,
  DdNode **f          /*  IN: Array of BDDs */,
  int rootN           /*  IN: Number of BDD roots in the array of BDDs */,
  int edgeInTh        /*  IN: Max # In-Edges, after a Insert Cut Point */,
  int pathLengthTh    /*  IN: Max Path Length (after, Insert a Cut Point) */,
  int *cnfIds         /* OUT: CNF identifiers for variables */,
  int id              /* OUT: Number of Temporary Variables Introduced */
  )
{
  int retValue, i;

  /*-------------------------- Remove From Unique ---------------------------*/

  for (i=0; i<rootN; i++) {
    RemoveFromUniqueRecurCnf (ddMgr, f[i]);
  }

  /*-------------------- Reset Counter and Reset Visited --------------------*/

  for (i=0; i<rootN; i++) {
    retValue = DddmpDdNodesResetCountRecur (f[i]);
  }

  /*  Here we must have:
   *    cnfIndex = 0
   *    visitedFlag = 0
   *  FOR ALL nodes
   */

#if DDDMP_DEBUG_CNF
  fprintf (stdout, "###---> BDDs After Count Reset:\n");
  DddmpPrintBddAndNext (ddMgr, f, rootN);
#endif

  /*----------------------- Count Incoming Edges ----------------------------*/

  for (i=0; i<rootN; i++) {
    retValue = DddmpDdNodesCountEdgesRecur (f[i]);
  }

  /*  Here we must have:
   *    cnfIndex = incoming edge count
   *    visitedFlag = 0 (AGAIN ... remains untouched)
   *  FOR ALL nodes
   */

#if DDDMP_DEBUG_CNF
  fprintf (stdout, "###---> BDDs After Count Recur:\n");
  DddmpPrintBddAndNext (ddMgr, f, rootN);
#endif

  /*------------------------- Count Path Length ----------------------------*/
     
  for (i=0; i<rootN; i++) {
    DddmpDdNodesCheckIncomingAndScanPath (f[i], 0, edgeInTh,
      pathLengthTh);
  }

  /* Here we must have:
   *   cnfIndex = 1 if we want to insert there a cut point
   *              0 if we do NOT want to insert there a cut point
   *    visitedFlag = 1
   *  FOR ALL nodes
   */

#if DDDMP_DEBUG_CNF
  fprintf (stdout, "###---> BDDs After Check Incoming And Scan Path:\n");
  DddmpPrintBddAndNext (ddMgr, f, rootN);
#endif

  /*-------------------- Number Nodes and Set Visited -----------------------*/

  for (i=0; i<rootN; i++) {
    id = DddmpDdNodesNumberEdgesRecur (f[i], cnfIds, id);
  }

  /*  Here we must have:
   *    cnfIndex = CNF auxiliary variable enumeration
   *    visitedFlag = 0
   *  FOR ALL nodes
   */

#if DDDMP_DEBUG_CNF
  fprintf (stdout, "###---> BDDs After Count Edges Recur:\n");
  DddmpPrintBddAndNext (ddMgr, f, rootN);
#endif

  return (id);
}

/**Function********************************************************************

  Synopsis     [Restores nodes in unique table, loosing numbering]

  Description  [Node indexes are no more needed. Nodes are re-linked in the
    unique table.
    ]

  SideEffects  [None]

  SeeAlso      [DddmpNumberDdNode()]

******************************************************************************/

void
DddmpUnnumberDdNodesCnf(
  DdManager *ddMgr   /* IN: DD Manager */,
  DdNode **f         /* IN: array of BDDs */,
  int rootN           /* IN: number of BDD roots in the array of BDDs */
  )
{
  int i;

  for (i=0; i<rootN; i++) {
    RestoreInUniqueRecurCnf (ddMgr, f[i]);
  }

  return;
}

/**Function********************************************************************

  Synopsis     [Prints debug information]

  Description  [Prints debug information for an array of BDDs on the screen]

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

int
DddmpPrintBddAndNext (
  DdManager *ddMgr   /* IN: DD Manager */,
  DdNode **f         /* IN: Array of BDDs to be displayed */,
  int rootN          /* IN: Number of BDD roots in the array of BDDs */
  )
{
  int i;

  for (i=0; i<rootN; i++) {
    fprintf (stdout, "---> Bdd %d:\n", i);
    fflush (stdout);
    DddmpPrintBddAndNextRecur (ddMgr, f[i]);
  }

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Write index to node]

  Description  [The index of the node is written in the "next" field of
    a DdNode struct. LSB is not used (set to 0). It is used as 
    "visited" flag in DD traversals.
    ]

  SideEffects  [None]

  SeeAlso      [DddmpReadNodeIndexCnf(), DddmpSetVisitedCnf (),
    DddmpVisitedCnf ()
    ]

******************************************************************************/

int
DddmpWriteNodeIndexCnf (
  DdNode *f   /* IN: BDD node */,
  int id      /* IN: index to be written */
  )
{
  if (!Cudd_IsConstant (f)) {
    f->next = (struct DdNode *)((ptruint)((id)<<1));
  }

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Returns true if node is visited]

  Description  [Returns true if node is visited]

  SideEffects  [None]

  SeeAlso      [DddmpSetVisitedCnf (), DddmpClearVisitedCnf ()]

******************************************************************************/

int
DddmpVisitedCnf (
  DdNode *f      /* IN: BDD node to be tested */
  )
{
  f = Cudd_Regular(f);

  return ((int)((ptruint)(f->next)) & (01));
}

/**Function********************************************************************

  Synopsis     [Marks a node as visited]
 
  Description  [Marks a node as visited]

  SideEffects  [None]

  SeeAlso      [DddmpVisitedCnf (), DddmpClearVisitedCnf ()]

******************************************************************************/

void
DddmpSetVisitedCnf (
  DdNode *f     /* IN: BDD node to be marked (as visited) */
  )
{
  f = Cudd_Regular(f);

  f->next = (DdNode *)(ptruint)((int)((ptruint)(f->next))|01);

  return;
}

/**Function********************************************************************

  Synopsis     [Reads the index of a node]

  Description  [Reads the index of a node. LSB is skipped (used as visited
    flag).
    ]

  SideEffects  [None]

  SeeAlso      [DddmpWriteNodeIndexCnf(), DddmpSetVisitedCnf (),
    DddmpVisitedCnf ()]

******************************************************************************/

int
DddmpReadNodeIndexCnf (
  DdNode *f     /* IN: BDD node */
  )
{
  if (!Cudd_IsConstant (f)) {
    return ((int)(((ptruint)(f->next))>>1));
  } else {
    return (1);
  }
}

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Write index to node]

  Description  [The index of the node is written in the "next" field of
    a DdNode struct. LSB is not used (set to 0). It is used as 
    "visited" flag in DD traversals. The index corresponds to 
    the BDD node variable if both the node's children are a 
    constant node, otherwise a new CNF variable is used. 
    ]

  SideEffects  [None]

  SeeAlso      [DddmpReadNodeIndexCnf(), DddmpSetVisitedCnf (),
    DddmpVisitedCnf ()]

*****************************************************************************/

static int
DddmpWriteNodeIndexCnfWithTerminalCheck (
  DdNode *f    /* IN: BDD node */,
  int *cnfIds  /* IN: possible source for the index to be written */,
  int id       /* IN: possible source for the index to be written */
  )
{
  if (!Cudd_IsConstant (f)) {
    if (Cudd_IsConstant (cuddT (f)) && Cudd_IsConstant (cuddE (f))) {
      /* If Variable SET ID as Variable ID */
      f->next = (struct DdNode *)((ptruint)((cnfIds[f->index])<<1));
    } else {
      f->next = (struct DdNode *)((ptruint)((id)<<1));
      id++;
    }
  }

  return(id);
}

/**Function********************************************************************

  Synopsis     [Mark ALL nodes as not visited]

  Description  [Mark ALL nodes as not visited (it recurs on the node children)]

  SideEffects  [None]

  SeeAlso      [DddmpVisitedCnf (), DddmpSetVisitedCnf ()]

******************************************************************************/

static int
DddmpClearVisitedCnfRecur (
  DdNode *f     /* IN: root of the BDD to be marked */
  )
{
  int retValue;

  f = Cudd_Regular(f);

  if (cuddIsConstant (f)) {
    return (DDDMP_SUCCESS);
  }

  if (!DddmpVisitedCnf (f)) {
    return (DDDMP_SUCCESS);
  }

  retValue = DddmpClearVisitedCnfRecur (cuddT (f));
  retValue = DddmpClearVisitedCnfRecur (cuddE (f));

  DddmpClearVisitedCnf (f);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Marks a node as not visited]

  Description  [Marks a node as not visited]

  SideEffects  [None]

  SeeAlso      [DddmpVisitedCnf (), DddmpSetVisitedCnf ()]

******************************************************************************/

static void
DddmpClearVisitedCnf (
  DdNode *f     /* IN: BDD node to be marked (as not visited) */
  )
{
  f = Cudd_Regular (f);

  f->next = (DdNode *)(ptruint)((int)((ptruint)(f->next)) & (~01));

  return;
}

/**Function********************************************************************

  Synopsis     [Number nodes recursively in post-order]

  Description  [Number nodes recursively in post-order.
    The "visited" flag is used with inverse polarity, because all nodes
    were set "visited" when removing them from unique. 
    ]

  SideEffects  ["visited" flags are reset.]

  SeeAlso      []

******************************************************************************/

static int
NumberNodeRecurCnf(
  DdNode *f        /*  IN: root of the BDD to be numbered */,
  int *cnfIds      /*  IN: possible source for numbering */,
  int id        /* IN/OUT: possible source for numbering */
  )
{
  f = Cudd_Regular(f);

  if (!DddmpVisitedCnf (f)) {
    return (id);
  }

  if (!cuddIsConstant (f)) {
    id = NumberNodeRecurCnf (cuddT (f), cnfIds, id);
    id = NumberNodeRecurCnf (cuddE (f), cnfIds, id);
  }

  id = DddmpWriteNodeIndexCnfWithTerminalCheck (f, cnfIds, id);
  DddmpClearVisitedCnf (f);

  return (id);
}

/**Function********************************************************************

  Synopsis     [Number nodes recursively in post-order]

  Description  [Number nodes recursively in post-order.
    The "visited" flag is used with the right polarity.
    The node is assigned to a new CNF variable only if it is a "shared"
    node (i.e. the number of its incoming edges is greater than 1). 
    ]

  SideEffects  ["visited" flags are set.]

  SeeAlso      []

******************************************************************************/

static void
DddmpDdNodesCheckIncomingAndScanPath (
  DdNode *f             /* IN: BDD node to be numbered */,
  int pathLengthCurrent /* IN: Current Path Length */,
  int edgeInTh          /* IN: Max # In-Edges, after a Insert Cut Point */,
  int pathLengthTh      /* IN: Max Path Length (after, Insert a Cut Point) */
  )
{
  int retValue;

  f = Cudd_Regular(f);

  if (DddmpVisitedCnf (f)) {
    return;
  }

  if (cuddIsConstant (f)) {
    return;
  }

  pathLengthCurrent++;
  retValue = DddmpReadNodeIndexCnf (f);

  if ( ((edgeInTh >= 0) && (retValue > edgeInTh)) ||
       ((pathLengthTh >= 0) && (pathLengthCurrent > pathLengthTh))
     ) {
    DddmpWriteNodeIndexCnf (f, 1);
    pathLengthCurrent = 0;
  } else {
    DddmpWriteNodeIndexCnf (f, 0);
  }

  DddmpDdNodesCheckIncomingAndScanPath (cuddT (f), pathLengthCurrent,
    edgeInTh, pathLengthTh);
  DddmpDdNodesCheckIncomingAndScanPath (cuddE (f), pathLengthCurrent,
    edgeInTh, pathLengthTh);

  DddmpSetVisitedCnf (f);

  return;
}

/**Function********************************************************************

  Synopsis     [Number nodes recursively in post-order]

  Description  [Number nodes recursively in post-order.
    The "visited" flag is used with the inverse polarity.
    Numbering follows the subsequent strategy:
    * if the index = 0 it remains so
    * if the index >= 1 it gets enumerated.
      This implies that the node is assigned to a new CNF variable only if
      it is not a terminal node otherwise it is assigned the index of
      the BDD variable.
    ]

  SideEffects  ["visited" flags are reset.]

  SeeAlso      []

******************************************************************************/

static int
DddmpDdNodesNumberEdgesRecur (
  DdNode *f      /*  IN: BDD node to be numbered */,
  int *cnfIds    /*  IN: possible source for numbering */,
  int id      /* IN/OUT: possible source for numbering */
  )
{
  int retValue;

  f = Cudd_Regular(f);

  if (!DddmpVisitedCnf (f)) {
    return (id);
  }

  if (cuddIsConstant (f)) {
    return (id);
  }

  id = DddmpDdNodesNumberEdgesRecur (cuddT (f), cnfIds, id);
  id = DddmpDdNodesNumberEdgesRecur (cuddE (f), cnfIds, id);

  retValue = DddmpReadNodeIndexCnf (f);
  if (retValue >= 1) {
    id = DddmpWriteNodeIndexCnfWithTerminalCheck (f, cnfIds, id);
  } else {
    DddmpWriteNodeIndexCnf (f, 0);
  }

  DddmpClearVisitedCnf (f);

  return (id);
}

/**Function********************************************************************

  Synopsis     [Resets counter and visited flag for ALL nodes of a BDD]

  Description  [Resets counter and visited flag for ALL nodes of a BDD (it 
    recurs on the node children). The index field of the node is 
    used as counter.
    ]

  SideEffects  []

  SeeAlso      []

******************************************************************************/

static int
DddmpDdNodesResetCountRecur (
  DdNode *f  /*  IN: root of the BDD whose counters are reset */
  )
{
  int retValue;

  f = Cudd_Regular (f);

  if (!DddmpVisitedCnf (f)) {
    return (DDDMP_SUCCESS);
  }

  if (!cuddIsConstant (f)) {
    retValue = DddmpDdNodesResetCountRecur (cuddT (f));
    retValue = DddmpDdNodesResetCountRecur (cuddE (f));
  }

  DddmpWriteNodeIndexCnf (f, 0);
  DddmpClearVisitedCnf (f);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Counts the number of incoming edges for each node of a BDD]

  Description  [Counts (recursively) the number of incoming edges for each 
    node of a BDD. This number is stored in the index field.
    ]

  SideEffects  ["visited" flags remain untouched.]

  SeeAlso      []

******************************************************************************/

static int
DddmpDdNodesCountEdgesRecur (
  DdNode *f    /* IN: root of the BDD */
  )
{
  int indexValue, retValue;

  f = Cudd_Regular (f);

  if (cuddIsConstant (f)) {
    return (DDDMP_SUCCESS);
  }

  if (Cudd_IsConstant (cuddT (f)) && Cudd_IsConstant (cuddE (f))) {
    return (DDDMP_SUCCESS);
  }

  indexValue = DddmpReadNodeIndexCnf (f);

  /* IF (first time) THEN recur */
  if (indexValue == 0) {
    retValue = DddmpDdNodesCountEdgesRecur (cuddT (f));
    retValue = DddmpDdNodesCountEdgesRecur (cuddE (f));
  }

  /* Increment Incoming-Edge Count Flag */
  indexValue++;
  DddmpWriteNodeIndexCnf (f, indexValue);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis    [Removes a node from unique table]

  Description [Removes a node from the unique table by locating the proper
    subtable and unlinking the node from it. It recurs on  on the 
    children of the node. Constants remain untouched.
    ]

  SideEffects [Nodes are left with the "visited" flag true.]

  SeeAlso     [RestoreInUniqueRecurCnf()]

******************************************************************************/

static void
RemoveFromUniqueRecurCnf (
  DdManager *ddMgr  /*  IN: DD Manager */,
  DdNode *f         /*  IN: root of the BDD to be extracted */
  )
{
  DdNode *node, *last, *next;
  DdNode *sentinel = &(ddMgr->sentinel);
  DdNodePtr *nodelist;
  DdSubtable *subtable;
  int pos, level;

  f = Cudd_Regular (f);

  if (DddmpVisitedCnf (f)) {
    return;
  }

  if (!cuddIsConstant (f)) {

    RemoveFromUniqueRecurCnf (ddMgr, cuddT (f));
    RemoveFromUniqueRecurCnf (ddMgr, cuddE (f));

    level = ddMgr->perm[f->index];
    subtable = &(ddMgr->subtables[level]);

    nodelist = subtable->nodelist;

    pos = ddHash (cuddT (f), cuddE (f), subtable->shift);
    node = nodelist[pos];
    last = NULL;
    while (node != sentinel) {
      next = node->next;
      if (node == f) {
        if (last != NULL)  
  	  last->next = next;
        else 
          nodelist[pos] = next;
        break;
      } else {
        last = node;
        node = next;
      }
    }

    f->next = NULL;

  }

  DddmpSetVisitedCnf (f);

  return;
}

/**Function********************************************************************

  Synopsis     [Restores a node in unique table]

  Description  [Restores a node in unique table (recursive)]

  SideEffects  [Nodes are not restored in the same order as before removal]

  SeeAlso      [RemoveFromUnique()]

******************************************************************************/

static void
RestoreInUniqueRecurCnf (
  DdManager *ddMgr  /*  IN: DD Manager */,
  DdNode *f         /*  IN: root of the BDD to be restored */
  )
{
  DdNodePtr *nodelist;
  DdNode *T, *E, *looking;
  DdNodePtr *previousP;
  DdSubtable *subtable;
  int pos, level;
#ifdef DDDMP_DEBUG
  DdNode *node;
  DdNode *sentinel = &(ddMgr->sentinel);
#endif

  f = Cudd_Regular(f);

  if (!Cudd_IsComplement (f->next)) {
    return;
  }

  if (cuddIsConstant (f)) {
    /* StQ 11.02.2004:
       Bug fixed --> restore NULL within the next field */
    /*DddmpClearVisitedCnf (f);*/
    f->next = NULL;

    return;
  }

  RestoreInUniqueRecurCnf (ddMgr, cuddT (f));
  RestoreInUniqueRecurCnf (ddMgr, cuddE (f));

  level = ddMgr->perm[f->index];
  subtable = &(ddMgr->subtables[level]);

  nodelist = subtable->nodelist;

  pos = ddHash (cuddT (f), cuddE (f), subtable->shift);

#ifdef DDDMP_DEBUG
  /* verify uniqueness to avoid duplicate nodes in unique table */
  for (node=nodelist[pos]; node != sentinel; node=node->next)
    assert(node!=f);
#endif

  T = cuddT (f);
  E = cuddE (f);
  previousP = &(nodelist[pos]);
  looking = *previousP;

  while (T < cuddT (looking)) {
    previousP = &(looking->next);
    looking = *previousP;
  }

  while (T == cuddT (looking) && E < cuddE (looking)) {
    previousP = &(looking->next);
    looking = *previousP;
  }
  f->next = *previousP;
  *previousP = f;

  return;
}

/**Function********************************************************************

  Synopsis     [Prints debug info]

  Description  [Prints debug info for a BDD on the screen. It recurs on 
    node's children.
    ]

  SideEffects  []

  SeeAlso      []

******************************************************************************/

static int
DddmpPrintBddAndNextRecur (
  DdManager *ddMgr   /* IN: DD Manager */,
  DdNode *f          /* IN: root of the BDD to be displayed */
  )
{
  int retValue;
  DdNode *fPtr, *tPtr, *ePtr;
    
  fPtr = Cudd_Regular (f);
  
  if (Cudd_IsComplement (f)) {
    fprintf (stdout, "sign=- ptr=%ld ", ((long int) fPtr));
  } else {
    fprintf (stdout, "sign=+ ptr=%ld ", ((long int) fPtr));
  }
 
  if (cuddIsConstant (fPtr)) {
    fprintf (stdout, "one\n");
    fflush (stdout);
    return (DDDMP_SUCCESS);
  }

  fprintf (stdout,  
    "thenPtr=%ld elsePtr=%ld BddId=%d CnfId=%d Visited=%d\n",
    ((long int) cuddT (fPtr)), ((long int) cuddE (fPtr)),
    fPtr->index, DddmpReadNodeIndexCnf (fPtr),
    DddmpVisitedCnf (fPtr));
  
  tPtr  = cuddT (fPtr);
  ePtr = cuddE (fPtr);
  if (Cudd_IsComplement (f)) {
    tPtr  = Cudd_Not (tPtr);
    ePtr = Cudd_Not (ePtr);
  }

  retValue = DddmpPrintBddAndNextRecur (ddMgr, tPtr);
  retValue = DddmpPrintBddAndNextRecur (ddMgr, ePtr);

  return (DDDMP_SUCCESS);
}


