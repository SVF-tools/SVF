/**CFile**********************************************************************

  FileName     [testdddmp.c]
 
  PackageName  [dddmp]

  Synopsis     [A simple test function for Dddmp package]

  Description  [This program constitutes a simple test program
    for the dddmp library (version 2.0).
    A simple interactive command selection allow the users to perform the
    main operation on BDDs, ADDs, and CNF, such as loading and storing.
    It can work also as a BDD calculators.
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

#include <string.h>
#include <stdio.h>
#include "CUDD/dddmpInt.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DDDMPTEST_MAX_FILENAME_LENGTH 256
#define DDDMPTEST_MAX_STRING_LENGTH 80
#define DDDMPTEST_MAX_OPERAND  20
#define DDDMPTEST_MAX_VARIABLE 50
#define DDDMP_MAX_BDDARRAY_LEN 1000

/**Enum************************************************************************

  Synopsis    [Message type for output messages]

  Description [Type supported by the output function to print-out
    the proper message.
    ]

******************************************************************************/

typedef enum {
  /* Int Messages */
  DDDMP_MESSAGE_MANAGER_VAR,
  DDDMP_MESSAGE_BDD,
  DDDMP_MESSAGE_BDD_ARRAY,
  DDDMP_MESSAGE_SOURCE1,
  DDDMP_MESSAGE_SOURCE2,
  DDDMP_MESSAGE_DESTINATION,
  DDDMP_MESSAGE_CUBE,
  DDDMP_MESSAGE_INDEX,
  DDDMP_MESSAGE_I_ID,
  DDDMP_MESSAGE_EDGE_MAX,
  DDDMP_MESSAGE_LENGHT_MAX,
  DDDMP_MESSAGE_REORDERING,
  /* String Messages */
  DDDMP_MESSAGE_PROMPT,
  DDDMP_MESSAGE_FILE,
  DDDMP_MESSAGE_OP,
  DDDMP_MESSAGE_FORMAT
} Dddmp_MessageType;

#if !defined(RAND_MAX) && defined(sun) && defined(sparc)
#define RAND_MAX 2147483647
#endif

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

typedef struct dddmpVarInfo {
  /*
   *  Local Information
   */

  int nDdVars;       /* Local Manager Number of Variables */
  char **rootNames;  /* Root Names */

  /*
   *  Header File Information
   */

  Dddmp_DecompType ddType;

  int nVars;      /* File Manager Number of Variables */
  int nSuppVars;  /* File Structure Number of Variables */

  int varNamesFlagUpdate;      /* 0 to NOT Update */
  char **suppVarNames;
  char **orderedVarNames;

  int varIdsFlagUpdate;        /* 0 to NOT Update */
  int *varIds;                 /* File ids - nSuppVars size */
  int *varIdsAll;              /* ALL ids - nVars size */

  int varComposeIdsFlagUpdate; /* 0 to NOT Update */
  int *varComposeIds;          /* File permids - nSuppVars size */
  int *varComposeIdsAll;       /* ALL permids - nVars size */

  int varAuxIdsFlagUpdate;     /* 0 to NOT Update */
  int *varAuxIds;              /* File auxids - nSuppVars size */
  int *varAuxIdsAll;           /* ALL auxids  - nVars size */

  int nRoots;
} dddmpVarInfo_t;

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

Dddmp_RootMatchType rootmatchmode;
Dddmp_VarMatchType varmatchmode;
Dddmp_VarInfoType varoutinfo;
char varname[DDDMPTEST_MAX_STRING_LENGTH];

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdManager *ManagerInit (dddmpVarInfo_t *varInfo);
static void ManagerQuit (DdManager **ddMgr, dddmpVarInfo_t *varInfo);
static int OneCreate(DdManager *ddMgr, DdNode **operandBdd);
static int BddZeroCreate(DdManager *ddMgr, DdNode **operandBdd);
static int LeafCreate(DdManager *ddMgr, DdNode **operandBdd);
static int BddCreate(DdManager *ddMgr, DdNode **operandBdd);
static int A2B(void);
static int B2A(void);
static int HeaderLoadBdd(dddmpVarInfo_t *varInfo);
static int HeaderLoadCnf(dddmpVarInfo_t *varInfo);
static int HeaderWrite(dddmpVarInfo_t *varInfo);
static int Help(void);
static int OrderNamesLoad(dddmpVarInfo_t *varInfo);
static int IntArrayLoad(dddmpVarInfo_t *varInfo, const char *mode);
static int BddLoad(DdManager *ddMgr, DdNode **operandBdd, dddmpVarInfo_t *varInfo);
static int BddArrayLoad(DdManager *ddMgr, DdNode ***operandBddArray, int *operandBddArraySize, dddmpVarInfo_t *varInfo);
static int AddLoad(DdManager *ddMgr, DdNode **operandBdd, dddmpVarInfo_t *varInfo);
static int AddArrayLoad(DdManager *ddMgr, DdNode ***operandBddArray, int *operandBddArraySize, dddmpVarInfo_t *varInfo);
static int BddLoadCnf(DdManager *ddMgr, DdNode **operandBdd, dddmpVarInfo_t *varInfo);
static int BddArrayLoadCnf(DdManager *ddMgr, DdNode ***operandBddArray, int *operandBddArraySize, dddmpVarInfo_t *varInfo);
static int Operation(DdManager *ddMgr, DdNode **operandBdd);
static int BddStore(DdManager *ddMgr, DdNode **operandBdd, dddmpVarInfo_t *varInfo);
static int BddArrayStore(DdManager *ddMgr, DdNode ***operandBddArray, int *operandBddArraySize, dddmpVarInfo_t *varInfo);
static int AddStore(DdManager *ddMgr, DdNode **operandBdd, dddmpVarInfo_t *varInfo);
static int AddArrayStore(DdManager *ddMgr, DdNode ***operandBddArray, int *operandBddArraySize, dddmpVarInfo_t *varInfo);
static int BddStoreCnf(DdManager *ddMgr, DdNode **operandBdd, dddmpVarInfo_t *varInfo);
static int BddArrayStoreCnf(DdManager *ddMgr, DdNode ***operandBddArray, int *operandBddArraySize, dddmpVarInfo_t *varInfo);
static int DynamicReordering(DdManager *ddMgr);
static int SetLoadMatchmode();
static int CompleteInfoStruct(Dddmp_DecompType ddType, int nVars, int nSuppVars, char **suppVarNames, char **orderedVarNames, int *varIds, int *varComposeIds, int *varAuxIds, int nRoots, dddmpVarInfo_t *varInfo);
static void ReadInt(Dddmp_MessageType message, int *i);
static void ReadString(Dddmp_MessageType message, char string[]);

/**AutomaticEnd***************************************************************/

int
main(
  int argc,
  char **argv
  )
{
  DdManager *ddMgr = NULL;
  DdNode **operandBdd = NULL;
  DdNode ***operandBddArray = NULL;
  int *operandBddArraySize = NULL;
  char *row = NULL;
  dddmpVarInfo_t varInfo;
  int i;

  /*--------------------- Echo command line and arguments -------------------*/

  fprintf (stdout, "#");
  for (i=0; i<argc; i++) {
    fprintf (stdout, "%s Version 2.0.2 (use command help)", argv[i]);
  }
  fprintf (stdout, "\n");
  if (argc>1) {
    Help();
  }

  /*-------------------------- Init Array of BDDs ---------------------------*/

 rootmatchmode = DDDMP_ROOT_MATCHLIST;
#if 1
  varmatchmode = DDDMP_VAR_MATCHIDS;
#else
  varmatchmode = DDDMP_VAR_MATCHNAMES;
#endif
  varoutinfo = DDDMP_VARIDS;

  row = DDDMP_ALLOC (char, DDDMPTEST_MAX_STRING_LENGTH);
  Dddmp_CheckAndReturn (row==NULL, "Allocation error.");

  operandBdd = DDDMP_ALLOC (DdNode *, DDDMPTEST_MAX_OPERAND);
  Dddmp_CheckAndReturn (operandBdd==NULL, "Allocation error.");

  operandBddArray = DDDMP_ALLOC (DdNode **, DDDMPTEST_MAX_OPERAND);
  Dddmp_CheckAndReturn (operandBddArray==NULL, "Allocation error.");

  operandBddArraySize = DDDMP_ALLOC (int, DDDMPTEST_MAX_OPERAND);
  Dddmp_CheckAndReturn (operandBddArraySize==NULL, "Allocation error.");

  for (i=0; i<DDDMPTEST_MAX_OPERAND; i++) {
    operandBdd[i] = NULL;
    operandBddArray[i] = NULL;
    operandBddArraySize[i] = 0;
  }

  /*--------------------- Manage command line parameters --------------------*/

  while (1) {
    ReadString (DDDMP_MESSAGE_PROMPT, row);
    if (row[0]=='\n') {
      continue;
    }
    if (strncmp (row, "help", 4)==0) {
      Help();
    } else if (strncmp (row, "mi", 2)==0) {
      ddMgr = ManagerInit (&varInfo);
    } else if (strncmp (row, "mq", 2)==0) {
      ManagerQuit (&ddMgr, &varInfo);
    } else if (strncmp (row, "onl", 3)==0) {
      OrderNamesLoad (&varInfo);
    } else if (strncmp (row, "oil", 3)==0) {
      IntArrayLoad (&varInfo, "oil");
    } else if (strncmp (row, "cil", 3)==0) {
      IntArrayLoad (&varInfo, "cil");
    } else if (strncmp (row, "slm", 3)==0) {
      SetLoadMatchmode ();
    } else if (strncmp (row, "op", 2)==0) {
      Operation (ddMgr, operandBdd);
    } else if (strncmp (row, "oc", 2)==0) {
      OneCreate (ddMgr, operandBdd);
    } else if (strncmp (row, "zc", 2)==0) {
      BddZeroCreate (ddMgr, operandBdd);
    } else if (strncmp (row, "lc", 2)==0) {
      LeafCreate (ddMgr, operandBdd);
    } else if (strncmp (row, "bc", 2)==0) {
      BddCreate (ddMgr, operandBdd);
    } else if (strncmp (row, "a2b", 3)==0) {
      A2B ();
    } else if (strncmp (row, "b2a", 3)==0) {
      B2A ();
    } else if (strncmp (row, "hlb", 3)==0) {
      HeaderLoadBdd (&varInfo);
    } else if (strncmp (row, "hlc", 3)==0) {
      HeaderLoadCnf (&varInfo);
    } else if (strncmp (row, "bl", 3)==0) {
      BddLoad (ddMgr, operandBdd, &varInfo);
    } else if (strncmp (row, "bal", 3)==0) {
      BddArrayLoad (ddMgr, operandBddArray, operandBddArraySize, &varInfo);
    } else if (strncmp (row, "al", 2)==0) {
      AddLoad (ddMgr, operandBdd, &varInfo);
    } else if (strncmp (row, "aal", 3)==0) {
      AddArrayLoad (ddMgr, operandBddArray, operandBddArraySize, &varInfo);
    } else if (strncmp (row, "cl", 2)==0) {
      BddLoadCnf (ddMgr, operandBdd, &varInfo);
    } else if (strncmp (row, "cal", 3)==0) {
      BddArrayLoadCnf (ddMgr, operandBddArray, operandBddArraySize, &varInfo);
    } else if (strncmp (row, "hw", 2)==0) {
      HeaderWrite (&varInfo);
    } else if (strncmp (row, "bs", 2)==0) {
      BddStore (ddMgr, operandBdd, &varInfo);
    } else if (strncmp (row, "bas", 3)==0) {
      BddArrayStore (ddMgr, operandBddArray, operandBddArraySize, &varInfo);
    } else if (strncmp (row, "as", 2)==0) {
      AddStore (ddMgr, operandBdd, &varInfo);
    } else if (strncmp (row, "aas", 2)==0) {
      AddArrayStore (ddMgr, operandBddArray, operandBddArraySize, &varInfo);
    } else if (strncmp (row, "cs", 2)==0) {
      BddStoreCnf (ddMgr, operandBdd, &varInfo);
    } else if (strncmp (row, "cas", 2)==0) {
      BddArrayStoreCnf (ddMgr, operandBddArray, operandBddArraySize, &varInfo);
    } else if (strncmp (row, "dr", 2)==0) {
      DynamicReordering (ddMgr);
    } else if (strncmp (row, "quit", 4)==0) {
      break;
    } else {
      fprintf (stderr, "Command not found: %s\n", row);
    }
  }

  /*-------------------------------- Free Memory ----------------------------*/

  ManagerQuit (&ddMgr, &varInfo);

  DDDMP_FREE (row);
  DDDMP_FREE (operandBdd);
  for (i=0; i<DDDMPTEST_MAX_OPERAND; i++) {
    if (operandBddArray[i] != NULL) {
      DDDMP_FREE (operandBddArray[i]);
    }
  }
  DDDMP_FREE (operandBddArray);
  DDDMP_FREE (operandBddArraySize);

  fprintf (stdout, "End of test.\n");

  return (DDDMP_SUCCESS);
}

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Create a CUDD Manager with nVars variables.]
  
  Description  [Create a CUDD Manager with nVars variables.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static DdManager *
ManagerInit (
  dddmpVarInfo_t *varInfo
  )
{
  DdManager *ddMgr = NULL;
  int nVars;

  ReadInt (DDDMP_MESSAGE_MANAGER_VAR, &nVars);

  /*----------------------- Init Var Information Structure ------------------*/

  varInfo->nDdVars = nVars;

  varInfo->rootNames = NULL;
  varInfo->ddType = DDDMP_NONE;
  varInfo->nVars = (-1);
  varInfo->nSuppVars = (-1);
  varInfo->varNamesFlagUpdate = 1;
  varInfo->suppVarNames = NULL;
  varInfo->orderedVarNames = NULL;
  varInfo->varIdsFlagUpdate = 1;
  varInfo->varIds = NULL;
  varInfo->varIdsAll = NULL;
  varInfo->varComposeIdsFlagUpdate = 1;
  varInfo->varComposeIds = NULL;
  varInfo->varComposeIdsAll = NULL;
  varInfo->varAuxIdsFlagUpdate = 1;
  varInfo->varAuxIds = NULL;
  varInfo->varAuxIdsAll = NULL;
  varInfo->nRoots = (-1);

  /*------------------------------ Init DD Manager --------------------------*/

  ddMgr = Cudd_Init (nVars, 0, CUDD_UNIQUE_SLOTS,
    CUDD_CACHE_SLOTS, 0);

  Dddmp_CheckAndReturn (ddMgr==NULL, "DdManager NOT inizializated.");

  return (ddMgr);
}

/**Function********************************************************************

  Synopsis     [Quit a CUDD Manager.]
  
  Description  [Quit a CUDD Manager.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static void
ManagerQuit (
  DdManager **ddMgrPtr      /* IN: CUDD Manager */,
  dddmpVarInfo_t *varInfo   /* IN: Internal Manager */
  )
{
  if (*ddMgrPtr == NULL) {
    return;
  }

  fprintf (stdout, "Quitting CUDD Manager.\n");
  Cudd_Quit (*ddMgrPtr);
  *ddMgrPtr = NULL;

  DddmpStrArrayFree (varInfo->rootNames, varInfo->nRoots);
  DddmpStrArrayFree (varInfo->suppVarNames, varInfo->nSuppVars);
  DddmpStrArrayFree (varInfo->orderedVarNames, varInfo->nVars);
  DDDMP_FREE (varInfo->varIds);
  DDDMP_FREE (varInfo->varIdsAll);
  DDDMP_FREE (varInfo->varComposeIds);
  DDDMP_FREE (varInfo->varComposeIdsAll);
  DDDMP_FREE (varInfo->varAuxIds);
  DDDMP_FREE (varInfo->varAuxIdsAll);

  varInfo->nDdVars = (-1);
  varInfo->rootNames = NULL;
  varInfo->ddType = DDDMP_NONE;
  varInfo->nVars = (-1);
  varInfo->nSuppVars = (-1);
  varInfo->varNamesFlagUpdate = 1;
  varInfo->suppVarNames = NULL;
  varInfo->orderedVarNames = NULL;
  varInfo->varIdsFlagUpdate = 1;
  varInfo->varIds = NULL;
  varInfo->varIdsAll = NULL;
  varInfo->varComposeIdsFlagUpdate = 1;
  varInfo->varComposeIds = NULL;
  varInfo->varComposeIdsAll = NULL;
  varInfo->varAuxIdsFlagUpdate = 1;
  varInfo->varAuxIds = NULL;
  varInfo->varAuxIdsAll = NULL;
  varInfo->nRoots = (-1);

  return;
}
                                                                       
/**Function********************************************************************

  Synopsis     [Create a One-BDD Leaf.]
  
  Description  [Create a One-BDD Leaf.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
OneCreate(
  DdManager *ddMgr      /* IN: CUDD Manager */,
  DdNode **operandBdd   /* In/OUT: Array of operand */
  )
{
  int i;

  /*------------------------ Read Operation Operands ------------------------*/

  ReadInt (DDDMP_MESSAGE_BDD, &i);

  operandBdd[i] = Cudd_ReadOne (ddMgr);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Create a Zero-BDD Leaf.]
  
  Description  [Create a Zero-BDD Leaf.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddZeroCreate(
  DdManager *ddMgr      /* IN: CUDD Manager */,
  DdNode **operandBdd   /* IN/OUT: array of operand */
  )
{
  int i;
  DdNode *one = NULL;

  /*------------------------ Read Operation Operands ------------------------*/

  ReadInt (DDDMP_MESSAGE_BDD, &i);

  one = Cudd_ReadOne(ddMgr);
  operandBdd[i] = Cudd_Not(one);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Create a One-Node BDD.]
  
  Description  [Create a One-Node BDD.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
LeafCreate(
  DdManager *ddMgr      /* IN: CUDD Manager */,
  DdNode **operandBdd   /* IN/OUT: Array of operandBdd */
  )
{
  int i, j;
  DdNode *f = NULL;

  /*------------------------ Read Operation Operands ------------------------*/

  ReadInt (DDDMP_MESSAGE_BDD, &i);
  ReadInt (DDDMP_MESSAGE_INDEX, &j);

  f = Cudd_bddIthVar (ddMgr, j);
  Cudd_Ref(f);
  operandBdd[i] = f;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************
 
  Synopsis     [Create a BDD.]
  
  Description  [Create a BDD: Variable index and number of cubes selection.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddCreate (
  DdManager *ddMgr      /* IN: CUDD Manager */,
  DdNode **operandBdd   /* array of operandBdd */
  )
{
  DdNode **vet, *f, *g, *h;
  int nb, nv, vi0, vi1, nc, i, j;
  char row[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadInt (DDDMP_MESSAGE_BDD, &nb);

  fprintf (stdout, "Variables Index [n-m] (m-n = number of variables): ");
  fgets (row, DDDMPTEST_MAX_STRING_LENGTH, stdin);
  sscanf (row, "%d-%d", &vi0, &vi1);
  nv = vi1-vi0+1;

  ReadInt (DDDMP_MESSAGE_CUBE, &nc);

  /* Leaf Creation */
  vet = DDDMP_ALLOC (DdNode *, nv);
  for (i=0; i<nv; i++) {
     vet[i] = Cudd_bddIthVar (ddMgr, vi0+i);
  }

  /* Cubes and BDD creation */
  f = Cudd_Not (Cudd_ReadOne (ddMgr));
  for (i=0; i<nc; i++)
    {
    g = Cudd_ReadOne (ddMgr);
    for (j=0; j<nv; j++)
      {
      if ( ((float) rand())/((float) RAND_MAX) > 0.5 ) {
        h = Cudd_bddAnd (ddMgr, g, vet[j]);
      } else {
        h = Cudd_bddAnd (ddMgr, g, Cudd_Not (vet[j]));
      }
      Cudd_Ref (h);
      Cudd_RecursiveDeref (ddMgr, g);
      g = h;
      }
    h = Cudd_bddOr (ddMgr, f, g);
    Cudd_Ref (h);
    Cudd_RecursiveDeref (ddMgr, f);
    Cudd_RecursiveDeref (ddMgr, g);
    f = h;
    }
      
  operandBdd[nb] = f;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Transform a BDD from the ASCII to the Binary format].]
  
  Description  [Input and Output file selection.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
A2B(
  void
)
{
  fprintf (stderr, "Not yet Implemented!!!\n");

  return (DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Transform a BDD from the Binary to the ASCII format].]
  
  Description  [Input and Output file selection.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
B2A(
  void
)
{
  fprintf (stderr, "Not yet Implemented!!!\n");

  return (DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Read the Header of a file containing a BDD.]
  
  Description  [File name Selection.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
HeaderLoadBdd (
  dddmpVarInfo_t *varInfo      /* IN/OUT: Variable Information */
  )
{
  Dddmp_DecompType ddType;
  int retValue, nRoots, nVars, nSuppVars;
  int *tmpVarIds = NULL;
  int *tmpVarAuxIds = NULL;
  int *tmpVarComposeIds = NULL;
  char **tmpOrderedVarNames = NULL;
  char **tmpSuppVarNames = NULL;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);

  retValue = Dddmp_cuddHeaderLoad (&ddType, &nVars, &nSuppVars,
    &tmpSuppVarNames, &tmpOrderedVarNames, &tmpVarIds, &tmpVarComposeIds,
    &tmpVarAuxIds, &nRoots, fileName, NULL);

  if (retValue == DDDMP_FAILURE) {
    return (DDDMP_FAILURE);
  }

  /*---------------------------- Tail Operations ----------------------------*/

  CompleteInfoStruct (ddType, nVars, nSuppVars,
    tmpSuppVarNames, tmpOrderedVarNames, tmpVarIds, tmpVarComposeIds,
    tmpVarAuxIds, nRoots, varInfo);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Read the Header of a file containing a CNF formula.]
  
  Description  [File name Selection.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
HeaderLoadCnf (
  dddmpVarInfo_t *varInfo      /* IN/OUT: Variable Information */
  )
{
  int retValue, nRoots, nVars, nSuppVars;
  int *tmpVarIds = NULL;
  int *tmpVarComposeIds = NULL;
  int *tmpVarAuxIds = NULL;
  char **tmpOrderedVarNames = NULL;
  char **tmpSuppVarNames = NULL;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);

  retValue = Dddmp_cuddHeaderLoadCnf (&nVars, &nSuppVars,
   &tmpSuppVarNames, &tmpOrderedVarNames, &tmpVarIds, &tmpVarComposeIds,
   &tmpVarAuxIds, &nRoots, fileName, NULL);

  if (retValue == DDDMP_FAILURE) {
    return (DDDMP_FAILURE);
  }

  /*---------------------------- Tail Operations ----------------------------*/

  CompleteInfoStruct (DDDMP_CNF, nVars, nSuppVars,
    tmpSuppVarNames, tmpOrderedVarNames, tmpVarIds, tmpVarComposeIds,
    tmpVarAuxIds, nRoots, varInfo);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Read the Header of a filke containing a BDD.]
  
  Description  [File name Selection.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
HeaderWrite(
  dddmpVarInfo_t *varInfo      /* IN/OUT: Variable Information */
  )
{
  int i;

  switch (varInfo->ddType) {
    case DDDMP_BDD:
      fprintf (stdout, "DD TYPE: DDDMP_BDD\n");
      break;
    case DDDMP_ADD:
      fprintf (stdout, "DD TYPE: DDDMP_ADD\n");
      break;
    case DDDMP_CNF:
      fprintf (stdout, "DD TYPE: DDDMP_CNF\n");
      break;
    case DDDMP_NONE:
      fprintf (stdout, "DD TYPE: NONE - Error\n");
      break;
  }

  fprintf (stdout, "Number of variables: %d\n", varInfo->nVars);
  fprintf (stdout, "Number of support variables: %d\n", varInfo->nSuppVars);

  if (varInfo->suppVarNames != NULL) {
    fprintf (stdout, "suppVarNames: ");
    for (i=0; i<varInfo->nSuppVars; i++) {
      if (varInfo->suppVarNames[i] != NULL) {
        fprintf (stdout, "%s ", varInfo->suppVarNames[i]);
      }
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->orderedVarNames != NULL) {
    fprintf (stdout, "orderedVarNames: ");
    for (i=0; i<varInfo->nVars; i++) {
      if (varInfo->orderedVarNames[i] != NULL) {
        fprintf (stdout, "%s ", varInfo->orderedVarNames[i]);
      }
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->varIds != NULL) {
    fprintf (stdout, "varIds: ");
    for (i=0; i<varInfo->nSuppVars; i++) {
      fprintf (stdout, "%d ", varInfo->varIds[i]);
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->varIdsAll != NULL) {
    fprintf (stdout, "varIds for ALL Manager Variables: ");
    for (i=0; i<varInfo->nVars; i++) {
      fprintf (stdout, "%d ", varInfo->varIdsAll[i]);
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->varComposeIds != NULL) {
    fprintf (stdout, "varComposeIds: ");
    for (i=0; i<varInfo->nSuppVars; i++) {
      fprintf (stdout, "%d ", varInfo->varComposeIds[i]);
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->varComposeIdsAll != NULL) {
    fprintf (stdout, "varComposeIds for ALL Manager Variables: ");
    for (i=0; i<varInfo->nVars; i++) {
      fprintf (stdout, "%d ", varInfo->varComposeIdsAll[i]);
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->varAuxIds != NULL) {
    fprintf (stdout, "varAuxIds: ");
    for (i=0; i<varInfo->nSuppVars; i++) {
      fprintf (stdout, "%d ", varInfo->varAuxIds[i]);
    }
    fprintf (stdout, "\n");
  }

  if (varInfo->varAuxIdsAll != NULL) {
    fprintf (stdout, "varAuxIds for ALL Manager Variables: ");
    for (i=0; i<varInfo->nVars; i++) {
      fprintf (stdout, "%d ", varInfo->varAuxIdsAll[i]);
    }
    fprintf (stdout, "\n");
  }

  fprintf (stdout, "Number of roots: %d\n", varInfo->nRoots);

  fflush (stdout);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************
  
  Synopsis     [Print the Help messages.] 
  
  Description  [Print the Help messages.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
Help(
  void
  )
{
  fprintf (stdout, "Commands:\n");
  fprintf (stdout, "MAIN\n");

  fprintf (stdout, "\thelp : Print this set of messages.\n");
  fprintf (stdout, "\tquit : Quit the test program.\n");

  fprintf (stdout, "MANAGER OPERATIONs\n");

  fprintf (stdout,
    "\thmi  : Manager Init (To do BEFORE any BDD/ADD operation).\n");
  fprintf (stdout, "\thmq  : Manager Quit.\n");

  fprintf (stdout, "LOAD\n");

  fprintf (stdout, "\thlb  : Load the header from a BDD/ADD file.\n");
  fprintf (stdout, "\thlc  : Load the header from a CNF file.\n");
  fprintf (stdout, "\tbl   : Load a BDD from a file.\n");
  fprintf (stdout, "\tbal  : Load an Array-BDD from a file.\n");
  fprintf (stdout, "\tal   : Load an ADD from a file.\n");
  fprintf (stdout, "\taal  : Load an Array-ADD from a file.\n");
  fprintf (stdout, "\tcl   : Load a CNF Formula from a file.\n");
  fprintf (stdout, "\tcal  : Load an Array of CNF Formulas from a file.\n");

  fprintf (stdout, "STORE\n");

  fprintf (stdout,
    "\thw   : (Header) Write variable information on stdout.\n");
  fprintf (stdout, "\tbs   : Store a BDD into a file.\n");
  fprintf (stdout, "\tbas  : Store an Array-BDD from a file.\n");
  fprintf (stdout, "\tas   : Store an ADD into a file.\n");
  fprintf (stdout, "\taas  : Store an Array-ADD into a file.\n");
  fprintf (stdout, "\tcs   : Store BDD as a CNF formula.\n");
  fprintf (stdout, "\tcas  : Store and Array of BDDs as a CNF formula.\n");

  fprintf (stdout, "MISC\n");

  fprintf (stdout, "\tdr   : Activate Dynamic Reordering.\n");
  fprintf (stdout, "\tonl  : Load the order from a file (varNames).\n");
  fprintf (stdout, "\toil  : Load the order from a file (varAuxIds).\n");
  fprintf (stdout, "\tcil  : Load compose IDs from a file.\n");
  fprintf (stdout, "\tslm  : Set Load matchmode for variables.\n");
  fprintf (stdout,
    "\top   : Operation (or, and, xor, not, =) between BDDs.\n");
  fprintf (stdout, "\toc   : Create a terminal-one BDD.\n");
  fprintf (stdout, "\tzc   : Create a terminal-zero BDD.\n");
  fprintf (stdout, "\tlc   : Create a single variable BDD (1 node).\n");
  fprintf (stdout, "\tbc   : Create a random BDD.\n");

  fprintf (stdout, "NOT YET IMPLEMENTED\n");

  fprintf (stdout,
    "\ta2b  : Convert a file from the ASCII format to the binary one.\n");
  fprintf (stdout,
    "\tb2a  : Convert a file from the binary format to the ASCII one.\n");

  fprintf (stdout, "HINT\n");

  fprintf (stdout,
    "  Command 'mi' has to be the first instruction to build:\n");
  fprintf (stdout, "  a) The CUDD manager.\n");
  fprintf (stdout,
    "  b) The internal manager (containing name and variable IDs).\n");
  fprintf (stdout,
    "  After that load an header file with 'hlb' or 'hlc' to have proper\n");
  fprintf (stdout,
    "  names and ids for each subsequent load/store operation.\n");

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load the BDD order from a file (varNames).] 
  
  Description  [Load the BDD order from a file (varNames).
    Force the orderedVarNames field of the varInfo structure,
    i.e., the local manager, to be stucked to this array of values.
    ]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
OrderNamesLoad(
  dddmpVarInfo_t *varInfo      /* IN/OUT: Variable Information */
  )
{
  FILE *fp = NULL;
  int i;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];
  char tmpBuf[DDDMPTEST_MAX_STRING_LENGTH];
  char tmpName[DDDMPTEST_MAX_STRING_LENGTH];
  char **tmpOrderedVarNames = NULL;

  /*-------------------------  Red New Var Names Array ----------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);

  fp = fopen (fileName, "r");
  Dddmp_CheckAndReturn (fp==NULL, "Cannot open file.");

  varoutinfo = DDDMP_VARNAMES;
  tmpOrderedVarNames = DDDMP_ALLOC (char *, varInfo->nDdVars);

  i=0;
  while (fgets (tmpBuf, DDDMPTEST_MAX_STRING_LENGTH, fp)!=NULL) {
    if (tmpBuf[0]=='#') {
      continue;
    }

    if (i>=varInfo->nDdVars) {
      fprintf (stdout,
        "Number of variables in files higher than DD manager vars (%d)\n",
        varInfo->nDdVars);
      fprintf (stdout, "Exceeding variables ignored\n");
      fprintf (stdout,
        "You might increase the DDDMPTEST_MAX_VARIABLE constant\n");
      break;
    }

    sscanf (tmpBuf, "%s", tmpName);
    tmpOrderedVarNames[i] = DDDMP_ALLOC (char, (strlen (tmpName) + 1));
    if (tmpOrderedVarNames[i]==NULL) {
      fprintf (stdout, "Error allocating memory\n");
    } else {
      strcpy (tmpOrderedVarNames[i], tmpName);
    }
    i++;
  }

  for ( ;i<varInfo->nDdVars; i++) {
    tmpOrderedVarNames[i] = NULL;
  }

  fclose(fp);

  /*----------------------- Free and Set Var Names Array --------------------*/

  DddmpStrArrayFree (varInfo->orderedVarNames, varInfo->nVars);
  varInfo->orderedVarNames = tmpOrderedVarNames;
  varInfo->nVars = varInfo->nDdVars;

  /* DO NOT ALLOW FURTHER UPDATES */
  varInfo->varNamesFlagUpdate = 0;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load the BDD order from a file (varauxids).] 
  
  Description  [Load the BDD order from a file (varauxids).
    Force the
    varAuxIds and varAuxIdsAll
    or the
    varComposeIds and varComposeIdsAll
    fields of the varInfo structure, i.e., the local manager, to be
    stucked to this array of values.
    ]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
IntArrayLoad (
  dddmpVarInfo_t *varInfo      /* IN/OUT: Variable Information */,
  const char *mode
  )
{
  FILE *fp = NULL;
  int i;
  int *tmpArray1 = NULL;
  int *tmpArray2 = NULL;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];
  char buf[DDDMPTEST_MAX_STRING_LENGTH];

  ReadString (DDDMP_MESSAGE_FILE, fileName);

  fp = fopen(fileName, "r");
  Dddmp_CheckAndReturn (fp==NULL, "Cannot open file.");

  tmpArray1 = DDDMP_ALLOC (int, varInfo->nDdVars);
  tmpArray2 = DDDMP_ALLOC (int, varInfo->nDdVars);
  Dddmp_CheckAndReturn (tmpArray1==NULL, "Error allocating memory.");
  Dddmp_CheckAndReturn (tmpArray2==NULL, "Error allocating memory.");

  i=0;
  while (fgets(buf, DDDMPTEST_MAX_STRING_LENGTH, fp)!=NULL) {
    if (buf[0]=='#') {
      continue;
    }
    if (i>=varInfo->nDdVars) {
      fprintf (stdout,
        "Number of variables in files higher than DD manager vars (%d)\n",
        varInfo->nDdVars);
      fprintf (stdout, "Exceeding variables ignored.\n");
      fprintf (stdout, "(Increase the DDDMPTEST_MAX_VARIABLE constant.)\n");
      break;
    }
    sscanf(buf, "%d", &tmpArray1[i]);
    sscanf(buf, "%d", &tmpArray2[i++]);
  }

  for (;i<varInfo->nDdVars;i++) {
    tmpArray1[i]= -1;
    tmpArray2[i]= -1;
  }

  fclose(fp);

  if (strcmp (mode, "oil") == 0) {
    varInfo->varAuxIds = tmpArray1;
    varInfo->varAuxIdsAll = tmpArray2;

    /* DO NOT ALLOW FURTHER UPDATES */
    varInfo->varAuxIdsFlagUpdate = 0;
  } else {
    if (strcmp (mode, "cil") == 0) {
      varInfo->varComposeIds = tmpArray1;
      varInfo->varComposeIdsAll = tmpArray2;

      /* DO NOT ALLOW FURTHER UPDATES */
      varInfo->varComposeIdsFlagUpdate = 0;
    }
  }

  varInfo->nVars = varInfo->nDdVars;
  varInfo->nSuppVars = varInfo->nDdVars;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load a BDD from a file.]
  
  Description  [Load a BDD from a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddLoad (
  DdManager *ddMgr         /* IN: CUDD Manager */,
  DdNode **operandBdd      /* IN: Operand BDD */,
  dddmpVarInfo_t *varInfo  /* IN/OUT: Variable Information */
  )
{
  DdNode *f = NULL;
  int i;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD, &i);
  
  /*-------------------------------- Load BDD -------------------------------*/

  fprintf (stdout, "Loading %s ...\n", fileName);

  f = Dddmp_cuddBddLoad (ddMgr, varmatchmode, varInfo->orderedVarNames,
    varInfo->varIdsAll, varInfo->varComposeIdsAll, DDDMP_MODE_DEFAULT,
    fileName, NULL);

  if (f==NULL) {
    fprintf (stderr, "Dddmp Test Error : %s is not loaded from file\n",
      fileName);
  } else {
    operandBdd[i] = f;
  }

  /*---------------------------- Tail Operations ----------------------------*/

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load an array of BDDs from a file.]
  
  Description  [Load an array of BDDs from a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddArrayLoad(
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode ***operandBddArray   /* IN: Array of operand BDD */,
  int *operandBddArraySize    /* IN: Number of ADD in the Array */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode **bddArray = NULL;  
  int i, j, nRoots;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD_ARRAY, &i);

  /*---------------------------- Load BDDs ----------------------------------*/

  nRoots = Dddmp_cuddBddArrayLoad (ddMgr, rootmatchmode,
    varInfo->rootNames, varmatchmode,
    varInfo->orderedVarNames, varInfo->varIdsAll, varInfo->varComposeIdsAll,
    DDDMP_MODE_DEFAULT, fileName, NULL, &bddArray);

  Dddmp_CheckAndReturn (nRoots>DDDMP_MAX_BDDARRAY_LEN,
    "DDDMP_MAX_BDDARRAY_LEN exceeded by BDD array len (increase it).");

  if (nRoots<=0) {
    return (DDDMP_FAILURE);
  }

  varInfo->nRoots = nRoots;
  operandBddArray[i] = DDDMP_ALLOC (DdNode *, nRoots);
  Dddmp_CheckAndReturn (operandBddArray[i]==NULL, "Allocation error.");

  for (j=0; j<nRoots; j++) {
    operandBddArray[i][j] = bddArray[j];
  }
  operandBddArraySize[i] = nRoots;

  /*---------------------------- Tail Operations ----------------------------*/

  /* free array */
  DDDMP_FREE (bddArray);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load an ADD from a file.]
  
  Description  [Load an ADD from a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
AddLoad(
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode **operandBdd         /* IN: Operand BDD */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode *f = NULL;
  int i;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD, &i);

  /*-------------------------------- Load ADD -------------------------------*/

  fprintf (stdout, "Loading %s ...\n", fileName);

  f = Dddmp_cuddAddLoad (ddMgr, varmatchmode, varInfo->orderedVarNames,
    varInfo->varIdsAll, varInfo->varComposeIdsAll, DDDMP_MODE_DEFAULT,
    fileName, NULL);

  if (f==NULL) {
    fprintf (stderr, "Dddmp Test Error : %s is not loaded from file\n",
      fileName);
  } else {
    operandBdd[i] = f;
  }

  /*---------------------------- Tail Operations ----------------------------*/

  fprintf (stdout, "Load:\n");
  Cudd_PrintMinterm (ddMgr, f);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load an array of ADDs from a file.]
  
  Description  [Load an array of ADDs from a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
AddArrayLoad(
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode ***operandBddArray   /* IN: Array of operand BDD */,
  int *operandBddArraySize    /* IN: Number of ADD in the Array */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];
  int i, j, nRoots;
  DdNode **bddArray = NULL;

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD_ARRAY, &i);

  /*------------------------------- Load ADDs -------------------------------*/

  nRoots = Dddmp_cuddAddArrayLoad (ddMgr, rootmatchmode,
    varInfo->rootNames, varmatchmode,
    varInfo->orderedVarNames, varInfo->varIdsAll, varInfo->varComposeIdsAll,
    DDDMP_MODE_DEFAULT, fileName, NULL, &bddArray);

  Dddmp_CheckAndReturn (nRoots>DDDMP_MAX_BDDARRAY_LEN,
    "DDDMP_MAX_BDDARRAY_LEN exceeded by BDD array len (increase it).");

  if (nRoots<=0) {
    return (DDDMP_FAILURE);
  }

  varInfo->nRoots = nRoots;
  operandBddArray[i] = DDDMP_ALLOC (DdNode *, nRoots);
  Dddmp_CheckAndReturn (operandBddArray[i]==NULL, "Allocation error.");

  for (j=0; j<nRoots; j++) {
    operandBddArray[i][j] = bddArray[j];
  }
  operandBddArraySize[i] = nRoots;

  /*---------------------------- Tail Operations ----------------------------*/

  /* Free array */
  DDDMP_FREE (bddArray);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Load a CNF formula from a file, and create a BDD.]
  
  Description  [Load a CNF formula from a file, and create a BDD.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddLoadCnf (
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode **operandBdd         /* IN: Operand BDD */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode **rootsPtr = NULL;
  Dddmp_DecompCnfLoadType loadingMode = DDDMP_CNF_MODE_CONJ_QUANT;
  int i, retValue, nRoots;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD, &i);
  
  /*------------------------------- Load BDDs -------------------------------*/

  fprintf (stdout, "Loading %s ...\n", fileName);

  retValue = Dddmp_cuddBddLoadCnf (ddMgr, varmatchmode,
    varInfo->orderedVarNames, varInfo->varAuxIdsAll, varInfo->varComposeIdsAll,
    loadingMode, fileName, NULL, &rootsPtr, &nRoots);

  Dddmp_CheckAndGotoLabel (retValue==DDDMP_FAILURE,
    "Dddmp Test: Load From File Error.\n", failure);

  operandBdd[i] = rootsPtr[0];

  /*---------------------------- Tail Operations ----------------------------*/

  /* Free array */
  DDDMP_FREE (rootsPtr);

  return (DDDMP_SUCCESS);

  failure:
    return(DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Load a CNF formula from a file, and create an array of
    BDDs.
  ]

  Description  [Load a CNF formula from a file, and create an array of
    BDDs.
  ]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddArrayLoadCnf (
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode ***operandBddArray   /* IN: Array of operand BDD */,
  int *operandBddArraySize    /* IN: Number of ADD in the Array */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode **rootsPtr = NULL;
  Dddmp_DecompCnfLoadType loadingMode = DDDMP_CNF_MODE_CONJ_QUANT;
  int i, j, nRoots, retValue;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD_ARRAY, &i);

  /*--------------------------- Loading BDDs --------------------------------*/

  retValue = Dddmp_cuddBddArrayLoadCnf (ddMgr, rootmatchmode,
    varInfo->rootNames, varmatchmode,
    varInfo->orderedVarNames, varInfo->varIdsAll, varInfo->varComposeIdsAll,
    loadingMode, fileName, NULL, &rootsPtr, &nRoots);

  Dddmp_CheckAndReturn (nRoots>DDDMP_MAX_BDDARRAY_LEN,
    "DDDMP_MAX_BDDARRAY_LEN exceeded by BDD array len (increase it).");

  if (nRoots<=0) {
    return (DDDMP_FAILURE);
  }

  varInfo->nRoots = nRoots;
  operandBddArray[i] = DDDMP_ALLOC (DdNode *, nRoots);
  Dddmp_CheckAndReturn (operandBddArray[i]==NULL, "Allocation error.");

  for (j=0; j<nRoots; j++) {
    operandBddArray[i][j] = rootsPtr[j];
  }
  operandBddArraySize[i] = nRoots;

  /*---------------------------- Tail Operations ----------------------------*/

  /* Free array */
  DDDMP_FREE (rootsPtr);

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Perform an Operation among BDDs.]
  
  Description  [Perform an Operation among BDDs.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
Operation(
  DdManager *ddMgr      /* IN: CUDD Manager */,
  DdNode **operandBdd   /* IN: Array of operandBdd */
  )
{
  DdNode *f, *g, *h;
  char buf[DDDMPTEST_MAX_STRING_LENGTH];
  int i;

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_OP, buf);
  ReadInt (DDDMP_MESSAGE_SOURCE1, &i);

  f = operandBdd[i];

  /*-------------------------- Compute Operation ----------------------------*/

  if ((strcmp(buf, "or")==0)|| (strcmp(buf, "OR")==0)) {
    ReadInt (DDDMP_MESSAGE_SOURCE2, &i);
    g = operandBdd[i];
    h = Cudd_bddOr(ddMgr, f, g);
    Cudd_RecursiveDeref(ddMgr, f);
    Cudd_Ref(h);
    Cudd_RecursiveDeref(ddMgr, g);
  } else if ((strcmp(buf, "and")==0) || (strcmp(buf, "AND")==0)) {
      ReadInt (DDDMP_MESSAGE_SOURCE2, &i);
      g = operandBdd[i];
      h = Cudd_bddAnd(ddMgr, f, g);
      Cudd_Ref(h);
      Cudd_RecursiveDeref(ddMgr, f);
      Cudd_RecursiveDeref(ddMgr, g);
  } else if ((strcmp(buf, "xor")==0) || (strcmp(buf, "XOR")==0)) {
      ReadInt (DDDMP_MESSAGE_SOURCE2, &i);
      g = operandBdd[i];
      h = Cudd_bddXor(ddMgr, f, g);
      Cudd_Ref(h);
      Cudd_RecursiveDeref(ddMgr, f);
      Cudd_RecursiveDeref(ddMgr, g);
  } else if (strcmp(buf, "!")==0) {
      h = Cudd_Not(f);
      Cudd_Ref(h);
      Cudd_RecursiveDeref(ddMgr, f);
  } else if ((strcmp(buf, "buf")==0)|| (strcmp(buf, "BUF")==0)) {
      h = f;
  } else {
      fprintf (stderr, "Dddmp Test Error : Operation %s unknown\n", buf);
      h = NULL;
  }

  ReadInt (DDDMP_MESSAGE_DESTINATION, &i);

  operandBdd[i] = h;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Store a BDD in a file.]
  
  Description  [Store a BDD in a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddStore (
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode **operandBdd         /* IN: Operand BDD */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode *f = NULL;
  int i, retValue;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD, &i);

  fprintf (stdout, "Storing %s ...\n", fileName);
  fflush (stdout);
  f = operandBdd[i];

  /*----------------------------- Store BDDs -------------------------------*/

  retValue = Dddmp_cuddBddStore(ddMgr, NULL, f, varInfo->orderedVarNames,
    varInfo->varAuxIdsAll, DDDMP_MODE_TEXT, varoutinfo, fileName, NULL);

  Dddmp_CheckAndGotoLabel (retValue!=DDDMP_SUCCESS, "BDD NOT stored.",
    failure);

  return (DDDMP_SUCCESS);

  failure:
    return(DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Store an Array of BDD in a file.]
  
  Description  [Store an Array of BDD in a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddArrayStore (
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode ***operandBddArray   /* IN: Array of operand BDD */,
  int *operandBddArraySize    /* IN: Number of ADD in the Array */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  int i, retValue, nRoots;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD_ARRAY, &i);

  nRoots = operandBddArraySize[i];

  /*----------------------------- Store BDDs -------------------------------*/

  fprintf (stdout, "Storing Array of BDDs in file %s ...\n", fileName);
  fflush (stdout);

  retValue = Dddmp_cuddBddArrayStore (ddMgr, NULL, nRoots, operandBddArray[i],
    NULL, varInfo->orderedVarNames, varInfo->varAuxIdsAll, DDDMP_MODE_TEXT,
    DDDMP_VARIDS, fileName, NULL);

  Dddmp_CheckAndGotoLabel (retValue!=DDDMP_SUCCESS, "BDD NOT stored.",
    failure);
  fprintf (stdout, "done.\n");

  return (DDDMP_SUCCESS);

  failure:
    return(DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Store an ADD in a file.]
  
  Description  [Store an ADD in a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
AddStore(
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode **operandBdd         /* IN: operand Bdd */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode *f;
  int i, retValue;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD, &i);

  fprintf (stdout, "Storing %s ...\n", fileName);
  fflush (stdout);
  f = operandBdd[i];

#if 0
  /* StQ Patch - CREATE temporary ADD to Store */
  f = Cudd_addResidue (ddMgr, 4, 3, 1, 1);
  fprintf (stderr, "Store:\n");
  Cudd_PrintMinterm (ddMgr, f);
  /* end ... StQ Patch */
#endif

  retValue = Dddmp_cuddAddStore (ddMgr, NULL, f, varInfo->orderedVarNames,
    varInfo->varAuxIdsAll, DDDMP_MODE_TEXT, varoutinfo, fileName, NULL);

  Dddmp_CheckAndReturn (retValue!=DDDMP_SUCCESS, "BDD NOT stored.");

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Store a BDD in a file.]
  
  Description  [Store a BDD in a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
AddArrayStore (
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode ***operandBddArray   /* IN: Array of operand ADD */,
  int *operandBddArraySize    /* IN: Number of ADD in the Array */,
  dddmpVarInfo_t *varInfo
  )
{
  int i, retValue, nRoots;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD_ARRAY, &i);

  nRoots = operandBddArraySize[i];

  fprintf (stdout, "Storing Array of BDDs in file %s ...\n", fileName);
  fflush (stdout);

  retValue = Dddmp_cuddAddArrayStore (ddMgr, NULL, nRoots, operandBddArray[i],
    NULL, varInfo->orderedVarNames, varInfo->varAuxIdsAll, DDDMP_MODE_TEXT,
    DDDMP_VARIDS, fileName, NULL);

  Dddmp_CheckAndReturn (retValue!=DDDMP_SUCCESS, "BDD NOT stored.");

  fprintf (stdout, "done.\n");
  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Store a BDD as CNF format in a file.]
  
  Description  [Store a BDD as CNF format in a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddStoreCnf(
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode **operandBdd         /* IN: Array of operand ADD */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  DdNode *f = NULL;
  Dddmp_DecompCnfStoreType storingMode = DDDMP_CNF_MODE_BEST;
  int noHeader = 0;
  int i, nVars, retValue, idInitial, varNewN, clauseNewN;
  int edgeInTh = (-1);
  int pathLengthTh = (-1);
  int *tmpBddIds = NULL;
  int *tmpCnfIds = NULL;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];
  char row[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD, &i);
  ReadString (DDDMP_MESSAGE_FORMAT, row);

  switch (row[0]) {
    case 'N':
      storingMode = DDDMP_CNF_MODE_NODE;
      break;
    case 'M':
      storingMode = DDDMP_CNF_MODE_MAXTERM;
      break;
    case 'B':
      storingMode = DDDMP_CNF_MODE_BEST;
      ReadInt (DDDMP_MESSAGE_EDGE_MAX, &edgeInTh);
      ReadInt (DDDMP_MESSAGE_LENGHT_MAX, &pathLengthTh);
      break;
  }
  ReadInt (DDDMP_MESSAGE_I_ID, &idInitial);

  fprintf (stdout, "Storing %s ...\n", fileName);
  fflush (stdout);

  f = operandBdd[i];

  nVars = varInfo->nDdVars;

  /*------------ From BDD and CNF ids to Proper Array of ids ----------------*/

  tmpBddIds = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndGotoLabel (tmpBddIds==NULL, "Error allocating memory.",
    failure);
  tmpCnfIds = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndGotoLabel (tmpBddIds==NULL, "Error allocating memory.",
    failure);

  for (i=0; i<nVars; i++) {
    tmpBddIds[i] = i;
    tmpCnfIds[i] = i+1;
  }

  retValue = Dddmp_cuddBddStoreCnf (ddMgr, f, storingMode, noHeader,
    varInfo->orderedVarNames, tmpBddIds, NULL, tmpCnfIds, idInitial,
    edgeInTh, pathLengthTh, fileName, NULL, &clauseNewN, &varNewN);

  Dddmp_CheckAndGotoLabel (retValue!=DDDMP_SUCCESS, "BDD NOT stored.",
    failure);

  fprintf (stdout, "Number of Clauses Stored = %d\n", clauseNewN);
  fprintf (stdout, "Number of New Variable Created Storing = %d\n",
    varNewN);
  fflush (stdout);

  DDDMP_FREE (tmpBddIds);
  DDDMP_FREE (tmpCnfIds);

  return (DDDMP_SUCCESS);

  failure:
    DDDMP_FREE (tmpBddIds);
    DDDMP_FREE (tmpCnfIds);

    return(DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Store a BDD as CNF format in a file.]
  
  Description  [Store a BDD as CNF format in a file.]
  
  SideEffects  []
  
  SeeAlso      []

******************************************************************************/

static int
BddArrayStoreCnf(
  DdManager *ddMgr            /* IN: CUDD Manager */,
  DdNode ***operandBddArray   /* IN: Array of operand ADD */,
  int *operandBddArraySize    /* IN: Number of ADD in the Array */,
  dddmpVarInfo_t *varInfo     /* IN/OUT: Variable Information */
  )
{
  Dddmp_DecompCnfStoreType storingMode = DDDMP_CNF_MODE_BEST;
  int noHeader = 0;
  int i, nVars, bddN, retValue, idInitial, varNewN, clauseNewN;
  int edgeInTh = (-1);
  int pathLengthTh = (-1);
  int *tmpBddIds = NULL;
  int *tmpCnfIds = NULL;
  char fileName[DDDMPTEST_MAX_FILENAME_LENGTH];
  char row[DDDMPTEST_MAX_FILENAME_LENGTH];

  /*------------------------ Read Operation Operands ------------------------*/

  ReadString (DDDMP_MESSAGE_FILE, fileName);
  ReadInt (DDDMP_MESSAGE_BDD_ARRAY, &bddN);
  ReadString (DDDMP_MESSAGE_FORMAT, row);
  switch (row[0]) {
    case 'N':
      storingMode = DDDMP_CNF_MODE_NODE;
      break;
    case 'M':
      storingMode = DDDMP_CNF_MODE_MAXTERM;
      break;
    case 'B':
      storingMode = DDDMP_CNF_MODE_BEST;
      ReadInt (DDDMP_MESSAGE_EDGE_MAX, &edgeInTh);
      ReadInt (DDDMP_MESSAGE_LENGHT_MAX, &pathLengthTh);
      break;
  }
  ReadInt (DDDMP_MESSAGE_I_ID, &idInitial);

  nVars = varInfo->nDdVars;

  /*------------ From BDD and CNF ids to Proper Array of ids ----------------*/

  tmpBddIds = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndReturn (tmpBddIds==NULL, "Allocation error.");
  tmpCnfIds = DDDMP_ALLOC (int, nVars);
  Dddmp_CheckAndReturn (tmpCnfIds==NULL, "Allocation error.");

  for (i=0; i<nVars; i++) {
    tmpBddIds[i] = i;
    tmpCnfIds[i] = i*10+1;
  }

  fprintf (stdout, "Storing %s ...\n", fileName);
  fflush (stdout);

  retValue = Dddmp_cuddBddArrayStoreCnf (ddMgr, operandBddArray[bddN],
    operandBddArraySize[bddN], storingMode, noHeader, varInfo->orderedVarNames,
    tmpBddIds, NULL, tmpCnfIds, idInitial, edgeInTh, pathLengthTh, fileName,
    NULL, &varNewN, &clauseNewN);

  Dddmp_CheckAndGotoLabel (retValue!=DDDMP_SUCCESS, "BDD NOT stored.",
    failure);

  fprintf (stdout, "Number of Clauses Stored = %d\n", clauseNewN);
  fprintf (stdout, "Number of New Variable Created Storing = %d\n",
    varNewN);
  fflush (stdout);

  DDDMP_FREE (tmpBddIds);
  DDDMP_FREE (tmpCnfIds);

  return (DDDMP_SUCCESS);

  failure:
    DDDMP_FREE (tmpBddIds);
    DDDMP_FREE (tmpCnfIds);

    return(DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Dynamic Reordering BDDs.]

  Description  [Dynamic Reordering BDDs using one of the allowed CUDD
    methods.]

  SideEffects  []

  SeeAlso      []

******************************************************************************/

static int
DynamicReordering (
  DdManager *ddMgr            /* IN: CUDD Manager */
  )
{
  Cudd_ReorderingType approach = CUDD_REORDER_SIFT;
  int method;

  /*------------------------ Read Operation Operands ------------------------*/

  ReadInt (DDDMP_MESSAGE_REORDERING, &method);
  approach = (Cudd_ReorderingType) method;

  Cudd_ReduceHeap (ddMgr, approach, 5);
  
  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Selects variable matching mode.]

  Description  [Selects variable matching mode.]

  SideEffects  []

  SeeAlso      []

******************************************************************************/

static int
SetLoadMatchmode (
  )
{
  int sel;
  char row[DDDMPTEST_MAX_FILENAME_LENGTH];

  fprintf (stdout, "Variable matchmode:\n");
  fprintf (stdout, "Match IDs                                (1)\n");
  fprintf (stdout, "Match permIDs                            (2)\n");
  fprintf (stdout, "Match names      (must have been loaded) (3)\n");
  fprintf (stdout, "Match auxids     (must have been loaded) (4)\n");
  fprintf (stdout, "Match composeids (must have been loaded) (5)\n");
  fprintf (stdout, "Your choice: ");
  fflush (stdout);

  fgets (row, DDDMPTEST_MAX_STRING_LENGTH, stdin);
  sscanf (row, "%d", &sel);

  switch (sel) {
    case 1:
      varmatchmode = DDDMP_VAR_MATCHIDS;
      break;
    case 2:
      varmatchmode = DDDMP_VAR_MATCHPERMIDS;
      break;
    case 3:
      varmatchmode = DDDMP_VAR_MATCHNAMES;
      break;
    case 4:
      varmatchmode = DDDMP_VAR_MATCHAUXIDS;
      break;
    case 5:
      varmatchmode = DDDMP_VAR_COMPOSEIDS;
      break;
    default:
      fprintf (stderr, "Wrong choice!\n");
      break;
  }

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Complete the internal manager structure for subsequent
    BDD/ADD/CNF operations.
  ]

  Description  [Complete the internal manager structure for subsequent
    BDD/ADD/CNF operations.
    The phylosophy is simple: to have proper names and ids it is necessary
    to load an header before each actual load/store operation.
    An header load should initialize variable names, variable ids,
    variable compose ids, and variable auxiliary ids for all variables
    stored in the file.
    This information has to be extended for all variables in the
    *current* CUDD manager (before any store operation).
    CompleteInfoStruct does this job.
    Arrays varIds, varComposeIds, and varAuxIds contain information for
    all the variable in the BDD/ADD/CNF while arrays varIdsAll,
    varComposeIdsAll, and varAuxIdsAll contain information for *all*
    variable in the current CUDD manager.
  ]


  SideEffects  []

  SeeAlso      []

******************************************************************************/

static int
CompleteInfoStruct (
  Dddmp_DecompType ddType  /* IN: selects the proper decomp type */,
  int nVars                /* IN: number of DD variables */,
  int nSuppVars            /* IN: number of support variables */,
  char **suppVarNames      /* IN: array of support variable names */,
  char **orderedVarNames   /* IN: array of variable names */,
  int *varIds              /* IN: array of variable ids */,
  int *varComposeIds       /* IN: array of permids ids */,
  int *varAuxIds           /* IN: array of variable aux ids */,
  int nRoots               /* IN: number of root in the file */,
  dddmpVarInfo_t *varInfo  /* IN: Variable Information */
  )
{
  int i;
  char tmpString[DDDMPTEST_MAX_STRING_LENGTH];

  /*------------------------- Updates Variable Names ------------------------*/

  DddmpStrArrayFree (varInfo->suppVarNames, varInfo->nSuppVars);
  varInfo->suppVarNames = suppVarNames;

  if (varInfo->varNamesFlagUpdate == 1) {

    DddmpStrArrayFree (varInfo->orderedVarNames, varInfo->nVars);

    if (orderedVarNames != NULL) {
      varInfo->orderedVarNames = orderedVarNames;
    } else {
      varInfo->orderedVarNames = DDDMP_ALLOC (char *, nVars);
      Dddmp_CheckAndReturn (varInfo->orderedVarNames==NULL,
        "Allocation error.");

      for (i=0; i<nVars; i++) {
        varInfo->orderedVarNames[i] = NULL;
      }

      if (varInfo->suppVarNames != NULL) {
        for (i=0; i<nSuppVars; i++) {
          varInfo->orderedVarNames[i] = DDDMP_ALLOC (char,
            (strlen (varInfo->suppVarNames[i]) + 1));
          strcpy (varInfo->orderedVarNames[i], varInfo->suppVarNames[i]);
        }
      }
 
      for (i=0; i<nVars; i++) {
        if (varInfo->orderedVarNames[i] == NULL) {
          sprintf (tmpString, "DUMMY%d", i);
          varInfo->orderedVarNames[i] = DDDMP_ALLOC (char,
            (strlen (tmpString) + 1));
          strcpy (varInfo->orderedVarNames[i], tmpString);
        }
      }
    }
  }

  /*------------------------------ Updates IDs ------------------------------*/

  DDDMP_FREE (varInfo->varIds);
  varInfo->varIds = varIds;

  if (varInfo->varIdsFlagUpdate == 1) {

    /* Free Previously Allocated Memory */
    DDDMP_FREE (varInfo->varIdsAll);

    /* Allocate New Memory and Check */
    varInfo->varIdsAll = DDDMP_ALLOC (int, nVars);
    Dddmp_CheckAndReturn (varInfo->varIdsAll==NULL, "Allocation error.");

    /* Set New Values */
    for (i=0; i<nVars; i++) {
      varInfo->varIdsAll[i] = (-1);
    }

    if (varInfo->varIds != NULL) {
      for (i=0; i<nSuppVars; i++) {
        varInfo->varIdsAll[varInfo->varIds[i]] = varInfo->varIds[i];
      }
    }
  }


  /*-------------------------- Updates Compose IDs --------------------------*/

  DDDMP_FREE (varInfo->varComposeIds);
  varInfo->varComposeIds = varComposeIds;

  if (varInfo->varComposeIdsFlagUpdate == 1) {

    /* Free Previously Allocated Memory */
    DDDMP_FREE (varInfo->varComposeIdsAll);

    /* Allocate New Memory and Check */
    varInfo->varComposeIdsAll = DDDMP_ALLOC (int, nVars);
    Dddmp_CheckAndReturn (varInfo->varComposeIdsAll==NULL,
      "Allocation error.");

    /* Set New Values */
    for (i=0; i<nVars; i++) {
      varInfo->varComposeIdsAll[i] = (-1);
    }

    if (varInfo->varComposeIds != NULL) {
      for (i=0; i<nSuppVars; i++) {
        varInfo->varComposeIdsAll[varInfo->varIds[i]] =
          varInfo->varComposeIds[i];
      }
    }
  }

  /*------------------------- Updates Auxiliary IDs -------------------------*/

  DDDMP_FREE (varInfo->varAuxIds);
  varInfo->varAuxIds = varAuxIds;

  if (varInfo->varAuxIdsFlagUpdate == 1) {

    /* Free Previously Allocated Memory */
    DDDMP_FREE (varInfo->varAuxIdsAll);

    /* Allocate New Memory and Check */
    varInfo->varAuxIdsAll = DDDMP_ALLOC (int, nVars);
    Dddmp_CheckAndReturn (varInfo->varAuxIdsAll==NULL, "Allocation error.");

    /* Set New Values */
    for (i=0; i<nVars; i++) {
      varInfo->varAuxIdsAll[i] = (-1);
    }

    if (varInfo->varAuxIds != NULL) {
      for (i=0; i<nSuppVars; i++) {
        varInfo->varAuxIdsAll[varInfo->varIds[i]] = varInfo->varAuxIds[i];
      }
    }
  }

  /*----------------------------- Updates Sizes -----------------------------*/

  varInfo->ddType = ddType;
  varInfo->nVars = nVars;
  varInfo->nSuppVars = nSuppVars;
  Dddmp_CheckAndReturn (varInfo->nDdVars<varInfo->nVars,
    "Local Manager with Not Enough Variables.");
  varInfo->nRoots = nRoots;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Reads an integer value from standard input.]

  Description  [Reads an integer value from standard input.]

  SideEffects  []

  SeeAlso      []

******************************************************************************/

static void
ReadInt (
  Dddmp_MessageType message,
  int *i
  )
{
  char row[DDDMPTEST_MAX_FILENAME_LENGTH];

  switch (message) {
    case DDDMP_MESSAGE_MANAGER_VAR:
      fprintf (stdout, "Number of Variables: ");
      break;
    case DDDMP_MESSAGE_BDD:
      fprintf (stdout, "Which BDDs [0..%d]: ",
        DDDMPTEST_MAX_OPERAND-1);
      break;
    case DDDMP_MESSAGE_BDD_ARRAY:
      fprintf (stdout, "Which Array of BDDs [0..%d]: ",
        DDDMPTEST_MAX_OPERAND-1);
      break;
    case DDDMP_MESSAGE_CUBE:
      fprintf (stdout, "How many cubes [1..]: ");
      break;
    case DDDMP_MESSAGE_INDEX:
      fprintf (stdout, "Index: ");
      break;
    case DDDMP_MESSAGE_SOURCE1:
      fprintf (stdout, "Source1 [0..%d]: ", DDDMPTEST_MAX_OPERAND-1);
      break;
    case DDDMP_MESSAGE_SOURCE2:
      fprintf (stdout, "Source2 [0..%d]: ", DDDMPTEST_MAX_OPERAND-1);
      break;
    case DDDMP_MESSAGE_DESTINATION:
      fprintf (stdout, "Destination [0..%d]: ", DDDMPTEST_MAX_OPERAND-1);
      break;
    case DDDMP_MESSAGE_I_ID:
      fprintf (stdout, "Initial ID : ");
      break;
    case DDDMP_MESSAGE_EDGE_MAX:
      fprintf (stdout,
        "Max Number of Edges (Insert cut-point from there on) : ");
      break;
     case DDDMP_MESSAGE_LENGHT_MAX:
      fprintf (stdout,
        "Max BDD-Path Length (Insert cut-point from there on) : ");
      break;
    case DDDMP_MESSAGE_REORDERING:
      fprintf (stdout, "Reordering Approach (1..17): ");
      break;
    default:
      fprintf (stdout, "Input Generic Integer: ");
      break;
  }
  fflush (stdout);

  fgets (row, DDDMPTEST_MAX_STRING_LENGTH, stdin);
  sscanf (row, "%d", i);
  fflush (stdin);

  return;
}


/**Function********************************************************************

  Synopsis     [Reads a string from standard input.]

  Description  [Reads a string from standard input.]

  SideEffects  []

  SeeAlso      []

******************************************************************************/

static void
ReadString (
  Dddmp_MessageType message,
  char string[]
  )
{
  char localString[DDDMPTEST_MAX_STRING_LENGTH];

  switch (message) {
    case DDDMP_MESSAGE_PROMPT:
      fprintf (stdout, "TestDddmp> ");
      break;
    case DDDMP_MESSAGE_FILE:
      fprintf (stdout, "File : ");
      break;
    case DDDMP_MESSAGE_OP:
      fprintf (stdout, "Operation [or,and,xor,!,buf(=)] : ");
      break;
    case DDDMP_MESSAGE_FORMAT:
      fprintf (stdout, "Format (Node=N, Maxterm=M, Best=B) : ");
      break;
    default:
      fprintf (stdout, "Input Generic String : ");
      break;
  }
  fflush (stdout);

  fgets (localString, DDDMPTEST_MAX_STRING_LENGTH, stdin);
  sscanf (localString, "%s", string);
  fflush (stdin);

  return;
}




