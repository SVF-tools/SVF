/**CFile**********************************************************************

  FileName     [dddmpStoreCnf.c]

  PackageName  [dddmp]

  Synopsis     [Functions to write out BDDs to file in a CNF format]

  Description  [Functions to write out BDDs to file in a CNF format.]

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

#include <limits.h>
#include "CUDD/dddmpInt.h"

/*-------------------------------1--------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DDDMP_DEBUG_CNF   0

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#define GET_MAX(x,y) (x>y?x:y)

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int DddmpCuddBddArrayStoreCnf(DdManager *ddMgr, DdNode **f, int rootN, Dddmp_DecompCnfStoreType mode, int noHeader, char **varNames, int *bddIds, int *bddAuxIds, int *cnfIds, int idInitial, int edgeInTh, int pathLengthTh, char *fname, FILE *fp, int *clauseNPtr, int *varNewNPtr);
static int StoreCnfNodeByNode(DdManager *ddMgr, DdNode **f, int rootN, int *bddIds, int *cnfIds, FILE *fp, int *clauseN, int *varMax, int *rootStartLine);
static int StoreCnfNodeByNodeRecur(DdManager *ddMgr, DdNode *f, int *bddIds, int *cnfIds, FILE *fp, int *clauseN, int *varMax);
static int StoreCnfOneNode(DdNode *f, int idf, int vf, int idT, int idE, FILE *fp, int *clauseN, int *varMax);
static int StoreCnfMaxtermByMaxterm(DdManager *ddMgr, DdNode **f, int rootN, int *bddIds, int *cnfIds, int idInitial, FILE *fp, int *varMax, int *clauseN, int *rootStartLine);
static int StoreCnfBest(DdManager *ddMgr, DdNode **f, int rootN, int *bddIds, int *cnfIds, int idInitial, FILE *fp, int *varMax, int *clauseN, int *rootStartLine);
static void StoreCnfMaxtermByMaxtermRecur(DdManager *ddMgr, DdNode *node, int *bddIds, int *cnfIds, FILE *fp, int *list, int *clauseN, int *varMax);
static int StoreCnfBestNotSharedRecur(DdManager *ddMgr, DdNode *node, int idf, int *bddIds, int *cnfIds, FILE *fp, int *list, int *clauseN, int *varMax);
static int StoreCnfBestSharedRecur(DdManager *ddMgr, DdNode *node, int *bddIds, int *cnfIds, FILE *fp, int *list, int *clauseN, int *varMax);
static int printCubeCnf(DdManager *ddMgr, DdNode *node, int *cnfIds, FILE *fp, int *list, int *varMax);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a CNF format.
    ]

  Description  [Dumps the argument BDD to file.
    This task is performed by calling the function
    Dddmp_cuddBddArrayStoreCnf.
    ]

  SideEffects  [Nodes are temporarily removed from unique hash. They are
    re-linked after the store operation in a modified order.
    ]

  SeeAlso      [Dddmp_cuddBddArrayStoreCnf]

******************************************************************************/

int
Dddmp_cuddBddStoreCnf (
  DdManager *ddMgr              /* IN: DD Manager */,
  DdNode *f                     /* IN: BDD root to be stored */,
  Dddmp_DecompCnfStoreType mode /* IN: format selection */,
  int noHeader                  /* IN: do not store header iff 1 */,
  char **varNames               /* IN: array of variable names (or NULL) */,
  int *bddIds                   /* IN: array of var ids */,
  int *bddAuxIds                /* IN: array of BDD node Auxiliary Ids */,
  int *cnfIds                   /* IN: array of CNF var ids */,
  int idInitial                 /* IN: starting id for cutting variables */,
  int edgeInTh                  /* IN: Max # Incoming Edges */,
  int pathLengthTh              /* IN: Max Path Length */,
  char *fname                   /* IN: file name */,
  FILE *fp                      /* IN: pointer to the store file */,
  int *clauseNPtr               /* OUT: number of clause stored */, 
  int *varNewNPtr               /* OUT: number of new variable created */
  )
{
  int retValue;
  DdNode *tmpArray[1];

  tmpArray[0] = f;

  retValue = Dddmp_cuddBddArrayStoreCnf (ddMgr, tmpArray, 1, mode,
    noHeader, varNames, bddIds, bddAuxIds, cnfIds, idInitial, edgeInTh,
    pathLengthTh, fname, fp, clauseNPtr, varNewNPtr);

  Dddmp_CheckAndReturn (retValue==DDDMP_FAILURE, "Failure.");

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument array of BDDs
    in CNF format.
    ]

  Description  [Dumps the argument array of BDDs to file.]

  SideEffects  [Nodes are temporarily removed from the unique hash
    table. They are re-linked after the store operation in a 
    modified order.
    Three methods are allowed:
    * NodeByNode method: Insert a cut-point for each BDD node (but the
                         terminal nodes)
    * MaxtermByMaxterm method: Insert no cut-points, i.e. the off-set of
                               trhe function is stored
    * Best method: Tradeoff between the previous two methods.
      Auxiliary variables, i.e., cut points are inserted following these
      criterias:
      * edgeInTh
        indicates the maximum number of incoming edges up to which
        no cut point (auxiliary variable) is inserted.
        If edgeInTh:
        * is equal to -1 no cut point due to incoming edges are inserted
          (MaxtermByMaxterm method.)
	* is equal to 0 a cut point is inserted for each node with a single
          incoming edge, i.e., each node, (NodeByNode method).
	* is equal to n a cut point is inserted for each node with (n+1)
          incoming edges.
      * pathLengthTh
        indicates the maximum length path up to which no cut points
        (auxiliary variable) is inserted.
        If the path length between two nodes exceeds this value, a cut point
        is inserted.
        If pathLengthTh:
        * is equal to -1 no cut point due path length are inserted
          (MaxtermByMaxterm method.)
	* is equal to 0 a cut point is inserted for each node (NodeByNode
          method).
	* is equal to n a cut point is inserted on path whose length is
          equal to (n+1).
        Notice that the maximum number of literals in a clause is equal
        to (pathLengthTh + 2), i.e., for each path we have to keep into
        account a CNF variable for each node plus 2 added variables for
        the bottom and top-path cut points.
    The stored file can contain a file header or not depending on the
    noHeader parameter (IFF 0, usual setting, the header is usually stored.
    This option can be useful in storing multiple BDDs, as separate BDDs,
    on the same file leaving the opening of the file to the caller. 
    ]

  SeeAlso      []

******************************************************************************/

int
Dddmp_cuddBddArrayStoreCnf (
  DdManager *ddMgr              /* IN: DD Manager */,
  DdNode **f                    /* IN: array of BDD roots to be stored */,
  int rootN                     /* IN: # output BDD roots to be stored */,
  Dddmp_DecompCnfStoreType mode /* IN: format selection */,
  int noHeader                  /* IN: do not store header iff 1 */,
  char **varNames               /* IN: array of variable names (or NULL) */,
  int *bddIds                   /* IN: array of converted var IDs */,
  int *bddAuxIds                /* IN: array of BDD node Auxiliary Ids */,
  int *cnfIds                   /* IN: array of converted var IDs */,
  int idInitial                 /* IN: starting id for cutting variables */,
  int edgeInTh                  /* IN: Max # Incoming Edges */,
  int pathLengthTh              /* IN: Max Path Length */,
  char *fname                   /* IN: file name */,
  FILE *fp                      /* IN: pointer to the store file */,
  int *clauseNPtr               /* OUT: number of clause stored */, 
  int *varNewNPtr               /* OUT: number of new variable created */ 
  )
{
  int retValue2;

#if 0
#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValue1;

  retValue1 = Cudd_DebugCheck (ddMgr);
  Dddmp_CheckAndReturn (retValue1==1,
    "Inconsistency Found During CNF Store.");
  Dddmp_CheckAndReturn (retValue1==CUDD_OUT_OF_MEM,
    "Out of Memory During CNF Store.");
#endif
#endif
#endif

  retValue2 = DddmpCuddBddArrayStoreCnf (ddMgr, f, rootN, mode, noHeader,
    varNames, bddIds, bddAuxIds, cnfIds, idInitial, edgeInTh, pathLengthTh,
    fname, fp, clauseNPtr, varNewNPtr);

#if 0
#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValue1 = Cudd_DebugCheck (ddMgr);
  Dddmp_CheckAndReturn (retValue1==1,
    "Inconsistency Found During CNF Store.");
  Dddmp_CheckAndReturn (retValue1==CUDD_OUT_OF_MEM,
    "Out of Memory During CNF Store.");
#endif
#endif
#endif

  return (retValue2);
}

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument Array of
    BDDs in the CNF standard format.
    ]

  Description  [Dumps the argument array of BDDs/ADDs to file in CNF format.
    The following arrays: varNames, bddIds, bddAuxIds, and cnfIds 
    fix the correspondence among variable names, BDD ids, BDD 
    auxiliary ids and the ids used to store the CNF problem.
    All these arrays are automatically created iff NULL.
    Auxiliary variable, iff necessary, are created starting from value
    idInitial.
    Iff idInitial is <= 0 its value is selected as the number of internal
    CUDD variable + 2.
    Auxiliary variables, i.e., cut points are inserted following these
    criterias:
    * edgeInTh
      indicates the maximum number of incoming edges up to which
      no cut point (auxiliary variable) is inserted.
      If edgeInTh:
      * is equal to -1 no cut point due to incoming edges are inserted
        (MaxtermByMaxterm method.)
	* is equal to 0 a cut point is inserted for each node with a single
        incoming edge, i.e., each node, (NodeByNode method).
	* is equal to n a cut point is inserted for each node with (n+1)
        incoming edges.
    * pathLengthTh
      indicates the maximum length path up to which no cut points
      (auxiliary variable) is inserted.
      If the path length between two nodes exceeds this value, a cut point
      is inserted.
      If pathLengthTh:
      * is equal to -1 no cut point due path length are inserted
        (MaxtermByMaxterm method.)
	* is equal to 0 a cut point is inserted for each node (NodeByNode
        method).
	* is equal to n a cut point is inserted on path whose length is
        equal to (n+1).
      Notice that the maximum number of literals in a clause is equal
      to (pathLengthTh + 2), i.e., for each path we have to keep into
      account a CNF variable for each node plus 2 added variables for
      the bottom and top-path cut points.
    ]

  SideEffects  [Nodes are temporarily removed from the unique hash table. 
    They are re-linked after the store operation in a modified 
    order.
    ]

  SeeAlso      [Dddmp_cuddBddStore]

******************************************************************************/

static int
DddmpCuddBddArrayStoreCnf (
  DdManager *ddMgr               /* IN: DD Manager */,
  DdNode **f                     /* IN: array of BDD roots to be stored */,
  int rootN                      /* IN: # of output BDD roots to be stored */,
  Dddmp_DecompCnfStoreType mode  /* IN: format selection */,
  int noHeader                   /* IN: do not store header iff 1 */,
  char **varNames                /* IN: array of variable names (or NULL) */,
  int *bddIds                    /* IN: array of BDD node Ids (or NULL) */,
  int *bddAuxIds                 /* IN: array of BDD Aux Ids (or NULL) */,
  int *cnfIds                    /* IN: array of CNF ids (or NULL) */,
  int idInitial                  /* IN: starting id for cutting variables */,
  int edgeInTh                   /* IN: Max # Incoming Edges */,
  int pathLengthTh               /* IN: Max Path Length */,
  char *fname                    /* IN: file name */,
  FILE *fp                       /* IN: pointer to the store file */,
  int *clauseNPtr                /* OUT: number of clause stored */,
  int *varNewNPtr                /* OUT: number of new variable created */ 
  )
{
  DdNode *support = NULL;
  DdNode *scan = NULL;
  int *bddIdsInSupport = NULL;
  int *permIdsInSupport = NULL;
  int *rootStartLine = NULL;
  int nVar, nVarInSupport, retValue, i, j, fileToClose;
  int varMax, clauseN, flagVar, intStringLength;
  int bddIdsToFree = 0;
  int bddAuxIdsToFree = 0;
  int cnfIdsToFree = 0;
  int varNamesToFree = 0;
  char intString[DDDMP_MAXSTRLEN];
  char tmpString[DDDMP_MAXSTRLEN];
  fpos_t posFile1, posFile2;

  /*---------------------------- Set Initial Values -------------------------*/

  support = scan = NULL;
  bddIdsInSupport = permIdsInSupport = rootStartLine = NULL;
  nVar = ddMgr->size;
  fileToClose = 0;
  sprintf (intString, "%d", INT_MAX);
  intStringLength = strlen (intString);

  /*---------- Check if File needs to be opened in the proper mode ----------*/

  if (fp == NULL) {
    fp = fopen (fname, "w");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  /*--------- Generate Bdd LOCAL IDs and Perm IDs and count them ------------*/

  /* BDD Ids */
  bddIdsInSupport = DDDMP_ALLOC (int, nVar);
  Dddmp_CheckAndGotoLabel (bddIdsInSupport==NULL, "Error allocating memory.",
    failure);
  /* BDD PermIds */
  permIdsInSupport = DDDMP_ALLOC (int, nVar);
  Dddmp_CheckAndGotoLabel (permIdsInSupport==NULL, "Error allocating memory.",
    failure);
  /* Support Size (Number of BDD Ids-PermIds */
  nVarInSupport = 0;

  for (i=0; i<nVar; i++) {
    bddIdsInSupport[i] = permIdsInSupport[i] = (-1);
  }

  /* 
   *  Take the union of the supports of each output function.
   *  Skip NULL functions.
   */


  for (i=0; i<rootN; i++) {
    if (f[i] == NULL) {
      continue;
    }
    support = Cudd_Support (ddMgr, f[i]);
    Dddmp_CheckAndGotoLabel (support==NULL, "NULL support returned.",
      failure);
    cuddRef (support);
    scan = support;
    while (!cuddIsConstant(scan)) {
      /* Count Number of Variable in the Support */
      nVarInSupport++;
      /* Set Ids and Perm-Ids */
      bddIdsInSupport[scan->index] = scan->index;
      permIdsInSupport[scan->index] = ddMgr->perm[scan->index];
      scan = cuddT (scan);
    }
    Cudd_RecursiveDeref (ddMgr, support);
  }
  /* so that we do not try to free it in case of failure */
  support = NULL;

  /*---------------------------- Start HEADER -------------------------------*/

  if (noHeader==0) {

    retValue = fprintf (fp,
      "c # BDD stored by the DDDMP tool in CNF format\n");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing on file.",
      failure);
    fprintf (fp, "c #\n");
  }

  /*-------------------- Generate Bdd IDs IFF necessary ---------------------*/

  if (bddIds == NULL) {
    if (noHeader==0) {
      fprintf (fp, "c # Warning: BDD IDs missing ... evaluating them.\n");
      fprintf (fp, "c # \n");
      fflush (fp);
    }

    bddIdsToFree = 1;
    bddIds = DDDMP_ALLOC (int, nVar);
    Dddmp_CheckAndGotoLabel (bddIds==NULL, "Error allocating memory.",
      failure);

    /* Get BDD-IDs Directly from Cudd Manager */
    for (i=0; i<nVar; i++) {
      bddIds[i] = i;
    }   
  } /* end if bddIds == NULL */

  /*------------------ Generate AUX BDD IDs IF necessary --------------------*/

  if (bddAuxIds == NULL) {
    if (noHeader==0) {
      fprintf (fp, "c # Warning: AUX IDs missing ... equal to BDD IDs.\n");
      fprintf (fp, "c #\n");
      fflush (fp);
    }

    bddAuxIdsToFree = 1;
    bddAuxIds = DDDMP_ALLOC (int, nVar);
    Dddmp_CheckAndGotoLabel (bddAuxIds==NULL, "Error allocating memory.",
      failure);

    for (i=0; i<nVar; i++) {
      bddAuxIds[i] = bddIds[i];
    }
  } /* end if cnfIds == NULL */

  /*------------------- Generate CNF IDs IF necessary -----------------------*/

  if (cnfIds == NULL) {
    if (noHeader==0) {
      fprintf (fp, "c # Warning: CNF IDs missing ... equal to BDD IDs.\n");
      fprintf (fp, "c #\n");
      fflush (fp);
    }

    cnfIdsToFree = 1;
    cnfIds = DDDMP_ALLOC (int, nVar);
    Dddmp_CheckAndGotoLabel (cnfIds==NULL, "Error allocating memory.",
      failure);

    for (i=0; i<nVar; i++) {
      cnfIds[i] = bddIds[i] + 1;
    }
  } /* end if cnfIds == NULL */

  /*------------------ Generate Var Names IF necessary ----------------------*/

  flagVar = 0;
  if (varNames == NULL) {
    if (noHeader==0) {
      fprintf (fp,
        "c # Warning: null variable names ... create DUMMY names.\n");
      fprintf (fp, "c #\n");
      fflush (stderr);
    }

    varNamesToFree = 1;
    varNames = DDDMP_ALLOC (char *, nVar);
    for (i=0; i<nVar; i++) {
       varNames[i] = NULL;       
    }
    Dddmp_CheckAndGotoLabel (varNames==NULL, "Error allocating memory.",
      failure);

    flagVar = 1;
  } else {
    /* Protect the user also from partially loaded varNames array !!! */
    for (i=0; i<nVar && flagVar==0; i++) {
      if (varNames[i] == NULL) {
        flagVar = 1;
      }
    }
  }

  if (flagVar == 1) {
    for (i=0; i<nVar; i++) {
      if (varNames[i] == NULL) {
        sprintf (tmpString, "DUMMY%d", bddIds[i]);
        varNames[i] = DDDMP_ALLOC (char, (strlen (tmpString)+1));
        strcpy (varNames[i], tmpString);
      }
    }
  }

  /*----------------------- Set Initial ID  IF necessary --------------------*/

  if (idInitial <= 0) {
    idInitial = nVar + 1;
  }

  /*--------------------------- Continue HEADER -----------------------------*/

  if (noHeader==0) {
    fprintf (fp, "c .ver %s\n", DDDMP_VERSION);
    fprintf (fp, "c .nnodes %d\n", Cudd_SharingSize (f, rootN));
    fprintf (fp, "c .nvars %d\n", nVar);
    fprintf (fp, "c .nsuppvars %d\n", nVarInSupport);

    /* Support Variable Names */
    if (varNames != NULL) {
      fprintf (fp, "c .suppvarnames");
      for (i=0; i<nVar; i++) {
        if (bddIdsInSupport[i] >= 0) {
          fprintf (fp, " %s", varNames[i]);
        }
      }
      fprintf (fp, "\n");
    }

    /* Ordered Variable Names */
    if (varNames != NULL) {
      fprintf (fp, "c .orderedvarnames");
      for (i=0; i<nVar; i++) {
        fprintf (fp, " %s", varNames[i]);
      }
      fprintf (fp, "\n");
    }

    /* BDD Variable Ids */
    fprintf (fp, "c .ids ");
    for (i=0; i<nVar; i++) {
      if (bddIdsInSupport[i] >= 0) {
        fprintf (fp, " %d", bddIdsInSupport[i]);
      }
    }
    fprintf (fp, "\n");

    /* BDD Variable Permutation Ids */
    fprintf (fp, "c .permids ");
    for (i=0; i<nVar; i++) {
      if (bddIdsInSupport[i] >= 0) {
        fprintf (fp, " %d", permIdsInSupport[i]);
      }
    }
    fprintf (fp, "\n");

    /* BDD Variable Auxiliary Ids */
    fprintf (fp, "c .auxids ");
    for (i=0; i<nVar; i++) {
      if (bddIdsInSupport[i] >= 0) {
        fprintf (fp, " %d", bddAuxIds[i]);
      }
    }
    fprintf (fp, "\n");

    /* CNF Ids */
    fprintf (fp, "c .cnfids ");
    for (i=0; i<nVar; i++) {
      if (bddIdsInSupport[i] >= 0) {
        fprintf (fp, " %d", cnfIds[i]);
      }
    }
    fprintf (fp, "\n");

    /* Number of Roots */
    fprintf (fp, "c .nroots %d", rootN);
    fprintf (fp, "\n");

    /* Root Starting Line */
    fgetpos (fp, &posFile1);
    fprintf (fp, "c .rootids");
    for (i=0; i<rootN; i++) {
      for (j=0; j<intStringLength+1; j++) {
        retValue = fprintf (fp, " ");
      }
    }
    retValue = fprintf (fp, "\n");
    fflush (fp);

  } /* End of noHeader check */

  /*------------ Select Mode and Print Number of Tmp Var Created ------------*/

  switch (mode) {
    case DDDMP_CNF_MODE_NODE:
      *varNewNPtr = idInitial;
      *varNewNPtr = DddmpNumberDdNodesCnf (ddMgr, f, rootN, cnfIds, idInitial)
        - *varNewNPtr;
      break;
    case DDDMP_CNF_MODE_MAXTERM:
      *varNewNPtr = 0;
      break;
    default:
      Dddmp_Warning (1, "Wrong DDDMP Store Mode. Force DDDMP_MODE_BEST.");
    case DDDMP_CNF_MODE_BEST:
      *varNewNPtr = idInitial;
      *varNewNPtr = DddmpDdNodesCountEdgesAndNumber (ddMgr, f, rootN,
        edgeInTh, pathLengthTh, cnfIds, idInitial) - *varNewNPtr;
      break;
  }

  /*------------ Print Space for Number of Variable and Clauses -------------*/

  if (noHeader==0) {
    fprintf (fp, "c .nAddedCnfVar %d\n", *varNewNPtr);
    fprintf (fp, "c #\n");
    fprintf (fp, "c # Init CNF Clauses\n");
    fprintf (fp, "c #\n");
    fgetpos (fp, &posFile2);
    retValue = fprintf (fp, "p cnf");
    for (j=0; j<2*(intStringLength+1); j++) {
      retValue = fprintf (fp, " ");
    }
    retValue = fprintf (fp, "\n");
    fflush (fp);
  }

  /*---------------------- Select Mode and Do the Job -----------------------*/

  clauseN = 0;
  varMax = -1;
  rootStartLine = DDDMP_ALLOC (int, rootN);
  Dddmp_CheckAndGotoLabel (rootStartLine==NULL, "Error allocating memory.",
    failure);
  for (i=0; i<rootN; i++) {
    rootStartLine[i] = (-1);
  }

  switch (mode) {
    case DDDMP_CNF_MODE_NODE:
      StoreCnfNodeByNode (ddMgr, f, rootN, bddIds, cnfIds, fp, &clauseN,
        &varMax, rootStartLine);
      DddmpUnnumberDdNodesCnf (ddMgr, f, rootN);
      break;
    case DDDMP_CNF_MODE_MAXTERM:
      StoreCnfMaxtermByMaxterm (ddMgr, f, rootN, bddIds, cnfIds, idInitial,
        fp, &varMax, &clauseN, rootStartLine);
      break;
    default:
      Dddmp_Warning (1, "Wrong DDDMP Store Mode. Force DDDMP_MODE_BEST.");
    case DDDMP_CNF_MODE_BEST:
      StoreCnfBest (ddMgr, f, rootN, bddIds, cnfIds, idInitial,
        fp, &varMax, &clauseN, rootStartLine);
      DddmpUnnumberDdNodesCnf (ddMgr, f, rootN);
      break;
  }

  /*------------------------------ Write trailer ----------------------------*/

  if (noHeader==0) {
    retValue = fprintf (fp, "c # End of Cnf From dddmp-2.0\n");
    Dddmp_CheckAndGotoLabel (retValue==EOF, "Error writing to file.",
      failure);
  }

  /*
   *  Write Root Starting Line
   */

  if (noHeader==0) {
    fsetpos (fp, &posFile1);
    fprintf (fp, "c .rootids");
    for (i=0; i<rootN; i++) {
      Dddmp_Warning (rootStartLine[i]==(-1),
        "Init Line for CNF file = (-1) {[(Stored one or zero BDD)]}.");
      sprintf (tmpString, " %d", rootStartLine[i]);
      for (j=strlen(tmpString); j<intStringLength+1; j++) {
        strcat (tmpString, " ");
      }
      retValue = fprintf (fp, "%s", tmpString);
    }
    retValue = fprintf (fp, "\n");
    fflush (fp);
  }

  /*
   *  Write Number of clauses and variable in the header 
   */

  *clauseNPtr = clauseN;

  if (noHeader==0) {
    fsetpos (fp, &posFile2);
    retValue = fprintf (fp, "p cnf");
    sprintf (tmpString, " %d %d", varMax, clauseN);
    for (j=strlen(tmpString); j<2*(intStringLength+1); j++) {
      strcat (tmpString, " ");
    }
    retValue = fprintf (fp, "%s\n", tmpString);
    fflush (fp);
  }

  /*-------------------------- Close file and return ------------------------*/

  if (fileToClose) {
    fclose (fp);
  }

  DDDMP_FREE (bddIdsInSupport);
  DDDMP_FREE (permIdsInSupport);
  DDDMP_FREE (rootStartLine);
  if (bddIdsToFree == 1) {
    DDDMP_FREE (bddIds);
  }
  if (bddAuxIdsToFree == 1) {
    DDDMP_FREE (bddAuxIds);
  }
  if (cnfIdsToFree == 1) {
    DDDMP_FREE (cnfIds);
  }
  if (varNamesToFree == 1) {
    for (i=0; i<nVar; i++) {
      DDDMP_FREE (varNames[i]);
    }
    DDDMP_FREE (varNames);
  }

  return (DDDMP_SUCCESS);

  failure:

    if (support != NULL) {
      Cudd_RecursiveDeref (ddMgr, support);
    }
    DDDMP_FREE (bddIdsInSupport);
    DDDMP_FREE (permIdsInSupport);
    DDDMP_FREE (rootStartLine);
    if (bddIdsToFree == 1) {
      DDDMP_FREE (bddIds);
    }
    if (bddAuxIdsToFree == 1) {
      DDDMP_FREE (bddAuxIds);
    }
    if (cnfIdsToFree == 1) {
      DDDMP_FREE (cnfIds);
    }
    if (varNamesToFree == 1) {
      for (i=0; i<nVar; i++) {
        DDDMP_FREE (varNames[i]);
      }
      DDDMP_FREE (varNames);
    }

    return (DDDMP_FAILURE);
}

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Store the BDD as CNF clauses.]

  Description  [Store the BDD as CNF clauses.
    Use a multiplexer description for each BDD node.
    ]

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

static int
StoreCnfNodeByNode (
  DdManager *ddMgr      /* IN: DD Manager */,
  DdNode **f            /* IN: BDD array to be stored */,
  int rootN             /* IN: number of BDDs in the array */,
  int *bddIds           /* IN: BDD ids for variables */,
  int *cnfIds           /* IN: CNF ids for variables */,
  FILE *fp              /* IN: store file */,
  int *clauseN      /* IN/OUT: number of clauses written in the CNF file */,
  int *varMax       /* IN/OUT: maximum value of id written in the CNF file */,
  int *rootStartLine   /* OUT: CNF line where root starts */
  )
{
  int retValue = 0;
  int i, idf;

  for (i=0; i<rootN; i++) {
    if (f[i] != NULL) {
      if (!cuddIsConstant(Cudd_Regular (f[i]))) {
        /*
         *  Set Starting Line for this Root
         */

        rootStartLine[i] = *clauseN + 1;

        /*
         *  Store the BDD
         */

        retValue = StoreCnfNodeByNodeRecur (ddMgr, Cudd_Regular(f[i]),
          bddIds, cnfIds, fp, clauseN, varMax);
        if (retValue == 0) {
          (void) fprintf (stderr,
            "DdStoreCnf: Error in recursive node store\n");
          fflush (stderr);
        }

        /*
         *  Store CNF for the root if necessary
         */

        idf = DddmpReadNodeIndexCnf (Cudd_Regular (f[i]));
#if DDDMP_DEBUG_CNF
        retValue = fprintf (fp, "root %d --> \n", i);
#endif
        if (Cudd_IsComplement (f[i])) {
          retValue = fprintf (fp, "-%d 0\n", idf);
        } else {
          retValue = fprintf (fp, "%d 0\n", idf);
        } 
        *varMax = GET_MAX (*varMax, idf);
        *clauseN = *clauseN + 1;

        if (retValue == EOF) {
          (void) fprintf (stderr,
            "DdStoreCnf: Error in recursive node store\n");
          fflush (stderr);
        }
      }
    }
  }

  return (retValue);
}

/**Function********************************************************************

  Synopsis     [Performs the recursive step of Dddmp_bddStore.]

  Description  [Performs the recursive step of Dddmp_bddStore.
    Traverse the BDD and store a CNF formula for each "terminal" node.
    ]

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

static int
StoreCnfNodeByNodeRecur (
  DdManager *ddMgr   /* IN: DD Manager */,
  DdNode *f          /* IN: BDD node to be stored */,
  int *bddIds        /* IN: BDD ids for variables */,
  int *cnfIds        /* IN: CNF ids for variables */,
  FILE *fp           /* IN: store file */,
  int *clauseN      /* OUT: number of clauses written in the CNF file */,
  int *varMax       /* OUT: maximum value of id written in the CNF file */
  )
{
  DdNode *T, *E;
  int idf, idT, idE, vf;
  int retValue;

#ifdef DDDMP_DEBUG
  assert(!Cudd_IsComplement(f));
  assert(f!=NULL);
#endif

  /* If constant, nothing to do. */
  if (Cudd_IsConstant(f)) {
    return (1);
  }

  /* If already visited, nothing to do. */
  if (DddmpVisitedCnf (f)) {
    return (1);
  }

  /* Mark node as visited. */
  DddmpSetVisitedCnf (f);

  /*------------------ Non Terminal Node -------------------------------*/

#ifdef DDDMP_DEBUG
  /* BDDs! Only one constant supported */
  assert (!cuddIsConstant(f));
#endif

  /* 
   *  Recursive call for Then edge
   */

  T = cuddT (f);
#ifdef DDDMP_DEBUG
  /* ROBDDs! No complemented Then edge */
  assert (!Cudd_IsComplement(T)); 
#endif
  /* recur */
  retValue = StoreCnfNodeByNodeRecur (ddMgr, T, bddIds, cnfIds, fp,
    clauseN, varMax);
  if (retValue != 1) {
    return(retValue);
  }

  /* 
   *  Recursive call for Else edge
   */

  E = Cudd_Regular (cuddE (f));
  retValue = StoreCnfNodeByNodeRecur (ddMgr, E, bddIds, cnfIds, fp,
    clauseN, varMax);
  if (retValue != 1) {
    return (retValue);
  }

  /* 
   *  Obtain nodeids and variable ids of f, T, E 
   */

  idf = DddmpReadNodeIndexCnf (f);
  vf = f->index;

  if (bddIds[vf] != vf) {
    (void) fprintf (stderr, "DdStoreCnf: Error writing to file\n");
    fflush (stderr);
    return (0);
  }

  idT = DddmpReadNodeIndexCnf (T);

  idE = DddmpReadNodeIndexCnf (E);
  if (Cudd_IsComplement (cuddE (f))) {
    idE = -idE;
  }

  retValue = StoreCnfOneNode (f, idf, cnfIds[vf], idT, idE, fp,
   clauseN, varMax);

  if (retValue == EOF) {
    return (0);
  } else {
    return (1);
  }
}

/**Function********************************************************************

  Synopsis     [Store One Single BDD Node.]

  Description  [Store One Single BDD Node translating it as a multiplexer.]

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

static int
StoreCnfOneNode (
  DdNode *f       /* IN: node to be stored */,
  int idf         /* IN: node CNF Index  */,
  int vf          /* IN: node BDD Index */,
  int idT         /* IN: Then CNF Index with sign = inverted edge */,
  int idE         /* IN: Else CNF Index with sign = inverted edge */,
  FILE *fp        /* IN: store file */,
  int *clauseN   /* OUT: number of clauses */,
  int *varMax    /* OUT: maximun Index of variable stored */
  )
{
  int retValue = 0;
  int idfAbs, idTAbs, idEAbs;

  idfAbs = abs (idf);
  idTAbs = abs (idT);
  idEAbs = abs (idE);

  /*----------------------------- Check for Constant ------------------------*/

  assert(!Cudd_IsConstant(f));

  /*------------------------- Check for terminal nodes ----------------------*/

  if ((idTAbs==1) && (idEAbs==1)) { 
    return (1);
  }

  /*------------------------------ Internal Node ----------------------------*/

#if DDDMP_DEBUG_CNF
  retValue = fprintf (fp, "id=%d var=%d idT=%d idE=%d\n",
    idf, vf, idT, idE);
#endif

  /*
   *  Then to terminal
   */

  if ((idTAbs==1) && (idEAbs!=1)) {
#if DDDMP_DEBUG_CNF
    retValue = fprintf (fp, "CASE 1 -->\n");
#endif
    retValue = fprintf (fp, "%d %d 0\n",
      idf, -vf);
    retValue = fprintf (fp, "%d %d 0\n",
      idf, -idE);
    retValue = fprintf (fp, "%d %d %d 0\n",
      -idf, vf, idE);
    *clauseN = *clauseN + 3;

    *varMax = GET_MAX (*varMax, idfAbs);
    *varMax = GET_MAX (*varMax, vf);
    *varMax = GET_MAX (*varMax, idEAbs);
  }

  /*
   *  Else to terminal
   */

  if ((idTAbs!=1) && (idEAbs==1)) {
    if (idE == 1) {
#if DDDMP_DEBUG_CNF
      retValue = fprintf (fp, "CASE 2 -->\n");
#endif
      retValue = fprintf (fp, "%d %d 0\n",
        idf, vf);
      retValue = fprintf (fp, "%d %d 0\n",
        idf, -idT);
      retValue = fprintf (fp, "%d %d %d 0\n",
        -idf, -vf, idT);
    } else {
#if DDDMP_DEBUG_CNF
      retValue = fprintf (fp, "CASE 3 -->\n");
#endif
      retValue = fprintf (fp, "%d %d 0\n",
        -idf, vf);
      retValue = fprintf (fp, "%d %d 0\n",
        -idf, idT);
      retValue = fprintf (fp, "%d %d %d 0\n",
        idf, -vf, -idT);
    }

    *varMax = GET_MAX (*varMax, idfAbs);
    *varMax = GET_MAX (*varMax, vf);
    *varMax = GET_MAX (*varMax, idTAbs);

    *clauseN = *clauseN + 3;
  }

  /*
   *  Nor Then or Else to terminal
   */

  if ((idTAbs!=1) && (idEAbs!=1)) {
#if DDDMP_DEBUG_CNF
    retValue = fprintf (fp, "CASE 4 -->\n");
#endif
    retValue = fprintf (fp, "%d %d %d 0\n",
      idf, vf, -idE);
    retValue = fprintf (fp, "%d %d %d 0\n",
      -idf, vf, idE);
    retValue = fprintf (fp, "%d %d %d 0\n",
      idf, -vf, -idT);
    retValue = fprintf (fp, "%d %d %d 0\n",
      -idf, -vf, idT);

    *varMax = GET_MAX (*varMax, idfAbs);
    *varMax = GET_MAX (*varMax, vf);
    *varMax = GET_MAX (*varMax, idTAbs);
    *varMax = GET_MAX (*varMax, idEAbs);

    *clauseN = *clauseN + 4;
  }

  return (retValue);
}

/**Function********************************************************************
 
  Synopsis    [Prints a disjoint sum of products.]
 
  Description [Prints a disjoint sum of product cover for the function
    rooted at node. Each product corresponds to a path from node a 
    leaf node different from the logical zero, and different from 
    the background value. Uses the standard output.  Returns 1 if 
    successful, 0 otherwise.
    ]
 
  SideEffects [None]
 
  SeeAlso     [StoreCnfBest]
 
******************************************************************************/

static int
StoreCnfMaxtermByMaxterm (
  DdManager *ddMgr    /* IN: DD Manager */,
  DdNode **f          /* IN: array of BDDs to store */,
  int rootN           /* IN: number of BDDs in the array */,
  int *bddIds         /* IN: BDD Identifiers */,
  int *cnfIds         /* IN: corresponding CNF Identifiers */,
  int idInitial       /* IN: initial value for numbering new CNF variables */,
  FILE *fp            /* IN: file pointer */,
  int *varMax        /* OUT: maximum identifier of the variables created */,
  int *clauseN       /* OUT: number of stored clauses */,
  int *rootStartLine /* OUT: line where root starts */
  )
{
  int i, j, *list;

  list = DDDMP_ALLOC (int, ddMgr->size);
  if (list == NULL) {
    ddMgr->errorCode = CUDD_MEMORY_OUT;
    return (DDDMP_FAILURE);
  }

  for (i=0; i<rootN; i++) {
    if (f[i] != NULL) {
      if (!cuddIsConstant(Cudd_Regular (f[i]))) {
        for (j=0; j<ddMgr->size; j++) {
          list[j] = 2;
        }

        /*
         *  Set Starting Line for this Root
         */

        rootStartLine[i] = *clauseN + 1;

        StoreCnfMaxtermByMaxtermRecur (ddMgr, f[i], bddIds, cnfIds, fp,
          list, clauseN, varMax);
      }
    }
  }

  FREE (list);

  return (1);
}

/**Function********************************************************************
 
  Synopsis    [Prints a disjoint sum of products with intermediate
    cutting points.]
 
  Description [Prints a disjoint sum of product cover for the function
    rooted at node intorducing cutting points whenever necessary.
    Each product corresponds to a path from node a leaf
    node different from the logical zero, and different from the
    background value. Uses the standard output.  Returns 1 if 
    successful, 0 otherwise.
    ]
 
  SideEffects [None]
 
  SeeAlso     [StoreCnfMaxtermByMaxterm]
 
******************************************************************************/

static int
StoreCnfBest (
  DdManager *ddMgr    /* IN: DD Manager */,
  DdNode **f          /* IN: array of BDDs to store */,
  int rootN           /* IN: number of BDD in the array */,
  int *bddIds         /* IN: BDD identifiers */,
  int *cnfIds         /* IN: corresponding CNF identifiers */,
  int idInitial       /* IN: initial value for numbering new CNF variables */,
  FILE *fp            /* IN: file pointer */,
  int *varMax        /* OUT: maximum identifier of the variables created */,
  int *clauseN       /* OUT: number of stored clauses */,
  int *rootStartLine /* OUT: line where root starts */
  )
{
  int i, j, *list;

  list = DDDMP_ALLOC (int, ddMgr->size);
  if (list == NULL) {
    ddMgr->errorCode = CUDD_MEMORY_OUT;
    return (DDDMP_FAILURE);
  }

  for (i=0; i<rootN; i++) {
    if (f[i] != NULL) {
      if (!cuddIsConstant(Cudd_Regular (f[i]))) {
        for (j=0; j<ddMgr->size; j++) {
          list[j] = 2;
        }

        /*
         *  Set Starting Line for this Root
         */

        rootStartLine[i] = *clauseN + 1;

#if DDDMP_DEBUG_CNF
        fprintf (fp, "root NOT shared BDDs %d --> \n", i);
#endif
        StoreCnfBestNotSharedRecur (ddMgr, f[i], 0, bddIds, cnfIds, fp, list,
          clauseN, varMax);

#if DDDMP_DEBUG_CNF
        fprintf (fp, "root SHARED BDDs %d --> \n", i);
#endif
        StoreCnfBestSharedRecur (ddMgr, Cudd_Regular (f[i]), bddIds, cnfIds,
          fp, list, clauseN, varMax);
      }
    }
  }

#if DDDMP_DEBUG_CNF
  fprintf (stdout, "###---> BDDs After the Storing Process:\n");
  DddmpPrintBddAndNext (ddMgr, f, rootN);
#endif

  FREE (list);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************
 
  Synopsis    [Performs the recursive step of Print Maxterm.]
 
  Description [Performs the recursive step of Print Maxterm.
    Traverse a BDD a print out a cube in CNF format each time a terminal
    node is reached.
    ]
 
  SideEffects [None]

  SeeAlso     []
 
******************************************************************************/

static void
StoreCnfMaxtermByMaxtermRecur (
  DdManager *ddMgr  /* IN: DD Manager */,
  DdNode *node      /* IN: BDD to store */,
  int *bddIds       /* IN: BDD identifiers */,
  int *cnfIds       /* IN: corresponding CNF identifiers */,
  FILE *fp          /* IN: file pointer */,
  int *list         /* IN: temporary array to store cubes */,
  int *clauseN     /* OUT: number of stored clauses */,
  int *varMax      /* OUT: maximum identifier of the variables created */
  )
{
  DdNode *N, *Nv, *Nnv;
  int retValue, index;
 
  N = Cudd_Regular (node);

  /*
   *  Terminal case: Print one cube based on the current recursion
   */

  if (cuddIsConstant (N)) {
    retValue = printCubeCnf (ddMgr, node, cnfIds, fp, list, varMax);
    if (retValue == DDDMP_SUCCESS) {
      fprintf (fp, "0\n");
      *clauseN = *clauseN + 1;
    }
    return;
  }

  /*
   *  NON Terminal case: Recur
   */

  Nv  = cuddT (N);
  Nnv = cuddE (N);
  if (Cudd_IsComplement (node)) {
    Nv  = Cudd_Not (Nv);
    Nnv = Cudd_Not (Nnv);
  }
  index = N->index;

  /*
   *  StQ 06.05.2003
   *  Perform the optimization:
   *  f = (a + b)' = (a') ^ (a + b') = (a') ^ (b')
   *  i.e., if the THEN node is the constant ZERO then that variable
   *  can be forgotten (list[index] = 2) for subsequent ELSE cubes
   */
  if (cuddIsConstant (Cudd_Regular (Nv)) &&  Nv != ddMgr->one) {
    list[index] = 2;
  } else {
    list[index] = 0;
  }
  StoreCnfMaxtermByMaxtermRecur (ddMgr, Nnv, bddIds, cnfIds, fp, list,
    clauseN, varMax);

  /*
   *  StQ 06.05.2003
   *  Perform the optimization:
   *  f = a ^ b = (a) ^ (a' + b) = (a) ^ (b)
   *  i.e., if the ELSE node is the constant ZERO then that variable
   *  can be forgotten (list[index] = 2) for subsequent THEN cubes
   */
  if (cuddIsConstant (Cudd_Regular (Nnv)) &&  Nnv != ddMgr->one) {
    list[index] = 2;
  } else {
    list[index] = 1;
  }
  StoreCnfMaxtermByMaxtermRecur (ddMgr, Nv, bddIds, cnfIds, fp, list,
    clauseN, varMax);
  list[index] = 2;

  return;
}

/**Function********************************************************************
 
  Synopsis    [Performs the recursive step of Print Best on Not Shared
    sub-BDDs.]
 
  Description [Performs the recursive step of Print Best on Not Shared
    sub-BDDs, i.e., print out information for the nodes belonging to
    BDDs not shared (whose root has just one incoming edge).
    ]
 
  SideEffects [None]

  SeeAlso     []
 
******************************************************************************/

static int
StoreCnfBestNotSharedRecur (
  DdManager *ddMgr   /* IN: DD Manager */,
  DdNode *node       /* IN: BDD to store */,
  int idf            /* IN: Id to store */,
  int *bddIds        /* IN: BDD identifiers */,
  int *cnfIds        /* IN: corresponding CNF identifiers */,
  FILE *fp           /* IN: file pointer */,
  int *list          /* IN: temporary array to store cubes */,
  int *clauseN      /* OUT: number of stored clauses */,
  int *varMax       /* OUT: maximum identifier of the variables created */
  )
{
  DdNode *N, *Nv, *Nnv;
  int index, retValue;
  DdNode *one;
    
  one = ddMgr->one;
 
  N = Cudd_Regular (node);

  /*
   *  Terminal case or Already Visited:
   *    Print one cube based on the current recursion
   */

  if (cuddIsConstant (N)) {
    retValue = printCubeCnf (ddMgr, node, cnfIds, fp, list, varMax);
    if (retValue == DDDMP_SUCCESS) {
      if (idf != 0) {
         fprintf (fp, "%d ", idf);
      }
      fprintf (fp, "0\n");
      *varMax = GET_MAX (*varMax, abs(idf));
      *clauseN = *clauseN + 1;
    }
    return (DDDMP_SUCCESS);
  }

  /*
   *  Shared Sub-Tree: Print Cube
   */

  index = DddmpReadNodeIndexCnf (N);
  if (index > 0) {
    if (idf != 0) {
      fprintf (fp, "%d ", idf);
    }
    if (Cudd_IsComplement (node)) {
      retValue = fprintf (fp, "-%d ", index);
    } else {
      retValue = fprintf (fp, "%d ", index);
    }
    retValue = printCubeCnf (ddMgr, node, cnfIds, fp, list, varMax);
    fprintf (fp, "0\n");
    *varMax = GET_MAX (*varMax, abs(index));
    *clauseN = *clauseN + 1;
    return (DDDMP_SUCCESS);
  }

  /*
   *  NON Terminal case: Recur
   */

  Nv  = cuddT (N);
  Nnv = cuddE (N);
  if (Cudd_IsComplement (node)) {
    Nv  = Cudd_Not (Nv);
    Nnv = Cudd_Not (Nnv);
  }
  index = N->index;

  /*
   *  StQ 06.05.2003
   *  Perform the optimization:
   *  f = (a + b)' = (a') ^ (a + b') = (a') ^ (b')
   *  i.e., if the THEN node is the constant ZERO then that variable
   *  can be forgotten (list[index] = 2) for subsequent ELSE cubes
   */
  if (cuddIsConstant (Cudd_Regular (Nv)) &&  Nv != ddMgr->one) {
    list[index] = 2;
  } else {
    list[index] = 0;
  }
  StoreCnfBestNotSharedRecur (ddMgr, Nnv, idf, bddIds, cnfIds, fp, list,
    clauseN, varMax);

  /*
   *  StQ 06.05.2003
   *  Perform the optimization:
   *  f = a ^ b = (a) ^ (a' + b) = (a) ^ (b)
   *  i.e., if the ELSE node is the constant ZERO then that variable
   *  can be forgotten (list[index] = 2) for subsequent THEN cubes
   */
  if (cuddIsConstant (Cudd_Regular (Nnv)) &&  Nnv != ddMgr->one) {
    list[index] = 2;
  } else {
    list[index] = 1;
  }
  StoreCnfBestNotSharedRecur (ddMgr, Nv, idf, bddIds, cnfIds, fp, list,
    clauseN, varMax);
  list[index] = 2;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************
 
  Synopsis    [Performs the recursive step of Print Best on Shared
    sub-BDDs.
    ]
 
  Description [Performs the recursive step of Print Best on Not Shared
    sub-BDDs, i.e., print out information for the nodes belonging to
    BDDs not shared (whose root has just one incoming edge).
    ]
 
  SideEffects [None]

  SeeAlso      []
 
******************************************************************************/

static int
StoreCnfBestSharedRecur (
  DdManager *ddMgr  /* IN: DD Manager */,
  DdNode *node      /* IN: BDD to store */,
  int *bddIds       /* IN: BDD identifiers */,
  int *cnfIds       /* IN: corresponding CNF identifiers */,
  FILE *fp          /* IN: file pointer */,
  int *list         /* IN: temporary array to store cubes */,
  int *clauseN     /* OUT: number of stored clauses */,
  int *varMax      /* OUT: maximum identifier of the variables created */
  )
{
  DdNode *nodeThen, *nodeElse;
  int i, idf, index;
  DdNode *one;
    
  one = ddMgr->one;

  Dddmp_Assert (node==Cudd_Regular(node),
    "Inverted Edge during Shared Printing.");

  /* If constant, nothing to do. */
  if (cuddIsConstant (node)) {
    return (DDDMP_SUCCESS);
  }

  /* If already visited, nothing to do. */
  if (DddmpVisitedCnf (node)) {
    return (DDDMP_SUCCESS);
  }

  /*
   *  Shared Sub-Tree: Print Cube
   */

  idf = DddmpReadNodeIndexCnf (node);
  if (idf > 0) {
    /* Cheat the Recur Function about the Index of the Current Node */
    DddmpWriteNodeIndexCnf (node, 0);

#if DDDMP_DEBUG_CNF
    fprintf (fp, "Else of XNOR\n");
#endif
    for (i=0; i<ddMgr->size; i++) {
      list[i] = 2;
    }
    StoreCnfBestNotSharedRecur (ddMgr, Cudd_Not (node), idf, bddIds, cnfIds,
      fp, list, clauseN, varMax);

#if DDDMP_DEBUG_CNF
    fprintf (fp, "Then of XNOR\n");
#endif
    for (i=0; i<ddMgr->size; i++) {
      list[i] = 2;
    }
    StoreCnfBestNotSharedRecur (ddMgr, node, -idf, bddIds, cnfIds,
      fp, list, clauseN, varMax);

    /* Set Back Index of Current Node */
    DddmpWriteNodeIndexCnf (node, idf);
  }

  /* Mark node as visited. */
  DddmpSetVisitedCnf (node);

  /*
   *  Recur
   */

  nodeThen  = cuddT (node);
  nodeElse = cuddE (node);
  index = node->index;

  StoreCnfBestSharedRecur (ddMgr, Cudd_Regular (nodeThen), bddIds, cnfIds,
    fp, list, clauseN, varMax);
  StoreCnfBestSharedRecur (ddMgr, Cudd_Regular (nodeElse), bddIds, cnfIds,
    fp, list, clauseN, varMax);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************
 
  Synopsis    [Print One Cube in CNF Format.]
 
  Description [Print One Cube in CNF Format.
    Return DDDMP_SUCCESS if something is printed out, DDDMP_FAILURE
    is nothing is printed out.
    ]
 
  SideEffects [None]

  SeeAlso      []
 
******************************************************************************/

static int
printCubeCnf (
  DdManager *ddMgr /* IN: DD Manager */,
  DdNode *node     /* IN: BDD to store */,
  int *cnfIds      /* IN: CNF identifiers */,
  FILE *fp         /* IN: file pointer */,
  int *list        /* IN: temporary array to store cubes */,
  int *varMax      /* OUT: maximum identifier of the variables created */
  )
{
  int i, retValue;
  DdNode *one;
    
  retValue = DDDMP_FAILURE;
  one = ddMgr->one;

  if (node != one) {
    for (i=0; i<ddMgr->size; i++) {
      if (list[i] == 0) {
        retValue = DDDMP_SUCCESS;
        (void) fprintf (fp, "%d ", cnfIds[i]);
	*varMax = GET_MAX(*varMax, cnfIds[i]);
      } else {
        if (list[i] == 1) {
          retValue = DDDMP_SUCCESS;
          (void) fprintf (fp, "-%d ", cnfIds[i]);
  	  *varMax = GET_MAX(*varMax, cnfIds[i]);
        }
      }
    }
  }

  return (retValue);
}





