/**CFile**********************************************************************
  FileName     [dddmpStoreAdd.c]

  PackageName  [dddmp]

  Synopsis     [Functions to write ADDs to file.]

  Description  [Functions to write ADDs to file.
    ADDs are represended on file either in text or binary format under the
    following rules.  A file contains a forest of ADDs (a vector of
    Boolean functions).  ADD nodes are numbered with contiguous numbers,
    from 1 to NNodes (total number of nodes on a file). 0 is not used to
    allow negative node indexes for complemented edges.  A file contains
    a header, including information about variables and roots to ADD
    functions, followed by the list of nodes.
    ADD nodes are listed according to their numbering, and in the present
    implementation numbering follows a post-order strategy, in such a way
    that a node is never listed before its Then/Else children.
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

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int NodeStoreRecurAdd(DdManager *ddMgr, DdNode *f, int mode, int *supportids, char **varnames, int *outids, FILE *fp);
static int NodeTextStoreAdd(DdManager *ddMgr, DdNode *f, int mode, int *supportids, char **varnames, int *outids, FILE *fp, int idf, int vf, int idT, int idE);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument ADD.]

  Description  [Dumps the argument ADD to file. Dumping is done through
    Dddmp_cuddAddArrayStore, And a dummy array of 1 ADD root is
    used for this purpose.
    ]

  SideEffects  [Nodes are temporarily removed from unique hash. They are 
    re-linked after the store operation in a modified order.]

  SeeAlso      [Dddmp_cuddAddLoad Dddmp_cuddAddArrayLoad]

******************************************************************************/

int
Dddmp_cuddAddStore (
  DdManager *ddMgr          /* IN: DD Manager */,
  char *ddname              /* IN: DD name (or NULL) */,
  DdNode *f                 /* IN: ADD root to be stored */,
  char **varnames           /* IN: array of variable names (or NULL) */,
  int *auxids               /* IN: array of converted var ids */,
  int mode                  /* IN: storing mode selector */,
  Dddmp_VarInfoType varinfo /* IN: extra info for variables in text mode */,
  char *fname               /* IN: File name */,
  FILE *fp                  /* IN: File pointer to the store file */ 
  )
{
  int retValue;
  DdNode *tmpArray[1];

  tmpArray[0] = f;
  retValue = Dddmp_cuddAddArrayStore (ddMgr, ddname, 1, tmpArray, NULL,
    varnames, auxids, mode, varinfo, fname, fp);

  return (retValue);
}

/**Function********************************************************************

  Synopsis    [Writes a dump file representing the argument Array of ADDs.]

  Description [Dumps the argument array of ADDs to file. Dumping is
    either in text or binary form. see the corresponding BDD dump 
    function for further details.
    ]

  SideEffects [Nodes are temporarily removed from the unique hash
    table. They are re-linked after the store operation in a 
    modified order.
    ]

  SeeAlso     [Dddmp_cuddAddStore, Dddmp_cuddAddLoad,
    Dddmp_cuddAddArrayLoad]

******************************************************************************/

int
Dddmp_cuddAddArrayStore (
  DdManager *ddMgr          /* IN: DD Manager */,
  char *ddname              /* IN: DD name (or NULL) */,
  int nRoots                /* IN: number of output BDD roots to be stored */,
  DdNode **f                /* IN: array of ADD roots to be stored */,
  char **rootnames          /* IN: array of root names (or NULL) */,
  char **varnames           /* IN: array of variable names (or NULL) */,
  int *auxids               /* IN: array of converted var IDs */,
  int mode                  /* IN: storing mode selector */,
  Dddmp_VarInfoType varinfo /* IN: extra info for variables in text mode */,
  char *fname               /* IN: File name */,
  FILE *fp                  /* IN: File pointer to the store file */ 
  )
{
  int retValue;

#if 0
#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During ADD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During ADD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif
#endif

  retValue = DddmpCuddDdArrayStoreBdd (DDDMP_ADD, ddMgr, ddname, nRoots, f,
    rootnames, varnames, auxids, mode, varinfo, fname, fp);

#if 0
#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During ADD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During ADD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif
#endif

  return (retValue);
}

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument Array of
    BDDs/ADDs.
    ]

  Description  [Dumps the argument array of BDDs/ADDs to file. Internal 
    function doing inner steps of store for BDDs and ADDs.
    ADD store is presently supported only with the text format.
    ]

  SideEffects  [Nodes are temporarily removed from the unique hash
    table. They are re-linked after the store operation in a 
    modified order.
    ]	
	
  SeeAlso      [Dddmp_cuddBddStore, Dddmp_cuddBddLoad,
    Dddmp_cuddBddArrayLoad
    ]

******************************************************************************/

int
DddmpCuddDdArrayStoreBdd (
  Dddmp_DecompType ddType   /* IN: Selects the decomp type: BDD or ADD */,
  DdManager *ddMgr          /* IN: DD Manager */,
  char *ddname              /* IN: DD name (or NULL) */,
  int nRoots                /* IN: number of output BDD roots to be stored */,
  DdNode **f                /* IN: array of DD roots to be stored */,
  char **rootnames          /* IN: array of root names (or NULL) */,
  char **varnames           /* IN: array of variable names (or NULL) */,
  int *auxids               /* IN: array of converted var IDs */,
  int mode                  /* IN: storing mode selector */,
  Dddmp_VarInfoType varinfo /* IN: extra info for variables in text mode */,
  char *fname               /* IN: File name */,
  FILE *fp                  /* IN: File pointer to the store file */ 
  )
{
  DdNode *support = NULL;
  DdNode *scan;
  int *ids = NULL;
  int *permids = NULL;
  int *invpermids = NULL;
  int *supportids = NULL;
  int *outids = NULL;
  char **outvarnames = NULL;
  int nVars = ddMgr->size;
  int nnodes;
  int retValue;
  int i, var;
  int fileToClose = 0;

  /* 
   *  Check DD Type and Mode
   */

  Dddmp_CheckAndGotoLabel (ddType==DDDMP_BDD,
    "Error writing to file: BDD Type.", failure);
  Dddmp_CheckAndGotoLabel (mode==DDDMP_MODE_BINARY,
    "Error writing to file: ADD Type with Binary Mode.", failure);

  /* 
   *  Check if File needs to be opened in the proper mode.
   */

  if (fp == NULL) {
    fp = fopen (fname, "w");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  /* 
   *  Force binary mode if automatic.
   */

  switch (mode) {
    case DDDMP_MODE_TEXT:
    case DDDMP_MODE_BINARY:
      break;
    case DDDMP_MODE_DEFAULT:
      mode = DDDMP_MODE_BINARY;
      break;
    default:
      mode = DDDMP_MODE_BINARY;
      break;
  }

  /* 
   * Alloc vectors for variable IDs, perm IDs and support IDs.
   *  +1 to include a slot for terminals.
   */

  ids = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndGotoLabel (ids==NULL, "Error allocating memory.", failure);
  permids = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndGotoLabel (permids==NULL, "Error allocating memory.", failure);
  invpermids = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndGotoLabel (invpermids==NULL, "Error allocating memory.",
    failure);
  supportids = DDDMP_ALLOC (int, nVars+1);
  Dddmp_CheckAndGotoLabel (supportids==NULL, "Error allocating memory.",
    failure);
    
  for (i=0; i<nVars; i++) {
    ids[i] = permids[i] = invpermids[i] = supportids[i] = (-1);
  }
  /* StQ */
  supportids[nVars] = -1;
  
  /* 
   *  Take the union of the supports of each output function.
   *  skip NULL functions.
   *  Set permids and invpermids of support variables to the proper values.
   */

  for (i=0; i<nRoots; i++) {
    if (f[i] == NULL) {
      continue;
    }
    support = Cudd_Support (ddMgr, f[i]);
    Dddmp_CheckAndGotoLabel (support==NULL, "NULL support returned.",
      failure);
    cuddRef (support);
    scan = support;
    while (!cuddIsConstant(scan)) {
      ids[scan->index] = scan->index;
      permids[scan->index] = ddMgr->perm[scan->index];
      invpermids[ddMgr->perm[scan->index]] = scan->index;
      scan = cuddT (scan);
    }
    Cudd_RecursiveDeref (ddMgr, support);
  }
  /* so that we do not try to free it in case of failure */
  support = NULL;

  /*
   *  Set supportids to incremental (shrinked) values following the ordering.
   */

  for (i=0, var=0; i<nVars; i++) {
    if (invpermids[i] >= 0) {
      supportids[invpermids[i]] = var++;
    }
  }
  /* set a dummy id for terminal nodes */
  supportids[nVars] = var;

  /*
   *  Select conversion array for extra var info
   */

  switch (mode) {
    case DDDMP_MODE_TEXT:
      switch (varinfo) {
        case DDDMP_VARIDS:
          outids = ids;
          break;
        case DDDMP_VARPERMIDS:
          outids = permids;
          break;
        case DDDMP_VARAUXIDS:
          outids = auxids;
          break;
        case DDDMP_VARNAMES:
          outvarnames = varnames;
          break;
        case DDDMP_VARDEFAULT:
          break;
      }
      break;
    case DDDMP_MODE_BINARY:
      outids = NULL;
      break;
  }

  /* 
   *  Number dd nodes and count them (numbering is from 1 to nnodes)
   */

  nnodes = DddmpNumberAddNodes (ddMgr, f, nRoots);

  /* 
   * Start Header
   */

#ifdef DDDMP_VERSION
  retValue = fprintf (fp, ".ver %s\n", DDDMP_VERSION);
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);
#endif

  retValue = fprintf (fp, ".add\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  retValue = fprintf (fp, ".mode %c\n", mode);
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  if (mode == DDDMP_MODE_TEXT) {
    retValue = fprintf (fp, ".varinfo %d\n", varinfo);
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  if (ddname != NULL) {
    retValue = fprintf (fp, ".dd %s\n",ddname);
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  retValue = fprintf (fp, ".nnodes %d\n", nnodes);
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  retValue = fprintf (fp, ".nvars %d\n", nVars);
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  retValue = fprintf (fp, ".nsuppvars %d\n", var);
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  /*------------  Write the Var Names by scanning the ids array -------------*/

  if (varnames != NULL) {

    retValue = fprintf (fp, ".suppvarnames");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);

    for (i=0; i<nVars; i++) {
      if (ids[i] >= 0) {
        if (varnames[ids[i]] == NULL) {
          (void) fprintf (stderr,
             "DdStore Warning: null variable name. DUMMY%d generated\n", i);
          fflush (stderr);
          varnames[ids[i]] = DDDMP_ALLOC (char, 10);
          Dddmp_CheckAndGotoLabel (varnames[ids[i]] == NULL,
            "Error allocating memory.", failure);
          sprintf (varnames[ids[i]], "DUMMY%d", i);
        }
        retValue = fprintf (fp, " %s", varnames[ids[i]]);
        Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
          failure);
      }
    }

    retValue = fprintf (fp, "\n");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  /*--------- Write the Var SUPPORT Names by scanning the ids array ---------*/

  if (varnames != NULL) {
    retValue = fprintf (fp, ".orderedvarnames");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);

    for (i=0; i<nVars; i++) {
      if (varnames[ddMgr->invperm[i]] == NULL) {
          (void) fprintf (stderr,
           "DdStore Warning: null variable name. DUMMY%d generated\n", i);
        fflush (stderr);
        varnames[ddMgr->invperm[i]] = DDDMP_ALLOC (char, 10);
        Dddmp_CheckAndGotoLabel (varnames[ddMgr->invperm[i]] == NULL,
          "Error allocating memory.", failure);
        sprintf (varnames[ddMgr->invperm[i]], "DUMMY%d", i);
      }

      retValue = fprintf (fp, " %s", varnames[ddMgr->invperm[i]]);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
        failure);
    }

    retValue = fprintf (fp, "\n");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  /*------------ Write the var ids by scanning the ids array ---------------*/

  retValue = fprintf (fp, ".ids");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  for (i=0; i<nVars; i++) {
    if (ids[i] >= 0) {
      retValue = fprintf (fp, " %d", i);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
        failure);
    }
  }
  retValue = fprintf (fp, "\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  /*
   *  Write the var permids by scanning the permids array. 
   */

  retValue = fprintf (fp, ".permids");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);
  for (i = 0; i < nVars; i++) {
    if (permids[i] >= 0) {
      retValue = fprintf (fp, " %d", permids[i]);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
        failure);
    }
  }

  retValue = fprintf (fp, "\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  if (auxids != NULL) {
  
    /*
     * Write the var auxids by scanning the ids array. 
     */

    retValue = fprintf (fp, ".auxids");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
    for (i = 0; i < nVars; i++) {
      if (ids[i] >= 0) {
        retValue = fprintf (fp, " %d", auxids[i]);
        Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
          failure);
      }
    }
    retValue = fprintf (fp, "\n");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  /* 
   * Write the roots info. 
   */

  retValue = fprintf (fp, ".nroots %d\n", nRoots);
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  if (rootnames != NULL) {

    /* 
     * Write the root names. 
     */

    retValue = fprintf (fp, ".rootnames");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);

    for (i = 0; i < nRoots; i++) {
      if (rootnames[i] == NULL) {
        (void) fprintf (stderr,
          "DdStore Warning: null variable name. ROOT%d generated\n",i);
        fflush (stderr);
        rootnames[i] = DDDMP_ALLOC(char,10);
        Dddmp_CheckAndGotoLabel (rootnames[i]==NULL,
          "Error writing to file.", failure);
        sprintf(rootnames[ids[i]], "ROOT%d",i);
      }
      retValue = fprintf (fp, " %s", rootnames[i]);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
        failure);
    }

    retValue = fprintf (fp, "\n");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  retValue = fprintf (fp, ".rootids");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  /* 
   * Write BDD indexes of function roots.
   * Use negative integers for complemented edges. 
   */

  for (i = 0; i < nRoots; i++) {
    if (f[i] == NULL) {
      (void) fprintf (stderr, "DdStore Warning: %d-th root is NULL\n",i);
      fflush (stderr);
      retValue = fprintf (fp, " 0");
    }
    if (Cudd_IsComplement(f[i])) {
      retValue = fprintf (fp, " -%d",
        DddmpReadNodeIndexAdd (Cudd_Regular (f[i])));
    } else {
      retValue = fprintf (fp, " %d",
        DddmpReadNodeIndexAdd (Cudd_Regular (f[i])));
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  retValue = fprintf (fp, "\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  retValue = fprintf (fp, ".nodes\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  /* 
   *  END HEADER
   */

  /* 
   *  Call the function that really gets the job done.
   */

  for (i = 0; i < nRoots; i++) {
    if (f[i] != NULL) {
      retValue = NodeStoreRecurAdd (ddMgr, Cudd_Regular(f[i]),
        mode, supportids, outvarnames, outids, fp);
      Dddmp_CheckAndGotoLabel (retValue==DDDMP_FAILURE,
        "Error writing to file.", failure);
    }
  }

  /* 
   *  Write trailer and return.
   */

  retValue = fprintf (fp, ".end\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
    failure);

  if (fileToClose) {
    fclose (fp);
  }

  DddmpUnnumberAddNodes (ddMgr, f, nRoots);
  DDDMP_FREE (ids);
  DDDMP_FREE (permids);
  DDDMP_FREE (invpermids);
  DDDMP_FREE (supportids);

  return (DDDMP_SUCCESS);

  failure:

    if (ids != NULL) {
      DDDMP_FREE (ids);
    }
    if (permids != NULL) {
      DDDMP_FREE (permids);
    }
    if (invpermids != NULL) {
      DDDMP_FREE (invpermids);
    }
    if (supportids != NULL) {
      DDDMP_FREE (supportids);
    }
    if (support != NULL) {
      Cudd_RecursiveDeref (ddMgr, support);
    }
    
    return (DDDMP_FAILURE);
}

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Performs the recursive step of Dddmp_bddStore.]

  Description  [Stores a node to file in either test or binary mode.<l>
    In text mode a node is represented (on a text line basis) as
    <UL>
    <LI> node-index \[var-extrainfo\] var-index Then-index Else-index
    </UL>
    
    where all indexes are integer numbers and var-extrainfo 
    (optional redundant field) is either an integer or a string 
    (variable name). Node-index is redundant (due to the node 
    ordering) but we keep it for readability.<p>
    
    In binary mode nodes are represented as a sequence of bytes,
    representing var-index, Then-index, and Else-index in an 
    optimized way. Only the first byte (code) is mandatory. 
    Integer indexes are represented in absolute or relative mode, 
    where relative means offset wrt. a Then/Else node info. 
    Suppose Var(NodeId), Then(NodeId) and Else(NodeId) represent 
    infos about a given node.<p>
    
    The generic "NodeId" node is stored as 

    <UL>
    <LI> code-byte
    <LI> \[var-info\]
    <LI> \[Then-info\]
    <LI> \[Else-info\]
    </UL>

    where code-byte contains bit fields

    <UL>
    <LI>Unused  : 1 bit
    <LI>Variable: 2 bits, one of the following codes
    <UL>
    <LI>DDDMP_ABSOLUTE_ID   var-info = Var(NodeId) follows
    <LI>DDDMP_RELATIVE_ID   Var(NodeId) is represented in relative form as
        var-info = Min(Var(Then(NodeId)),Var(Else(NodeId))) -Var(NodeId)
    <LI>DDDMP_RELATIVE_1    No var-info follows, because
        Var(NodeId) = Min(Var(Then(NodeId)),Var(Else(NodeId)))-1
    <LI>DDDMP_TERMINAL      Node is a terminal, no var info required
    </UL>
    <LI>T       : 2 bits, with codes similar to V
    <UL>
    <LI>DDDMP_ABSOLUTE_ID   Then-info = Then(NodeId) follows
    <LI>DDDMP_RELATIVE_ID   Then(NodeId) is represented in relative form as
          Then-info = Nodeid-Then(NodeId)
    <LI>DDDMP_RELATIVE_1    No info on Then(NodeId) follows, because
          Then(NodeId) = NodeId-1
    <LI>DDDMP_TERMINAL Then Node is a terminal, no info required (for BDDs)
    </UL>
    <LI>Ecompl  : 1 bit, if 1 means complemented edge
    <LI>E       : 2 bits, with codes and meanings as for the Then edge
    </UL>
    var-info, Then-info, Else-info (if required) are represented as unsigned 
    integer values on a sufficient set of bytes (MSByte first).
    ]

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

static int
NodeStoreRecurAdd (
  DdManager *ddMgr  /* IN: DD Manager */,
  DdNode *f         /* IN: DD node to be stored */,
  int mode          /* IN: store mode */,
  int *supportids   /* IN: internal ids for variables */,
  char **varnames   /* IN: names of variables: to be stored with nodes */,
  int *outids       /* IN: output ids for variables */,
  FILE *fp          /* IN: store file */
  )
{
  DdNode *T = NULL;
  DdNode *E = NULL;
  int idf = (-1);
  int idT = (-1);
  int idE = (-1);
  int vf = (-1);
  int vT = (-1);
  int vE = (-1);
  int retValue;
  int nVars;

  nVars = ddMgr->size;
  T = E = NULL;
  idf = idT =  idE = (-1);

#ifdef DDDMP_DEBUG
  assert(!Cudd_IsComplement(f));
  assert(f!=NULL);
  assert(supportids!=NULL);
#endif

  /* If already visited, nothing to do. */
  if (DddmpVisitedAdd (f)) {
    return (DDDMP_SUCCESS);
  }

  /* Mark node as visited. */
  DddmpSetVisitedAdd (f);

  if (Cudd_IsConstant(f)) {
    /* Check for special case: don't recur */
    idf = DddmpReadNodeIndexAdd (f);
  } else {

#ifdef DDDMP_DEBUG
    /* BDDs! Only one constant supported */
    assert (!cuddIsConstant(f));
#endif

    /* 
     *  Recursive call for Then edge
     */

    T = cuddT(f);
#ifdef DDDMP_DEBUG
    /* ROBDDs! No complemented Then edge */
    assert (!Cudd_IsComplement(T)); 
#endif
    /* recur */
    retValue = NodeStoreRecurAdd (ddMgr, T, mode, supportids, varnames, outids,
      fp);
    if (retValue != DDDMP_SUCCESS) {
      return (retValue);
    }

    /* 
     *  Recursive call for Else edge
     */

    E = Cudd_Regular (cuddE (f));
    retValue = NodeStoreRecurAdd (ddMgr, E, mode, supportids, varnames, outids,
      fp);
    if (retValue != DDDMP_SUCCESS) {
      return (retValue);
    }

    /* 
     *  Obtain nodeids and variable ids of f, T, E 
     */

    idf = DddmpReadNodeIndexAdd (f);
    vf = f->index;

    idT = DddmpReadNodeIndexAdd (T);
    if (Cudd_IsConstant(T)) {
      vT = nVars;
    } else {
      vT = T->index;
    }

    idE = DddmpReadNodeIndexAdd (E);
    if (Cudd_IsConstant(E)) {
      vE = nVars;
    } else {
      vE = E->index;
    }
  }

  retValue = NodeTextStoreAdd (ddMgr, f, mode, supportids, varnames,
    outids, fp, idf, vf, idT, idE);

  return (retValue);
}

/**Function********************************************************************

  Synopsis     [Store One Single Node in Text Format.]

  Description  [Store 1 0 0 for the terminal node.
    Store id, left child pointer, right pointer for all the other nodes.
    ]

  SideEffects  [None]

  SeeAlso      [NodeBinaryStore]

******************************************************************************/

static int
NodeTextStoreAdd (
  DdManager *ddMgr  /* IN: DD Manager */,
  DdNode *f         /* IN: DD node to be stored */,
  int mode          /* IN: store mode */,
  int *supportids   /* IN: internal ids for variables */,
  char **varnames   /* IN: names of variables: to be stored with nodes */,
  int *outids       /* IN: output ids for variables */,
  FILE *fp          /* IN: Store file */,
  int idf           /* IN: index of the current node */,
  int vf            /* IN: variable of the current node */,
  int idT           /* IN: index of the Then node */,
  int idE           /* IN: index of the Else node */
  )
{
  int retValue;

  /*
   *  Check for Constant
   */

  if (Cudd_IsConstant(f)) {

    if (f == Cudd_ReadOne(ddMgr)) {
      if ((varnames != NULL) || (outids != NULL)) {
        retValue = fprintf (fp, "%d T 1 0 0\n", idf);
      } else {
        retValue = fprintf (fp, "%d 1 0 0\n", idf);
      }

      if (retValue == EOF) {
        return (DDDMP_FAILURE);
      } else {
        return (DDDMP_SUCCESS);
      }
    }

    if (f == Cudd_ReadZero(ddMgr)) {
      if ((varnames != NULL) || (outids != NULL)) {
        retValue = fprintf (fp, "%d T 0 0 0\n", idf);
      } else {
        retValue = fprintf (fp, "%d 0 0 0\n", idf);
      }

      if (retValue == EOF) {
        return (DDDMP_FAILURE);
      } else {
        return (DDDMP_SUCCESS);
      }
    }

    /*
     *  A constant node different from 1: an ADD constant
     */

    if ((varnames != NULL) || (outids != NULL)) {
      retValue = fprintf (fp, "%d T %g 0 0\n",idf,Cudd_V(f));
    } else {
      retValue = fprintf (fp, "%d %g 0 0\n",idf, Cudd_V(f));
    }

    if (retValue == EOF) {
      return (DDDMP_FAILURE);
    } else {
      return (DDDMP_SUCCESS);
    }
  }

  /*
   *  ... Not A Constant
   */

  if (Cudd_IsComplement (cuddE(f))) {
    idE = -idE;
  }

  if (varnames != NULL) {   
    retValue = fprintf (fp, "%d %s %d %d %d\n",
       idf, varnames[vf], supportids[vf], idT, idE);

    if (retValue == EOF) {
      return (DDDMP_FAILURE);
    } else {
      return (DDDMP_SUCCESS);
    }
  }

  if (outids != NULL) {   
    retValue = fprintf (fp, "%d %d %d %d %d\n",
       idf, outids[vf], supportids[vf], idT, idE);

    if (retValue == EOF) {
      return (DDDMP_FAILURE);
    } else {
      return (DDDMP_SUCCESS);
    }
    }

  retValue = fprintf (fp, "%d %d %d %d\n",
    idf, supportids[vf], idT, idE);

  if (retValue == EOF) {
    return (DDDMP_FAILURE);
  } else {
    return (DDDMP_SUCCESS);
  }
}
