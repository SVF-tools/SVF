/**CFile**********************************************************************

  FileName     [dddmpStoreMisc.c]

  PackageName  [dddmp]

  Synopsis     [Functions to write out bdds to file in prefixed
    and in Blif form.]

  Description  [Functions to write out bdds to file. 
    BDDs are represended on file in text format.
    Each node is stored as a multiplexer in a prefix notation format for
    the prefix notation file or in PLA format for the blif file.
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

static int DddmpCuddDdArrayStorePrefix(DdManager *ddMgr, int n, DdNode **f, char **inputNames, char **outputNames, char *modelName, FILE *fp);
static int DddmpCuddDdArrayStorePrefixBody(DdManager *ddMgr, int n, DdNode **f, char **inputNames, char **outputNames, FILE *fp);
static int DddmpCuddDdArrayStorePrefixStep(DdManager * ddMgr, DdNode * f, FILE * fp, st_table * visited, char ** names);
static int DddmpCuddDdArrayStoreBlif(DdManager *ddMgr, int n, DdNode **f, char **inputNames, char **outputNames, char *modelName, FILE *fp);
static int DddmpCuddDdArrayStoreBlifBody(DdManager *ddMgr, int n, DdNode **f, char **inputNames, char **outputNames, FILE *fp);
static int DddmpCuddDdArrayStoreBlifStep(DdManager *ddMgr, DdNode *f, FILE *fp, st_table *visited, char **names);
static int DddmpCuddDdArrayStoreSmv(DdManager *ddMgr, int n, DdNode **f, char **inputNames, char **outputNames, char *modelName, FILE *fp);
static int DddmpCuddDdArrayStoreSmvBody(DdManager *ddMgr, int n, DdNode **f, char **inputNames, char **outputNames, FILE *fp);
static int DddmpCuddDdArrayStoreSmvStep(DdManager * ddMgr, DdNode * f, FILE * fp, st_table * visited, char ** names);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a prefix notation.]

  Description  [Dumps the argument BDD to file.
    Dumping is done through Dddmp_cuddBddArrayStorePrefix.
    A dummy array of 1 BDD root is used for this purpose.
    ]

  SideEffects  []

  SeeAlso      [Dddmp_cuddBddStore]

******************************************************************************/

int
Dddmp_cuddBddStorePrefix (
  DdManager *ddMgr     /* IN: DD Manager */,
  int nRoots           /* IN: Number of BDD roots */,
  DdNode *f            /* IN: BDD root to be stored */,
  char **inputNames    /* IN: Array of variable names */,
  char **outputNames   /* IN: Array of root names */,
  char *modelName      /* IN: Model Name */,
  char *fileName       /* IN: File name */,
  FILE *fp             /* IN: File pointer to the store file */
  )
{
  int retValue;
  DdNode *tmpArray[1];

  tmpArray[0] = f;

  retValue = Dddmp_cuddBddArrayStorePrefix (ddMgr, 1, tmpArray,
    inputNames, outputNames, modelName, fileName, fp);

  return (retValue);
}

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a prefix notation.]

  Description  [Dumps the argument BDD to file.
    Dumping is done through Dddmp_cuddBddArrayStorePrefix.
    A dummy array of 1 BDD root is used for this purpose.
    ]

  SideEffects  []

  SeeAlso      [Dddmp_cuddBddArrayStore]

******************************************************************************/

int
Dddmp_cuddBddArrayStorePrefix (
  DdManager *ddMgr       /* IN: DD Manager */,
  int nroots             /* IN: number of output BDD roots to be stored */,
  DdNode **f             /* IN: array of BDD roots to be stored */,
  char **inputNames      /* IN: array of variable names (or NULL) */,
  char **outputNames     /* IN: array of root names (or NULL) */,
  char *modelName        /* IN: Model Name */,
  char *fname            /* IN: File name */,
  FILE *fp               /* IN: File pointer to the store file */
  )
{
  int retValue;
  int fileToClose = 0;

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  /* 
   *  Check if File needs to be opened in the proper mode.
   */

  if (fp == NULL) {
    fp = fopen (fname, "w");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  retValue = DddmpCuddDdArrayStorePrefix (ddMgr, nroots, f,
    inputNames, outputNames, modelName, fp);

  if (fileToClose) {
    fclose (fp);
  }

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  return (retValue);

  failure:
    return (DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a Blif/Exlif notation.]

  Description  [Dumps the argument BDD to file.
    Dumping is done through Dddmp_cuddBddArrayStoreBlif.
    A dummy array of 1 BDD root is used for this purpose.
    ]

  SideEffects  []

  SeeAlso      [Dddmp_cuddBddStorePrefix]

******************************************************************************/

int
Dddmp_cuddBddStoreBlif (
  DdManager *ddMgr     /* IN: DD Manager */,
  int nRoots           /* IN: Number of BDD roots */,
  DdNode *f            /* IN: BDD root to be stored */,
  char **inputNames    /* IN: Array of variable names */,
  char **outputNames   /* IN: Array of root names */,
  char *modelName      /* IN: Model Name */,
  char *fileName       /* IN: File name */,
  FILE *fp             /* IN: File pointer to the store file */
  )
{
  int retValue;
  DdNode *tmpArray[1];

  tmpArray[0] = f;

  retValue = Dddmp_cuddBddArrayStoreBlif (ddMgr, 1, tmpArray,
    inputNames, outputNames, modelName, fileName, fp);

  return (retValue);
}

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a Blif/Exlif notation.]

  Description  [Dumps the argument BDD to file.
    Dumping is done through Dddmp_cuddBddArrayStoreBLif.
    A dummy array of 1 BDD root is used for this purpose.
    ]

  SideEffects  []

  SeeAlso      [Dddmp_cuddBddArrayStorePrefix]

******************************************************************************/

int
Dddmp_cuddBddArrayStoreBlif (
  DdManager *ddMgr       /* IN: DD Manager */,
  int nroots             /* IN: number of output BDD roots to be stored */,
  DdNode **f             /* IN: array of BDD roots to be stored */,
  char **inputNames      /* IN: array of variable names (or NULL) */,
  char **outputNames     /* IN: array of root names (or NULL) */,
  char *modelName        /* IN: Model Name */,
  char *fname            /* IN: File name */,
  FILE *fp               /* IN: File pointer to the store file */
  )
{
  int retValue;
  int fileToClose = 0;

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  /* 
   *  Check if File needs to be opened in the proper mode.
   */

  if (fp == NULL) {
    fp = fopen (fname, "w");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  retValue = DddmpCuddDdArrayStoreBlif (ddMgr, nroots, f,
    inputNames, outputNames, modelName, fp);

  if (fileToClose) {
    fclose (fp);
  }

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  return (retValue);

  failure:
    return (DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a prefix notation.]

  Description  [Dumps the argument BDD to file.
    Dumping is done through Dddmp_cuddBddArrayStorePrefix.
    A dummy array of 1 BDD root is used for this purpose.
    ]

  SideEffects  []

  SeeAlso      [Dddmp_cuddBddStore]

******************************************************************************/

int
Dddmp_cuddBddStoreSmv (
  DdManager *ddMgr     /* IN: DD Manager */,
  int nRoots           /* IN: Number of BDD roots */,
  DdNode *f            /* IN: BDD root to be stored */,
  char **inputNames    /* IN: Array of variable names */,
  char **outputNames   /* IN: Array of root names */,
  char *modelName      /* IN: Model Name */,
  char *fileName       /* IN: File name */,
  FILE *fp             /* IN: File pointer to the store file */
  )
{
  int retValue;
  DdNode *tmpArray[1];

  tmpArray[0] = f;

  retValue = Dddmp_cuddBddArrayStoreSmv (ddMgr, 1, tmpArray,
    inputNames, outputNames, modelName, fileName, fp);

  return (retValue);
}

/**Function********************************************************************

  Synopsis     [Writes a dump file representing the argument BDD in
    a prefix notation.]

  Description  [Dumps the argument BDD to file.
    Dumping is done through Dddmp_cuddBddArrayStorePrefix.
    A dummy array of 1 BDD root is used for this purpose.
    ]

  SideEffects  []

  SeeAlso      [Dddmp_cuddBddArrayStore]

******************************************************************************/

int
Dddmp_cuddBddArrayStoreSmv (
  DdManager *ddMgr       /* IN: DD Manager */,
  int nroots             /* IN: number of output BDD roots to be stored */,
  DdNode **f             /* IN: array of BDD roots to be stored */,
  char **inputNames      /* IN: array of variable names (or NULL) */,
  char **outputNames     /* IN: array of root names (or NULL) */,
  char *modelName        /* IN: Model Name */,
  char *fname            /* IN: File name */,
  FILE *fp               /* IN: File pointer to the store file */
  )
{
  int retValue;
  int fileToClose = 0;

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  /* 
   *  Check if File needs to be opened in the proper mode.
   */

  if (fp == NULL) {
    fp = fopen (fname, "w");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  retValue = DddmpCuddDdArrayStoreSmv (ddMgr, nroots, f,
    inputNames, outputNames, modelName, fp);

  if (fileToClose) {
    fclose (fp);
  }

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Store.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Store.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  return (retValue);

  failure:
    return (DDDMP_FAILURE);
}

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Internal function to writes a dump file representing the
    argument BDD in a prefix notation.]

  Description  [One multiplexer is written for each BDD node.
    It returns 1 in case of success; 0 otherwise (e.g., out-of-memory, file
    system full, or an ADD with constants different from 0 and 1).
    It does not close the file: This is the caller responsibility.
    It uses a minimal unique subset of the hexadecimal address of a node as
    name for it.
    If the argument inputNames is non-null, it is assumed to hold the
    pointers to the names of the inputs. Similarly for outputNames.
    For each BDD node of function f, variable v, then child T, and else
    child E it stores:
    f = v * T + v' * E
    that is
    (OR f (AND v T) (AND (NOT v) E))
    If E is a complemented child this results in the following
    (OR f (AND v T) (AND (NOT v) (NOT E)))
    Comments (COMMENT) are added at the beginning of the description to
    describe inputs and outputs of the design.
    A buffer (BUF) is add on the output to cope with complemented functions.
    ]

  SideEffects [None]

  SeeAlso     [DddmpCuddDdArrayStoreBlif]

******************************************************************************/

static int
DddmpCuddDdArrayStorePrefix (
  DdManager *ddMgr    /* IN: Manager */,
  int n               /* IN: Number of output nodes to be dumped */,
  DdNode **f          /* IN: Array of output nodes to be dumped */,
  char **inputNames   /* IN: Array of input names (or NULL) */,
  char **outputNames  /* IN: Array of output names (or NULL) */,
  char *modelName     /* IN: Model name (or NULL) */,
  FILE *fp            /* IN: Pointer to the dump file */
  )
{
  DdNode *support = NULL;
  DdNode *scan;
  int *sorted = NULL;
  int nVars = ddMgr->size;
  int retValue;
  int i;

  /* Build a bit array with the support of f. */
  sorted = ALLOC(int, nVars);
  if (sorted == NULL) {
    ddMgr->errorCode = CUDD_MEMORY_OUT;
    Dddmp_CheckAndGotoLabel (1, "Allocation Error.", failure);
  }
  for (i = 0; i < nVars; i++) {
    sorted[i] = 0;
  }

  /* Take the union of the supports of each output function. */
  support = Cudd_VectorSupport(ddMgr,f,n);
  Dddmp_CheckAndGotoLabel (support==NULL,
    "Error in function Cudd_VectorSupport.", failure);
  cuddRef(support);
  scan = support;
  while (!cuddIsConstant(scan)) {
    sorted[scan->index] = 1;
    scan = cuddT(scan);
  }
  Cudd_RecursiveDeref(ddMgr,support);
  /* so that we do not try to free it in case of failure */
  support = NULL;

  /* Write the header (.model .inputs .outputs). */
  if (modelName == NULL) {
    retValue = fprintf (fp, "(COMMENT - model name: Unknown )\n");
  } else {
    retValue = fprintf (fp, "(COMMENT - model name: %s )\n", modelName);
  }
  if (retValue == EOF) {
    return(0);
  }

  retValue = fprintf(fp, "(COMMENT - input names: ");
  if (retValue == EOF) {
    return(0);
  }
  /* Write the input list by scanning the support array. */
  for (i = 0; i < nVars; i++) {
    if (sorted[i]) {
      if (inputNames == NULL) {
	retValue = fprintf(fp," inNode%d", i);
      } else {
	retValue = fprintf(fp," %s", inputNames[i]);
      }
      Dddmp_CheckAndGotoLabel (retValue==EOF,
        "Error during file store.", failure);
    }
  }
  FREE(sorted);
  sorted = NULL;
  retValue = fprintf(fp, " )\n");
  if (retValue == EOF) {
    return(0);
  }

  /* Write the .output line. */
  retValue = fprintf(fp,"(COMMENT - output names: ");
  if (retValue == EOF) {
    return(0);
  }
  for (i = 0; i < n; i++) {
    if (outputNames == NULL) {
      retValue = fprintf (fp," outNode%d", i);
    } else {
      retValue = fprintf (fp," %s", outputNames[i]);
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);
  } 
  retValue = fprintf(fp, " )\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF,
    "Error during file store.", failure);

  retValue = DddmpCuddDdArrayStorePrefixBody (ddMgr, n, f, inputNames,
    outputNames, fp);
  Dddmp_CheckAndGotoLabel (retValue==0,
    "Error in function DddmpCuddDdArrayStorePrefixBody.", failure);

  return(1);

failure:
    if (sorted != NULL) {
      FREE(sorted);
    }
    if (support != NULL) {
      Cudd_RecursiveDeref(ddMgr,support);
    }
    return(0);
}

/**Function********************************************************************

  Synopsis     [Internal function to writes a dump file representing the
    argument BDD in a prefix notation. Writes the body of the file.]

  Description  [One multiplexer is written for each BDD node.
    It returns 1 in case of success; 0 otherwise (e.g., out-of-memory, file
    system full, or an ADD with constants different from 0 and 1).
    It does not close the file: This is the caller responsibility.
    It uses a minimal unique subset of the hexadecimal address of a node as
    name for it.
    If the argument inputNames is non-null, it is assumed to hold the
    pointers to the names of the inputs. Similarly for outputNames.
    For each BDD node of function f, variable v, then child T, and else
    child E it stores:
    f = v * T + v' * E
    that is
    (OR f (AND v T) (AND (NOT v) E))
    If E is a complemented child this results in the following
    (OR f (AND v T) (AND (NOT v) (NOT E)))
    ]

  SideEffects [None]

  SeeAlso     [DddmpCuddDdArrayStoreBlif]

******************************************************************************/

static int
DddmpCuddDdArrayStorePrefixBody (
  DdManager *ddMgr      /* IN: Manager */,
  int n                 /* IN: Number of output nodes to be dumped */,
  DdNode **f            /* IN: Array of output nodes to be dumped */,
  char **inputNames     /* IN: Array of input names (or NULL) */,
  char **outputNames    /* IN: Array of output names (or NULL) */,
  FILE *fp              /* IN: Pointer to the dump file */
  )
{
  st_table *visited = NULL;
  int retValue;
  int i;

  /* Initialize symbol table for visited nodes. */
  visited = st_init_table(st_ptrcmp, st_ptrhash);
  Dddmp_CheckAndGotoLabel (visited==NULL,
    "Error if function st_init_table.", failure);

  /* Call the function that really gets the job done. */
  for (i = 0; i < n; i++) {
    retValue = DddmpCuddDdArrayStorePrefixStep (ddMgr, Cudd_Regular(f[i]),
      fp, visited, inputNames);
    Dddmp_CheckAndGotoLabel (retValue==0,
      "Error if function DddmpCuddDdArrayStorePrefixStep.", failure);
  }

  /* To account for the possible complement on the root,
   ** we put either a buffer or an inverter at the output of
   ** the multiplexer representing the top node.
   */
  for (i=0; i<n; i++) {
    if (outputNames == NULL) {
      retValue = fprintf (fp,  "(BUF outNode%d ", i);
    } else {
      retValue = fprintf (fp,  "(BUF %s ", outputNames[i]);
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);

    if (Cudd_IsComplement(f[i])) {
#if SIZEOF_VOID_P == 8
      retValue = fprintf (fp, "(NOT node%lx))\n",
        (unsigned long) f[i] / (unsigned long) sizeof(DdNode));
#else
      retValue = fprintf (fp, "(NOT node%x))\n",
        (unsigned) f[i] / (unsigned) sizeof(DdNode));
#endif
    } else {
#if SIZEOF_VOID_P == 8
      retValue = fprintf (fp, "node%lx)\n",
        (unsigned long) f[i] / (unsigned long) sizeof(DdNode));
#else
      retValue = fprintf (fp, "node%x)\n",
        (unsigned) f[i] / (unsigned) sizeof(DdNode));
#endif
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);
  }

  st_free_table (visited);

  return(1);

failure:
    if (visited != NULL) st_free_table(visited);
    return(0);

}

/**Function********************************************************************

  Synopsis    [Performs the recursive step of
    DddmpCuddDdArrayStorePrefixBody.]

  Description [Performs the recursive step of
    DddmpCuddDdArrayStorePrefixBody.
    Traverses the BDD f and writes a multiplexer-network description to the
    file pointed by fp.
    For each BDD node of function f, variable v, then child T, and else
    child E it stores:
    f = v * T + v' * E
    that is
    (OR f (AND v T) (AND (NOT v) E))
    If E is a complemented child this results in the following
    (OR f (AND v T) (AND (NOT v) (NOT E)))
    f is assumed to be a regular pointer and the function guarantees this
    assumption in the recursive calls.
    ]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

static int
DddmpCuddDdArrayStorePrefixStep (
  DdManager * ddMgr,
  DdNode * f,
  FILE * fp,
  st_table * visited,
  char ** names
  )
{
  DdNode *T, *E;
  int retValue;

#ifdef DDDMP_DEBUG
  assert(!Cudd_IsComplement(f));
#endif

  /* If already visited, nothing to do. */
  if (st_is_member(visited, (char *) f) == 1) {
    return(1);
  }

  /* Check for abnormal condition that should never happen. */
  if (f == NULL) {
    return(0);
  }

  /* Mark node as visited. */
  if (st_insert(visited, (char *) f, NULL) == ST_OUT_OF_MEM) {
    return(0);
  }

  /* Check for special case: If constant node, generate constant 1. */
  if (f == DD_ONE (ddMgr)) {
#if SIZEOF_VOID_P == 8
    retValue = fprintf (fp,
      "(OR node%lx vss vdd)\n",
      (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
    retValue = fprintf (fp,
      "(OR node%x vss vdd)\n",
      (unsigned) f / (unsigned) sizeof(DdNode));
#endif
    if (retValue == EOF) {
       return(0);
    } else {
       return(1);
    }
  }

  /*
   *  Check whether this is an ADD. We deal with 0-1 ADDs, but not
   *  with the general case.
   */

  if (f == DD_ZERO(ddMgr)) {
#if SIZEOF_VOID_P == 8
    retValue = fprintf (fp,
      "(AND node%lx vss vdd)\n",
       (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
    retValue = fprintf (fp,
      "(AND node%x vss vdd)\n",
      (unsigned) f / (unsigned) sizeof(DdNode));
#endif
   if (retValue == EOF) {
     return(0);
   } else {
     return(1);
   }
  }

  if (cuddIsConstant(f)) {
    return(0);
  }

  /* Recursive calls. */
  T = cuddT(f);
  retValue = DddmpCuddDdArrayStorePrefixStep (ddMgr,T,fp,visited,names);
  if (retValue != 1) {
    return(retValue);
  }
  E = Cudd_Regular(cuddE(f));
  retValue = DddmpCuddDdArrayStorePrefixStep (ddMgr,E,fp,visited,names);
  if (retValue != 1) {
    return(retValue);
  }

  /* Write multiplexer taking complement arc into account. */
#if SIZEOF_VOID_P == 8
  retValue = fprintf (fp, "(OR node%lx (AND ",
    (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
  retValue = fprintf (fp, "(OR node%x (AND ",
    (unsigned) f / (unsigned) sizeof(DdNode));
#endif
  if (retValue == EOF) {
    return(0);
  }

  if (names != NULL) {
    retValue = fprintf(fp, "%s ", names[f->index]);
  } else {
    retValue = fprintf(fp, "inNode%d ", f->index);
  }
  if (retValue == EOF) {
    return(0);
  }

#if SIZEOF_VOID_P == 8
  retValue = fprintf (fp, "node%lx) (AND (NOT ",
    (unsigned long) T / (unsigned long) sizeof(DdNode));
#else
  retValue = fprintf (fp, "node%x) (AND (NOT ",
    (unsigned) T / (unsigned) sizeof(DdNode));
#endif
  if (retValue == EOF) {
    return(0);
  }

  if (names != NULL) {
    retValue = fprintf (fp, "%s", names[f->index]);
  } else {
    retValue = fprintf (fp, "inNode%d", f->index);
  }
  if (retValue == EOF) {
    return(0);
  }

#if SIZEOF_VOID_P == 8
  if (Cudd_IsComplement(cuddE(f))) {
    retValue = fprintf (fp, ") (NOT node%lx)))\n",
      (unsigned long) E / (unsigned long) sizeof(DdNode));
  } else {
    retValue = fprintf (fp, ") node%lx))\n",
      (unsigned long) E / (unsigned long) sizeof(DdNode));
  }
#else
  if (Cudd_IsComplement(cuddE(f))) {
    retValue = fprintf (fp, ") (NOT node%x)))\n",
      (unsigned) E / (unsigned) sizeof(DdNode));
  } else {
    retValue = fprintf (fp, ") node%x))\n",
      (unsigned) E / (unsigned) sizeof(DdNode));
  }
#endif

  if (retValue == EOF) {
    return(0);
  } else {
    return(1);
  }
}

/**Function********************************************************************

  Synopsis    [Writes a blif file representing the argument BDDs.]

  Description [Writes a blif file representing the argument BDDs as a
    network of multiplexers. One multiplexer is written for each BDD
    node. It returns 1 in case of success; 0 otherwise (e.g.,
    out-of-memory, file system full, or an ADD with constants different
    from 0 and 1).
    DddmpCuddDdArrayStoreBlif does not close the file: This is the
    caller responsibility.
    DddmpCuddDdArrayStoreBlif uses a minimal unique subset of
    the hexadecimal address of a node as name for it.  If the argument
    inames is non-null, it is assumed to hold the pointers to the names
    of the inputs. Similarly for outputNames.
    It prefixes the string "NODE" to each nome to have "regular" names
    for each elements.
    ]

  SideEffects [None]

  SeeAlso     [DddmpCuddDdArrayStoreBlifBody,Cudd_DumpBlif]

******************************************************************************/

static int
DddmpCuddDdArrayStoreBlif (
  DdManager *ddMgr    /* IN: Manager */,
  int n               /* IN: Number of output nodes to be dumped */,
  DdNode **f          /* IN: Array of output nodes to be dumped */,
  char **inputNames   /* IN: Array of input names (or NULL) */,
  char **outputNames  /* IN: Array of output names (or NULL) */,
  char *modelName     /* IN: Model name (or NULL) */,
  FILE *fp            /* IN: Pointer to the dump file */
  )
{
  DdNode *support = NULL;
  DdNode *scan;
  int *sorted = NULL;
  int nVars = ddMgr->size;
  int retValue;
  int i;

  /* Build a bit array with the support of f. */
  sorted = ALLOC (int, nVars);
  if (sorted == NULL) {
    ddMgr->errorCode = CUDD_MEMORY_OUT;
    Dddmp_CheckAndGotoLabel (1, "Allocation Error.", failure);
  }
  for (i = 0; i < nVars; i++) {
    sorted[i] = 0;
  }

  /* Take the union of the supports of each output function. */
  support = Cudd_VectorSupport(ddMgr,f,n);
  Dddmp_CheckAndGotoLabel (support==NULL,
    "Error in function Cudd_VectorSupport.", failure);
  cuddRef(support);
  scan = support;
  while (!cuddIsConstant(scan)) {
    sorted[scan->index] = 1;
    scan = cuddT(scan);
  }
  Cudd_RecursiveDeref(ddMgr,support);
  support = NULL;
  /* so that we do not try to free it in case of failure */

  /* Write the header (.model .inputs .outputs). */
  if (modelName == NULL) {
    retValue = fprintf(fp,".model DD\n.inputs");
  } else {
    retValue = fprintf(fp,".model %s\n.inputs", modelName);
  }
  if (retValue == EOF) {
    return(0);
  }

  /* Write the input list by scanning the support array. */
  for (i = 0; i < nVars; i++) {
    if (sorted[i]) {
      if (inputNames == NULL || (inputNames[i] == NULL)) {
	retValue = fprintf(fp," inNode%d", i);
      } else {
	retValue = fprintf(fp," %s", inputNames[i]);
      }
      Dddmp_CheckAndGotoLabel (retValue==EOF,
        "Error during file store.", failure);
    }
  }
  FREE(sorted);
  sorted = NULL;

  /* Write the .output line. */
  retValue = fprintf(fp,"\n.outputs");
  Dddmp_CheckAndGotoLabel (retValue==EOF,
    "Error during file store.", failure);
  for (i = 0; i < n; i++) {
    if (outputNames == NULL || (outputNames[i] == NULL)) {
      retValue = fprintf(fp," outNode%d", i);
    } else {
      retValue = fprintf(fp," %s", outputNames[i]);
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);
  }
  retValue = fprintf(fp,"\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF,
    "Error during file store.", failure);

  retValue = DddmpCuddDdArrayStoreBlifBody(ddMgr, n, f, inputNames,
    outputNames, fp);
  Dddmp_CheckAndGotoLabel (retValue==0,
    "Error if function DddmpCuddDdArrayStoreBlifBody.", failure);

  /* Write trailer and return. */
  retValue = fprintf (fp, ".end\n");
  Dddmp_CheckAndGotoLabel (retValue==EOF,
    "Error during file store.", failure);

  return(1);

failure:
  if (sorted != NULL) {
    FREE(sorted);
  }
  if (support != NULL) {
    Cudd_RecursiveDeref(ddMgr,support);
  }

  return(0);
}


/**Function********************************************************************

  Synopsis    [Writes a blif body representing the argument BDDs.]

  Description [Writes a blif body representing the argument BDDs as a
    network of multiplexers. One multiplexer is written for each BDD
    node. It returns 1 in case of success; 0 otherwise (e.g.,
    out-of-memory, file system full, or an ADD with constants different
    from 0 and 1).
    DddmpCuddDdArrayStoreBlif does not close the file: This is the
    caller responsibility.
    DddmpCuddDdArrayStoreBlif uses a minimal unique subset of
    the hexadecimal address of a node as name for it.  If the argument
    inputNames is non-null, it is assumed to hold the pointers to the names
    of the inputs. Similarly for outputNames. This function prints out only
    .names part.
  ]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

static int
DddmpCuddDdArrayStoreBlifBody (
  DdManager *ddMgr      /* IN: Manager */,
  int n                 /* IN: Number of output nodes to be dumped */,
  DdNode **f            /* IN: Array of output nodes to be dumped */,
  char **inputNames     /* IN: Array of input names (or NULL) */,
  char **outputNames    /* IN: Array of output names (or NULL) */,
  FILE *fp              /* IN: Pointer to the dump file */
  )
{
  st_table *visited = NULL;
  int retValue;
  int i;

  /* Initialize symbol table for visited nodes. */
  visited = st_init_table(st_ptrcmp, st_ptrhash);
  Dddmp_CheckAndGotoLabel (visited==NULL,
    "Error if function st_init_table.", failure);

  /* Call the function that really gets the job done. */
  for (i = 0; i < n; i++) {
    retValue = DddmpCuddDdArrayStoreBlifStep (ddMgr, Cudd_Regular(f[i]),
      fp, visited, inputNames);
    Dddmp_CheckAndGotoLabel (retValue==0,
      "Error if function DddmpCuddDdArrayStoreBlifStep.", failure);
  }

  /*
   *  To account for the possible complement on the root,
   *  we put either a buffer or an inverter at the output of
   *  the multiplexer representing the top node.
   */

  for (i = 0; i < n; i++) {
    if (outputNames == NULL) {
      retValue = fprintf(fp,
#if SIZEOF_VOID_P == 8
        ".names node%lx outNode%d\n",
        (unsigned long) f[i] / (unsigned long) sizeof(DdNode), i);
#else
	".names node%x outNode%d\n",
        (unsigned) f[i] / (unsigned) sizeof(DdNode), i);
#endif
    } else {
      retValue = fprintf(fp,
#if SIZEOF_VOID_P == 8
        ".names node%lx %s\n",
        (unsigned long) f[i] / (unsigned long) sizeof(DdNode), outputNames[i]);
#else
        ".names node%x %s\n",
        (unsigned) f[i] / (unsigned) sizeof(DdNode), outputNames[i]);
#endif
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);
    if (Cudd_IsComplement(f[i])) {
      retValue = fprintf(fp,"0 1\n");
    } else {
      retValue = fprintf(fp,"1 1\n");
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);
  }

  st_free_table(visited);
  return(1);

failure:
  if (visited != NULL) {
    st_free_table(visited);
  }
  return(0);
}

/**Function********************************************************************

  Synopsis    [Performs the recursive step of DddmpCuddDdArrayStoreBlif.]

  Description [Performs the recursive step of DddmpCuddDdArrayStoreBlif.
    Traverses the BDD f and writes a multiplexer-network description to
    the file pointed by fp in blif format.
    f is assumed to be a regular pointer and DddmpCuddDdArrayStoreBlifStep
    guarantees this assumption in the recursive calls.
    ]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

static int
DddmpCuddDdArrayStoreBlifStep (
  DdManager *ddMgr,
  DdNode *f,
  FILE *fp,
  st_table *visited,
  char **names
  )
{
  DdNode *T, *E;
  int retValue;

#ifdef DDDMP_DEBUG
  assert(!Cudd_IsComplement(f));
#endif

  /* If already visited, nothing to do. */
  if (st_is_member(visited, (char *) f) == 1) {
    return(1);
  }

  /* Check for abnormal condition that should never happen. */
  if (f == NULL) {
    return(0);
  }

  /* Mark node as visited. */
  if (st_insert(visited, (char *) f, NULL) == ST_OUT_OF_MEM) {
    return(0);
  }

  /* Check for special case: If constant node, generate constant 1. */
  if (f == DD_ONE(ddMgr)) {
#if SIZEOF_VOID_P == 8
    retValue = fprintf(fp, ".names node%lx\n1\n",
      (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
    retValue = fprintf(fp, ".names node%x\n1\n",
      (unsigned) f / (unsigned) sizeof(DdNode));
#endif
     if (retValue == EOF) {
       return(0);
     } else {
       return(1);
    }
  }

  /* Check whether this is an ADD. We deal with 0-1 ADDs, but not
  ** with the general case.
  */
  if (f == DD_ZERO(ddMgr)) {
#if SIZEOF_VOID_P == 8
    retValue = fprintf(fp, ".names node%lx\n",
      (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
    retValue = fprintf(fp, ".names node%x\n",
      (unsigned) f / (unsigned) sizeof(DdNode));
#endif
    if (retValue == EOF) {
      return(0);
    } else {
      return(1);
    }
  }
  if (cuddIsConstant(f)) {
    return(0);
  }

  /* Recursive calls. */
  T = cuddT(f);
  retValue = DddmpCuddDdArrayStoreBlifStep(ddMgr,T,fp,visited,names);
  if (retValue != 1) return(retValue);
  E = Cudd_Regular(cuddE(f));
  retValue = DddmpCuddDdArrayStoreBlifStep(ddMgr,E,fp,visited,names);
  if (retValue != 1) return(retValue);

  /* Write multiplexer taking complement arc into account. */
  if (names != NULL) {
    retValue = fprintf(fp,".names %s", names[f->index]);
  } else {
    retValue = fprintf(fp,".names inNode%d", f->index);
  }
  if (retValue == EOF) {
    return(0);
  }

#if SIZEOF_VOID_P == 8
  if (Cudd_IsComplement(cuddE(f))) {
    retValue = fprintf(fp," node%lx node%lx node%lx\n11- 1\n0-0 1\n",
      (unsigned long) T / (unsigned long) sizeof(DdNode),
      (unsigned long) E / (unsigned long) sizeof(DdNode),
      (unsigned long) f / (unsigned long) sizeof(DdNode));
  } else {
    retValue = fprintf(fp," node%lx node%lx node%lx\n11- 1\n0-1 1\n",
      (unsigned long) T / (unsigned long) sizeof(DdNode),
      (unsigned long) E / (unsigned long) sizeof(DdNode),
      (unsigned long) f / (unsigned long) sizeof(DdNode));
  }
#else
  if (Cudd_IsComplement(cuddE(f))) {
    retValue = fprintf(fp," node%x node%x node%x\n11- 1\n0-0 1\n",
      (unsigned) T / (unsigned) sizeof(DdNode),
      (unsigned) E / (unsigned) sizeof(DdNode),
      (unsigned) f / (unsigned) sizeof(DdNode));
  } else {
    retValue = fprintf(fp," node%x node%x node%x\n11- 1\n0-1 1\n",
      (unsigned) T / (unsigned) sizeof(DdNode),
      (unsigned) E / (unsigned) sizeof(DdNode),
      (unsigned) f / (unsigned) sizeof(DdNode));
  }
#endif
  if (retValue == EOF) {
    return(0);
  } else {
    return(1);
  }
}

/**Function********************************************************************

  Synopsis     [Internal function to writes a dump file representing the
    argument BDD in a SMV notation.]

  Description  [One multiplexer is written for each BDD node.
    It returns 1 in case of success; 0 otherwise (e.g., out-of-memory, file
    system full, or an ADD with constants different from 0 and 1).
    It does not close the file: This is the caller responsibility.
    It uses a minimal unique subset of the hexadecimal address of a node as
    name for it.
    If the argument inputNames is non-null, it is assumed to hold the
    pointers to the names of the inputs. Similarly for outputNames.
    For each BDD node of function f, variable v, then child T, and else
    child E it stores:
    f = v * T + v' * E
    that is
    (OR f (AND v T) (AND (NOT v) E))
    If E is a complemented child this results in the following
    (OR f (AND v T) (AND (NOT v) (NOT E)))
    Comments (COMMENT) are added at the beginning of the description to
    describe inputs and outputs of the design.
    A buffer (BUF) is add on the output to cope with complemented functions.
    ]

  SideEffects [None]

  SeeAlso     [DddmpCuddDdArrayStoreBlif]

******************************************************************************/

static int
DddmpCuddDdArrayStoreSmv (
  DdManager *ddMgr    /* IN: Manager */,
  int n               /* IN: Number of output nodes to be dumped */,
  DdNode **f          /* IN: Array of output nodes to be dumped */,
  char **inputNames   /* IN: Array of input names (or NULL) */,
  char **outputNames  /* IN: Array of output names (or NULL) */,
  char *modelName     /* IN: Model name (or NULL) */,
  FILE *fp            /* IN: Pointer to the dump file */
  )
{
  DdNode *support = NULL;
  DdNode *scan;
  int *sorted = NULL;
  int nVars = ddMgr->size;
  int retValue;
  int i;

  /* Build a bit array with the support of f. */
  sorted = ALLOC(int, nVars);
  if (sorted == NULL) {
    ddMgr->errorCode = CUDD_MEMORY_OUT;
    Dddmp_CheckAndGotoLabel (1, "Allocation Error.", failure);
  }
  for (i = 0; i < nVars; i++) {
    sorted[i] = 0;
  }

  /* Take the union of the supports of each output function. */
  support = Cudd_VectorSupport(ddMgr,f,n);
  Dddmp_CheckAndGotoLabel (support==NULL,
    "Error in function Cudd_VectorSupport.", failure);
  cuddRef(support);
  scan = support;
  while (!cuddIsConstant(scan)) {
    sorted[scan->index] = 1;
    scan = cuddT(scan);
  }
  Cudd_RecursiveDeref(ddMgr,support);
  /* so that we do not try to free it in case of failure */
  support = NULL;

  /* Write the header */
  if (modelName == NULL) {
    retValue = fprintf (fp, "MODULE main -- Unknown\n");
  } else {
    retValue = fprintf (fp, "MODULE main -- %s\n", modelName);
  }
  if (retValue == EOF) {
    return(0);
  }

  retValue = fprintf(fp, "IVAR\n");
  if (retValue == EOF) {
    return(0);
  }

  /* Write the input list by scanning the support array. */
  for (i=0; i<nVars; i++) {
    if (sorted[i]) {
      if (inputNames == NULL) {
	retValue = fprintf (fp, " inNode%d : boolean;\n", i);
      } else {
	retValue = fprintf (fp, " %s : boolean;\n", inputNames[i]);
      }
      Dddmp_CheckAndGotoLabel (retValue==EOF,
        "Error during file store.", failure);
    }
  }
  FREE(sorted);
  sorted = NULL;

  retValue = fprintf (fp, "\nDEFINE\n");

  retValue = DddmpCuddDdArrayStoreSmvBody (ddMgr, n, f, inputNames,
    outputNames, fp);
  Dddmp_CheckAndGotoLabel (retValue==0,
    "Error in function DddmpCuddDdArrayStoreSmvBody.", failure);

  return(1);

failure:
    if (sorted != NULL) {
      FREE(sorted);
    }
    if (support != NULL) {
      Cudd_RecursiveDeref(ddMgr,support);
    }
    return(0);
}

/**Function********************************************************************

  Synopsis     [Internal function to writes a dump file representing the
    argument BDD in a SMV notation. Writes the body of the file.]

  Description  [One multiplexer is written for each BDD node.
    It returns 1 in case of success; 0 otherwise (e.g., out-of-memory, file
    system full, or an ADD with constants different from 0 and 1).
    It does not close the file: This is the caller responsibility.
    It uses a minimal unique subset of the hexadecimal address of a node as
    name for it.
    If the argument inputNames is non-null, it is assumed to hold the
    pointers to the names of the inputs. Similarly for outputNames.
    For each BDD node of function f, variable v, then child T, and else
    child E it stores:
    f = v * T + v' * E
    that is
    (OR f (AND v T) (AND (NOT v) E))
    If E is a complemented child this results in the following
    (OR f (AND v T) (AND (NOT v) (NOT E)))
    ]

  SideEffects [None]

  SeeAlso     [DddmpCuddDdArrayStoreBlif]

******************************************************************************/

static int
DddmpCuddDdArrayStoreSmvBody (
  DdManager *ddMgr      /* IN: Manager */,
  int n                 /* IN: Number of output nodes to be dumped */,
  DdNode **f            /* IN: Array of output nodes to be dumped */,
  char **inputNames     /* IN: Array of input names (or NULL) */,
  char **outputNames    /* IN: Array of output names (or NULL) */,
  FILE *fp              /* IN: Pointer to the dump file */
  )
{
  st_table *visited = NULL;
  int retValue;
  int i;

  /* Initialize symbol table for visited nodes. */
  visited = st_init_table(st_ptrcmp, st_ptrhash);
  Dddmp_CheckAndGotoLabel (visited==NULL,
    "Error if function st_init_table.", failure);

  /* Call the function that really gets the job done. */
  for (i = 0; i < n; i++) {
    retValue = DddmpCuddDdArrayStoreSmvStep (ddMgr, Cudd_Regular(f[i]),
      fp, visited, inputNames);
    Dddmp_CheckAndGotoLabel (retValue==0,
      "Error if function DddmpCuddDdArrayStoreSmvStep.", failure);
  }

  /*
   *  To account for the possible complement on the root,
   *  we put either a buffer or an inverter at the output of
   *  the multiplexer representing the top node.
   */

  for (i=0; i<n; i++) {
    if (outputNames == NULL) {
      retValue = fprintf (fp,  "outNode%d := ", i);
    } else {
      retValue = fprintf (fp,  "%s := ", outputNames[i]);
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);

    if (Cudd_IsComplement(f[i])) {
#if SIZEOF_VOID_P == 8
      retValue = fprintf (fp, "!node%lx\n",
        (unsigned long) f[i] / (unsigned long) sizeof(DdNode));
#else
      retValue = fprintf (fp, "!node%x\n",
        (unsigned) f[i] / (unsigned) sizeof(DdNode));
#endif
    } else {
#if SIZEOF_VOID_P == 8
      retValue = fprintf (fp, "node%lx\n",
        (unsigned long) f[i] / (unsigned long) sizeof(DdNode));
#else
      retValue = fprintf (fp, "node%x\n",
        (unsigned) f[i] / (unsigned) sizeof(DdNode));
#endif
    }
    Dddmp_CheckAndGotoLabel (retValue==EOF,
      "Error during file store.", failure);
  }

  st_free_table (visited);

  return(1);

failure:
    if (visited != NULL) st_free_table(visited);
    return(0);

}

/**Function********************************************************************

  Synopsis    [Performs the recursive step of
    DddmpCuddDdArrayStoreSmvBody.]

  Description [Performs the recursive step of
    DddmpCuddDdArrayStoreSmvBody.
    Traverses the BDD f and writes a multiplexer-network description to the
    file pointed by fp.
    For each BDD node of function f, variable v, then child T, and else
    child E it stores:
    f = v * T + v' * E
    that is
    (OR f (AND v T) (AND (NOT v) E))
    If E is a complemented child this results in the following
    (OR f (AND v T) (AND (NOT v) (NOT E)))
    f is assumed to be a regular pointer and the function guarantees this
    assumption in the recursive calls.
    ]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

static int
DddmpCuddDdArrayStoreSmvStep (
  DdManager * ddMgr,
  DdNode * f,
  FILE * fp,
  st_table * visited,
  char ** names
  )
{
  DdNode *T, *E;
  int retValue;

#ifdef DDDMP_DEBUG
  assert(!Cudd_IsComplement(f));
#endif

  /* If already visited, nothing to do. */
  if (st_is_member(visited, (char *) f) == 1) {
    return(1);
  }

  /* Check for abnormal condition that should never happen. */
  if (f == NULL) {
    return(0);
  }

  /* Mark node as visited. */
  if (st_insert(visited, (char *) f, NULL) == ST_OUT_OF_MEM) {
    return(0);
  }

  /* Check for special case: If constant node, generate constant 1. */
  if (f == DD_ONE (ddMgr)) {
#if SIZEOF_VOID_P == 8
    retValue = fprintf (fp,
      "node%lx := 1;\n",
      (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
    retValue = fprintf (fp,
      "node%x := 1;\n",
      (unsigned) f / (unsigned) sizeof(DdNode));
#endif
    if (retValue == EOF) {
       return(0);
    } else {
       return(1);
    }
  }

  /*
   *  Check whether this is an ADD. We deal with 0-1 ADDs, but not
   *  with the general case.
   */

  if (f == DD_ZERO(ddMgr)) {
#if SIZEOF_VOID_P == 8
    retValue = fprintf (fp,
      "node%lx := 0;\n",
       (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
    retValue = fprintf (fp,
      "node%x := 0;\n",
      (unsigned) f / (unsigned) sizeof(DdNode));
#endif
   if (retValue == EOF) {
     return(0);
   } else {
     return(1);
   }
  }

  if (cuddIsConstant(f)) {
    return(0);
  }

  /* Recursive calls. */
  T = cuddT(f);
  retValue = DddmpCuddDdArrayStoreSmvStep (ddMgr,T,fp,visited,names);
  if (retValue != 1) {
    return(retValue);
  }
  E = Cudd_Regular(cuddE(f));
  retValue = DddmpCuddDdArrayStoreSmvStep (ddMgr,E,fp,visited,names);
  if (retValue != 1) {
    return(retValue);
  }

  /* Write multiplexer taking complement arc into account. */
#if SIZEOF_VOID_P == 8
  retValue = fprintf (fp, "node%lx := ",
    (unsigned long) f / (unsigned long) sizeof(DdNode));
#else
  retValue = fprintf (fp, "node%x := ",
    (unsigned) f / (unsigned) sizeof(DdNode));
#endif
  if (retValue == EOF) {
    return(0);
  }

  if (names != NULL) {
    retValue = fprintf(fp, "%s ", names[f->index]);
  } else {
    retValue = fprintf(fp, "inNode%d ", f->index);
  }
  if (retValue == EOF) {
    return(0);
  }

#if SIZEOF_VOID_P == 8
  retValue = fprintf (fp, "& node%lx | ",
    (unsigned long) T / (unsigned long) sizeof(DdNode));
#else
  retValue = fprintf (fp, "& node%x | ",
    (unsigned) T / (unsigned) sizeof(DdNode));
#endif
  if (retValue == EOF) {
    return(0);
  }

  if (names != NULL) {
    retValue = fprintf (fp, "!%s ", names[f->index]);
  } else {
    retValue = fprintf (fp, "!inNode%d ", f->index);
  }
  if (retValue == EOF) {
    return(0);
  }

#if SIZEOF_VOID_P == 8
  if (Cudd_IsComplement(cuddE(f))) {
    retValue = fprintf (fp, "& !node%lx\n",
      (unsigned long) E / (unsigned long) sizeof(DdNode));
  } else {
    retValue = fprintf (fp, "& node%lx\n",
      (unsigned long) E / (unsigned long) sizeof(DdNode));
  }
#else
  if (Cudd_IsComplement(cuddE(f))) {
    retValue = fprintf (fp, "& !node%x\n",
      (unsigned) E / (unsigned) sizeof(DdNode));
  } else {
    retValue = fprintf (fp, "& node%x\n",
      (unsigned) E / (unsigned) sizeof(DdNode));
  }
#endif

  if (retValue == EOF) {
    return(0);
  } else {
    return(1);
  }
}

