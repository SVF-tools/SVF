/**CFile**********************************************************************

  FileName     [dddmpLoadCnf.c]

  PackageName  [dddmp]

  Synopsis     [Functions to read in CNF from file as BDDs.]

  Description  [Functions to read in CNF from file as BDDs.
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
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define DDDMP_MAX_CNF_ROW_LENGTH 1000
#define DDDMP_DEBUG_CNF 0

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

#define matchkeywd(str,key) (strncmp(str,key,strlen(key))==0)

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int DddmpCuddDdArrayLoadCnf(DdManager *ddMgr, Dddmp_RootMatchType rootmatchmode, char **rootmatchnames, Dddmp_VarMatchType varmatchmode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp, DdNode ***rootsPtrPtr, int *nRoots);
static Dddmp_Hdr_t * DddmpBddReadHeaderCnf(char *file, FILE *fp);
static void DddmpFreeHeaderCnf(Dddmp_Hdr_t *Hdr);
static int DddmpReadCnfClauses(Dddmp_Hdr_t *Hdr, int ***cnfTable, FILE *fp);
static int DddmpCnfClauses2Bdd(Dddmp_Hdr_t *Hdr, DdManager *ddMgr, int **cnfTable, int mode, DdNode ***rootsPtrPtr);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Reads a dump file in a CNF format.]

  Description  [Reads a dump file representing the argument BDD in a
    CNF formula.
    Dddmp_cuddBddArrayLoadCnf is used through a dummy array.
    The results is returned in different formats depending on the 
    mode selection:
      IFF mode == 0 Return the Clauses without Conjunction
      IFF mode == 1 Return the sets of BDDs without Quantification
      IFF mode == 2 Return the sets of BDDs AFTER Existential Quantification
   ]

  SideEffects  [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso      [Dddmp_cuddBddLoad, Dddmp_cuddBddArrayLoad]

******************************************************************************/

int
Dddmp_cuddBddLoadCnf (
  DdManager *ddMgr                /* IN: DD Manager */,
  Dddmp_VarMatchType varmatchmode /* IN: storing mode selector */,
  char **varmatchnames            /* IN: array of variable names, by IDs */,
  int *varmatchauxids             /* IN: array of variable auxids, by IDs */,
  int *varcomposeids              /* IN: array of new ids accessed, by IDs */,
  int mode                        /* IN: computation mode */,
  char *file		          /* IN: file name */,
  FILE *fp                        /* IN: file pointer */,
  DdNode ***rootsPtrPtr           /* OUT: array of returned BDD roots */,
  int *nRoots                     /* OUT: number of BDDs returned */
  )
{
  int i, retValue;

  retValue = Dddmp_cuddBddArrayLoadCnf (ddMgr, DDDMP_ROOT_MATCHLIST, NULL,
    varmatchmode, varmatchnames, varmatchauxids, varcomposeids, mode,
    file, fp, rootsPtrPtr, nRoots);

  if (retValue == DDDMP_FAILURE) {
    return (DDDMP_FAILURE);
  }

  if (*nRoots > 1) {
    fprintf (stderr,
      "Warning: %d BDD roots found in file. Only first retrieved.\n",
      *nRoots);
    for (i=1; i<*nRoots; i++) {
      Cudd_RecursiveDeref (ddMgr, *rootsPtrPtr[i]);
    } 
  } 

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis     [Reads a dump file in a CNF format.]

  Description  [Reads a dump file representing the argument BDD in a
    CNF formula.
    ]

  SideEffects  [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso      [Dddmp_cuddBddArrayLoad]

******************************************************************************/

int
Dddmp_cuddBddArrayLoadCnf (
  DdManager *ddMgr                 /* IN: DD Manager */,
  Dddmp_RootMatchType rootmatchmode/* IN: storing mode selector */,
  char **rootmatchnames            /* IN: sorted names for loaded roots */,
  Dddmp_VarMatchType varmatchmode  /* IN: storing mode selector */,
  char **varmatchnames             /* IN: array of variable names, by IDs */,
  int *varmatchauxids              /* IN: array of variable auxids, by IDs */,
  int *varcomposeids               /* IN: array of new ids, by IDs */,
  int mode                         /* IN: computation Mode */,
  char *file		           /* IN: file name */,
  FILE *fp                         /* IN: file pointer */,
  DdNode ***rootsPtrPtr            /* OUT: array of returned BDD roots */,
  int *nRoots                      /* OUT: number of BDDs returned */
  )
{
  int retValue;

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During CNF Load.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During CNF Load.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  retValue = DddmpCuddDdArrayLoadCnf (ddMgr, rootmatchmode,
    rootmatchnames, varmatchmode, varmatchnames, varmatchauxids,
    varcomposeids, mode, file, fp, rootsPtrPtr, nRoots);

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During CNF Load.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During CNF Load.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  return (retValue);
}

/**Function********************************************************************

  Synopsis    [Reads the header of a dump file representing the argument BDDs]

  Description [Reads the header of a dump file representing the argument BDDs. 
    Returns main information regarding DD type stored in the file,
    the variable ordering used, the number of variables, etc.
    It reads only the header of the file NOT the BDD/ADD section. 
    ]

  SideEffects []

  SeeAlso     [Dddmp_cuddBddArrayLoad]

******************************************************************************/

int
Dddmp_cuddHeaderLoadCnf (
  int *nVars                /* OUT: number of DD variables */,
  int *nsuppvars            /* OUT: number of support variables */,
  char ***suppVarNames      /* OUT: array of support variable names */,
  char ***orderedVarNames   /* OUT: array of variable names */,
  int **varIds              /* OUT: array of variable ids */,
  int **varComposeIds       /* OUT: array of permids ids */,
  int **varAuxIds           /* OUT: array of variable aux ids */,
  int *nRoots               /* OUT: number of root in the file */,
  char *file                 /* IN: file name */,
  FILE *fp                   /* IN: file pointer */
  )
{
  Dddmp_Hdr_t *Hdr;
  int i, fileToClose;
  char **tmpOrderedVarNames = NULL;
  char **tmpSuppVarNames = NULL;
  int *tmpVarIds = NULL;
  int *tmpVarComposeIds = NULL;
  int *tmpVarAuxIds = NULL; 

  fileToClose = 0;
  if (fp == NULL) {
    fp = fopen (file, "r");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  Hdr = DddmpBddReadHeaderCnf (NULL, fp);

  Dddmp_CheckAndGotoLabel (Hdr->nnodes==0, "Zero number of nodes.",
    failure);

  /*
   *  Number of variables (tot and support)
   */

  *nVars = Hdr->nVars;
  *nsuppvars = Hdr->nsuppvars;

  /*
   *  Support Varnames
   */

  if (Hdr->suppVarNames != NULL) {
    tmpSuppVarNames = DDDMP_ALLOC (char *, *nsuppvars);
    Dddmp_CheckAndGotoLabel (tmpSuppVarNames==NULL, "Error allocating memory.",
      failure);

    for (i=0; i<*nsuppvars; i++) {
      tmpSuppVarNames[i] = DDDMP_ALLOC (char,
        (strlen (Hdr->suppVarNames[i]) + 1));
      Dddmp_CheckAndGotoLabel (Hdr->suppVarNames[i]==NULL,
        "Support Variable Name Missing in File.", failure);
      strcpy (tmpSuppVarNames[i], Hdr->suppVarNames[i]);
    }

    *suppVarNames = tmpSuppVarNames;
  } else {
    *suppVarNames = NULL;
  }

  /*
   *  Ordered Varnames
   */

  if (Hdr->orderedVarNames != NULL) {
    tmpOrderedVarNames = DDDMP_ALLOC (char *, *nVars);
    Dddmp_CheckAndGotoLabel (tmpOrderedVarNames==NULL,
      "Error allocating memory.", failure);

    for (i=0; i<*nVars; i++) {
      tmpOrderedVarNames[i]  = DDDMP_ALLOC (char,
        (strlen (Hdr->orderedVarNames[i]) + 1));
      Dddmp_CheckAndGotoLabel (Hdr->orderedVarNames[i]==NULL,
        "Support Variable Name Missing in File.", failure);
      strcpy (tmpOrderedVarNames[i], Hdr->orderedVarNames[i]);
    }

    *orderedVarNames = tmpOrderedVarNames;
  } else {
    *orderedVarNames = NULL;
  }

  /*
   *  Variable Ids
   */

  if (Hdr->ids != NULL) {
    tmpVarIds = DDDMP_ALLOC (int, *nsuppvars);
    Dddmp_CheckAndGotoLabel (tmpVarIds==NULL, "Error allocating memory.",
      failure);
    for (i=0; i<*nsuppvars; i++) {
      tmpVarIds[i] = Hdr->ids[i];
    }

    *varIds = tmpVarIds; 
  } else {
    *varIds = NULL;
  }

  /*
   *  Variable Compose Ids
   */

  if (Hdr->permids != NULL) {
    tmpVarComposeIds = DDDMP_ALLOC (int, *nsuppvars);
    Dddmp_CheckAndGotoLabel (tmpVarComposeIds==NULL,
      "Error allocating memory.", failure);
    for (i=0; i<*nsuppvars; i++) {
      tmpVarComposeIds[i] = Hdr->permids[i];
    }

    *varComposeIds = tmpVarComposeIds; 
  } else {
    *varComposeIds = NULL;
  }

  /*
   *  Variable Auxiliary Ids
   */

  if (Hdr->auxids != NULL) {
    tmpVarAuxIds = DDDMP_ALLOC (int, *nsuppvars);
    Dddmp_CheckAndGotoLabel (tmpVarAuxIds==NULL,
      "Error allocating memory.", failure);
    for (i=0; i<*nsuppvars; i++) {
      tmpVarAuxIds[i] = Hdr->auxids[i];
    }

    *varAuxIds = tmpVarAuxIds; 
  } else {
    *varAuxIds = NULL;
  }

  /*
   *  Number of roots
   */

  *nRoots = Hdr->nRoots;

  /*
   *  Free and Return
   */

  if (fileToClose == 1) {
    fclose (fp);
  }
	
  DddmpFreeHeaderCnf (Hdr);

  return (DDDMP_SUCCESS);

  failure:
    return (DDDMP_FAILURE);
}

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Reads a dump file representing the argument BDDs in CNF
     format.
    ]

  Description [Reads a dump file representing the argument BDDs in CNF
    format.
      IFF mode == 0 Return the Clauses without Conjunction
      IFF mode == 1 Return the sets of BDDs without Quantification
      IFF mode == 2 Return the sets of BDDs AFTER Existential Quantification
    ]

  SideEffects [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso     [Dddmp_cuddBddArrayLoad]

******************************************************************************/

static int
DddmpCuddDdArrayLoadCnf (
  DdManager *ddMgr                 /* IN: DD Manager */,
  Dddmp_RootMatchType rootmatchmode/* IN: storing mode selector */,
  char **rootmatchnames            /* IN: sorted names for loaded roots */,
  Dddmp_VarMatchType varmatchmode  /* IN: storing mode selector */,
  char **varmatchnames             /* IN: array of variable names, by ids */,
  int *varmatchauxids              /* IN: array of variable auxids, by ids */,
  int *varcomposeids               /* IN: array of new ids, by ids */,
  int mode                         /* IN: computation mode */,
  char *file		           /* IN: file name */,
  FILE *fp                         /* IN: file pointer */,
  DdNode ***rootsPtrPtr            /* OUT: array of BDD roots */,
  int *nRoots                      /* OUT: number of BDDs returned */
  )
{
  Dddmp_Hdr_t *Hdr = NULL;
  int **cnfTable = NULL;
  int fileToClose = 0;
  int retValue, i;

  fileToClose = 0;
  *rootsPtrPtr = NULL;

  if (fp == NULL) {
    fp = fopen (file, "r");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  /*--------------------------- Read the Header -----------------------------*/

  Hdr = DddmpBddReadHeaderCnf (NULL, fp);

  Dddmp_CheckAndGotoLabel (Hdr->nnodes==0, "Zero number of nodes.",
    failure);

  /*------------------------ Read the CNF Clauses ---------------------------*/

  retValue = DddmpReadCnfClauses (Hdr, &cnfTable, fp);

  Dddmp_CheckAndGotoLabel (retValue==DDDMP_FAILURE,
    "Read CNF Clauses Failure.",  failure);

  /*------------------------- From Clauses to BDDs --------------------------*/

  retValue = DddmpCnfClauses2Bdd (Hdr, ddMgr, cnfTable, mode, rootsPtrPtr);

  Dddmp_CheckAndGotoLabel (retValue==DDDMP_FAILURE,
    "CNF Clauses To BDDs Failure.",  failure);

  *nRoots = Hdr->nRoots;

  if (fileToClose) {
    fclose (fp);
  }

  for (i=0; i<Hdr->nClausesCnf; i++) {
    DDDMP_FREE (cnfTable[i]);
  }
  DDDMP_FREE (cnfTable);

  DddmpFreeHeaderCnf (Hdr);

  return (DDDMP_SUCCESS);

  /*
   *  Failure Condition
   */

failure:

  if (fileToClose) {
    fclose (fp);
  }

  for (i=0; i<Hdr->nClausesCnf; i++) {
    DDDMP_FREE (cnfTable[i]);
  }
  DDDMP_FREE (cnfTable);

  DddmpFreeHeaderCnf (Hdr);

  /* return 0 on error ! */
  nRoots = 0;

  return (DDDMP_FAILURE);
}

/**Function********************************************************************

  Synopsis    [Reads a the header of a dump file representing the argument 
    BDDs.
    ]

  Description [Reads the header of a dump file. Builds a Dddmp_Hdr_t struct
    containing all infos in the header, for next manipulations.
    ]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/

static Dddmp_Hdr_t *
DddmpBddReadHeaderCnf (
  char *file	  /* IN: file name */,
  FILE *fp        /* IN: file pointer */
  )
{
  Dddmp_Hdr_t *Hdr = NULL;
  char buf[DDDMP_MAXSTRLEN];
  int nv, nc, retValue, fileToClose = 0;

  if (fp == NULL) {
    fp = fopen (file, "r");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  /* Start Header */
  Hdr = DDDMP_ALLOC (Dddmp_Hdr_t, 1);
  if (Hdr == NULL) {
    return NULL;
  }

  Hdr->ver = NULL;
  Hdr->mode = 0;
  Hdr->ddType = DDDMP_CNF;
  Hdr->varinfo = DDDMP_VARIDS;
  Hdr->dd = NULL;
  Hdr->nnodes = 0;
  Hdr->nVars = 0;
  Hdr->nsuppvars = 0;
  Hdr->orderedVarNames = NULL;
  Hdr->suppVarNames = NULL;
  Hdr->ids = NULL;
  Hdr->permids = NULL;
  Hdr->auxids = NULL;
  Hdr->cnfids = NULL;
  Hdr->nRoots = 0;
  Hdr->rootids = NULL;
  Hdr->rootnames = NULL;
  Hdr->nAddedCnfVar = 0;
  Hdr->nVarsCnf = 0;
  Hdr->nClausesCnf = 0;

  while (fscanf (fp, "%s", buf) != EOF) {

    /* Init Problem Line */ 
    if (buf[0] == 'p') {
      fscanf (fp, "%*s %d %d", &nv, &nc);
      Hdr->nVarsCnf = nv;
      Hdr->nClausesCnf = nc;
      break;
    }

    /* CNF Comment Line */
    if (buf[0] == 'c') {
      if (fscanf (fp, "%s", buf) == EOF) {
        break;
      }
    }

    /* Skip Comment? */
    if (buf[0] != '.') {
      fgets (buf, DDDMP_MAXSTRLEN, fp);
      continue;
    }

    if (matchkeywd (buf, ".ver")) {    
      /* this not checked so far: only read */
      retValue = fscanf (fp, "%s", buf);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading from file.",
        failure);

      Hdr->ver=DddmpStrDup(buf);
      Dddmp_CheckAndGotoLabel (Hdr->ver==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".dd")) {    
      retValue = fscanf (fp, "%s", buf);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      Hdr->dd = DddmpStrDup (buf);
      Dddmp_CheckAndGotoLabel (Hdr->dd==NULL, "Error allocating memory.",
        failure);

      continue;
    }

    if (matchkeywd (buf, ".nnodes")) {
      retValue = fscanf (fp, "%d", &(Hdr->nnodes));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd (buf, ".nvars")) {   
      retValue = fscanf (fp, "%d", &(Hdr->nVars));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd (buf, ".nsuppvars")) {
      retValue = fscanf (fp, "%d", &(Hdr->nsuppvars));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd (buf, ".orderedvarnames")) {
      Hdr->orderedVarNames = DddmpStrArrayRead (fp, Hdr->nVars);
      Dddmp_CheckAndGotoLabel (Hdr->orderedVarNames==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".suppvarnames")) {
      Hdr->suppVarNames = DddmpStrArrayRead (fp, Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->suppVarNames==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if matchkeywd (buf, ".ids") {
      Hdr->ids = DddmpIntArrayRead(fp,Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->ids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".permids")) {
      Hdr->permids = DddmpIntArrayRead(fp,Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->permids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".auxids")) {
      Hdr->auxids = DddmpIntArrayRead(fp,Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->auxids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".cnfids")) {
      Hdr->cnfids = DddmpIntArrayRead (fp, Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->cnfids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".nroots")) {
      retValue = fscanf (fp, "%d", &(Hdr->nRoots));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd (buf, ".rootids")) {
      Hdr->rootids = DddmpIntArrayRead(fp,Hdr->nRoots);
      Dddmp_CheckAndGotoLabel (Hdr->rootids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd (buf, ".rootnames")) {
      Hdr->rootnames = DddmpStrArrayRead(fp,Hdr->nRoots);
      Dddmp_CheckAndGotoLabel (Hdr->rootnames==NULL,
        "Error allocating memory.", failure);

      continue;
    }


    if (matchkeywd (buf, ".nAddedCnfVar")) {
      retValue = fscanf (fp, "%d", &(Hdr->nAddedCnfVar));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }
  }

  /* END HEADER */
  return (Hdr);

failure:

  if (fileToClose == 1) {
    fclose (fp);
  }

  DddmpFreeHeaderCnf (Hdr);

  return (NULL);
}


/**Function********************************************************************

  Synopsis    [Frees the internal header structure.]

  Description [Frees the internal header structure by freeing all internal
    fields first.
    ]

  SideEffects []

  SeeAlso     []

******************************************************************************/

static void
DddmpFreeHeaderCnf (
  Dddmp_Hdr_t *Hdr   /* IN: pointer to header */
  )
{
  if (Hdr==NULL) {
    return;
  }

  DDDMP_FREE (Hdr->ver);
  DDDMP_FREE (Hdr->dd);
  DddmpStrArrayFree (Hdr->orderedVarNames, Hdr->nVars);
  DddmpStrArrayFree (Hdr->suppVarNames, Hdr->nsuppvars);
  DDDMP_FREE (Hdr->ids);
  DDDMP_FREE (Hdr->permids);
  DDDMP_FREE (Hdr->auxids);
  DDDMP_FREE (Hdr->cnfids);
  DDDMP_FREE (Hdr->rootids);
  DddmpStrArrayFree (Hdr->rootnames, Hdr->nRoots);

  DDDMP_FREE (Hdr);

  return;
}

/**Function********************************************************************

  Synopsis    [Read the CNF clauses from the file in the standard DIMACS
    format.
    ]

  Description [Read the CNF clauses from the file in the standard DIMACS
    format. Store all the clauses in an internal structure for 
    future transformation into BDDs.
    ]

  SideEffects []

  SeeAlso     []

******************************************************************************/

static int
DddmpReadCnfClauses (
  Dddmp_Hdr_t *Hdr  /*  IN: file header */,
  int ***cnfTable   /* OUT: CNF table for clauses */,
  FILE *fp          /*  IN: source file */
  )
{
  char word[DDDMP_MAX_CNF_ROW_LENGTH];
  int i, j, var;
  int **cnfTableLocal = NULL;
  int *clause = NULL;

  cnfTableLocal = DDDMP_ALLOC (int *, Hdr->nClausesCnf);
  clause = DDDMP_ALLOC (int, 2*Hdr->nVarsCnf+1);

  for (i=0; i<Hdr->nClausesCnf; i++) {
    cnfTableLocal[i] = NULL;
  }

  for (i=0; i<=2*Hdr->nVarsCnf; i++) {
    clause[i] = 0;
  }

  i = j = 0;
  do { 
    if (fscanf(fp, "%s", word)==EOF) {
      if (j>0) {
        /* force last zero */
        strcpy(word,"0");
      }
      else break;
    }

    /* Check for Comment */
    if (word[0] == 'c') {
      /* Comment Found: Skip line */
      fgets (word, DDDMP_MAX_CNF_ROW_LENGTH-1, fp);
      break;
    }

    var = atoi (word);
    Dddmp_Assert ((var>=(-Hdr->nVarsCnf))&&(var<=Hdr->nVarsCnf),
      "Wrong num found");
    clause[j++] = var;
    if (var == 0) {
      cnfTableLocal[i] = DDDMP_ALLOC (int, j);
      while (--j >=0) {
        cnfTableLocal[i][j] = clause[j];
	}
      i++;
      j=0;
    }

  } while (!feof(fp));

  Dddmp_Assert (i==Hdr->nClausesCnf,
    "Wrong number of clauses in file");

#if DDDMP_DEBUG_CNF
  for (i=0; i<Hdr->nClausesCnf; i++) {
    fprintf (stdout, "[%4d] ", i);
    j=0;
    while ((var = cnfTableLocal[i][j++]) != 0) {
      fprintf (stdout, "%d ", var);
    } 
    fprintf (stdout, "0\n");
  }
#endif

  DDDMP_FREE (clause);

  *cnfTable = cnfTableLocal;

  return (DDDMP_SUCCESS);
}

/**Function********************************************************************

  Synopsis    [Transforms CNF clauses into BDDs.]

  Description [Transforms CNF clauses into BDDs. Clauses are stored in an
   internal structure previously read. The results can be given in
   different format according to the mode selection:
     IFF mode == 0 Return the Clauses without Conjunction
     IFF mode == 1 Return the sets of BDDs without Quantification
     IFF mode == 2 Return the sets of BDDs AFTER Existential Quantification
   ]

  SideEffects []

  SeeAlso     []

******************************************************************************/

static int
DddmpCnfClauses2Bdd (
  Dddmp_Hdr_t *Hdr       /* IN: file header */,
  DdManager *ddMgr       /* IN: DD Manager */,
  int **cnfTable         /* IN: CNF table for clauses */,
  int mode               /* IN: computation mode */,
  DdNode ***rootsPtrPtr  /* OUT: array of returned BDD roots (by reference) */
  )
{
  DdNode **rel = NULL;
  DdNode *lit = NULL;
  DdNode *tmp1 = NULL;
  DdNode *tmp2 = NULL;
  DdNode **rootsPtr = NULL;
  DdNode *cubeAllVar = NULL;
  DdNode *cubeBddVar = NULL;
  DdNode *cubeCnfVar = NULL;
  int i, j, k, n, var1, var2, fromLine, toLine;

  rootsPtr = NULL;
  *rootsPtrPtr = NULL;

  /*-------------------------- Read The Clauses -----------------------------*/

  rel = DDDMP_ALLOC (DdNode *, Hdr->nClausesCnf);

  cubeBddVar = Cudd_ReadOne (ddMgr);
  cubeCnfVar = Cudd_ReadOne (ddMgr);
  cubeAllVar = Cudd_ReadOne (ddMgr);
  Cudd_Ref (cubeBddVar);
  Cudd_Ref (cubeCnfVar);
  Cudd_Ref (cubeAllVar);

  for (i=0; i<Hdr->nClausesCnf; i++) {
    rel[i] = Cudd_Not (Cudd_ReadOne (ddMgr));
    Cudd_Ref (rel[i]);
    j=0;
    while ((var1 = cnfTable[i][j++]) != 0) {

      /* Deal with the Literal */
      var2 = abs (var1);
      n = (-1);
      for (k=0; k<Hdr->nsuppvars; k++) {
        if (Hdr->cnfids[k] == var2) {
          n = k;
          break;
        }
      }

      if (n == (-1)) {
        lit = Cudd_bddIthVar (ddMgr, var2);

        /* Create the cubes of CNF Variables */
        tmp1 = Cudd_bddAnd (ddMgr, cubeCnfVar, lit);
        Cudd_Ref (tmp1);
        Cudd_RecursiveDeref (ddMgr, cubeCnfVar);
        cubeCnfVar = tmp1;

      } else {
        lit = Cudd_bddIthVar (ddMgr, Hdr->ids[n]);

        /* Create the cubes of BDD Variables */
        tmp1 = Cudd_bddAnd (ddMgr, cubeBddVar, lit);
        Cudd_Ref (tmp1);
        Cudd_RecursiveDeref (ddMgr, cubeBddVar);
        cubeBddVar = tmp1;
      }

      /* Create the cubes of ALL Variables */
      tmp1 = Cudd_bddAnd (ddMgr, cubeAllVar, lit);
      Cudd_Ref (tmp1);
      Cudd_RecursiveDeref (ddMgr, cubeAllVar);
      cubeAllVar = tmp1;

      /* Deal with Relations */
      if (var1<0) {
        lit = Cudd_Not (lit);
      }
      tmp1 = Cudd_bddOr (ddMgr, rel[i], lit);
      Cudd_Ref (tmp1);
      Cudd_RecursiveDeref (ddMgr, rel[i]);
      rel[i] = tmp1;
    }
  }

  /*
   *  Mode == 0 Return the Clauses without Conjunction
   */

  if (mode == 0) {
    return (DDDMP_SUCCESS);
  }

  rootsPtr = DDDMP_ALLOC (DdNode *, Hdr->nRoots);
  Dddmp_CheckAndGotoLabel (rootsPtr==NULL, "Error allocating memory.",
    failure);

  for (i=0; i<Hdr->nRoots; i++) {
    if (i == (Hdr->nRoots-1)) {
      fromLine = Hdr->rootids[i] - 1;
      toLine = Hdr->nClausesCnf;
    } else {
      fromLine = Hdr->rootids[i] - 1;
      toLine = Hdr->rootids[i+1];
    }

    tmp1 = Cudd_ReadOne (ddMgr);
    Cudd_Ref (tmp1);  
    for (j=fromLine; j<toLine; j++) {
      tmp2 = Cudd_bddAnd (ddMgr, rel[j], tmp1);
      Cudd_Ref (tmp2);
      Cudd_RecursiveDeref (ddMgr, tmp1);
      Cudd_RecursiveDeref (ddMgr, rel[j]);
      tmp1 = tmp2;
    }
    rootsPtr[i] = tmp1;
  }

  DDDMP_FREE (rel);

  /*
   *  Mode == 1 Return the sets of BDDs without Quantification
   */

  if (mode == 1) {
    *rootsPtrPtr = rootsPtr;
    return (DDDMP_SUCCESS);
  }

  /*
   *  Mode == 2 Return the sets of BDDs AFTER Existential Quantification
   */

#if DDDMP_DEBUG_CNF
  cubeBddVar = Cudd_ReadOne (ddMgr);
  Cudd_Ref (cubeBddVar);
  for (i=0; i<Hdr->nsuppvars; i++) {
    lit = Cudd_bddIthVar (ddMgr, Hdr->ids[i]);
    tmp1 = Cudd_bddAnd (ddMgr, cubeBddVar, lit);
    Cudd_Ref (tmp1);
    Cudd_RecursiveDeref (ddMgr, cubeBddVar);
    cubeBddVar = tmp1;
  }

  cubeCnfVar = Cudd_bddExistAbstract (ddMgr, cubeAllVar, cubeBddVar);
#endif

  for (i=0; i<Hdr->nRoots; i++) {
#if DDDMP_DEBUG_CNF
    fprintf (stdout, "rootsPtr Before Exist:\n");
    Cudd_PrintDebug (ddMgr, rootsPtr[i], 0, 3);
#endif

    tmp1 = Cudd_bddExistAbstract (ddMgr, rootsPtr[i], cubeCnfVar);
    Cudd_RecursiveDeref (ddMgr, rootsPtr[i]);
    rootsPtr[i] = tmp1;

#if DDDMP_DEBUG_CNF
    fprintf (stdout, "rootsPtr After Exist:\n");
    Cudd_PrintDebug (ddMgr, rootsPtr[i], 0, 3);
#endif
  }

#if DDDMP_DEBUG_CNF
  fprintf (stdout, "cubeAllVar:\n");
  Cudd_PrintDebug (ddMgr, cubeAllVar, 0, 3);
  fprintf (stdout, "cubeBddVar:\n");
  Cudd_PrintDebug (ddMgr, cubeBddVar, 0, 3);
  fprintf (stdout, "cubeCnfVar:\n");
  Cudd_PrintDebug (ddMgr, cubeCnfVar, 0, 3);
#endif
    
  Cudd_RecursiveDeref (ddMgr, cubeAllVar);
  Cudd_RecursiveDeref (ddMgr, cubeBddVar);
  Cudd_RecursiveDeref (ddMgr, cubeCnfVar);
  *rootsPtrPtr = rootsPtr;

  return (DDDMP_SUCCESS);

  /*
   *  Failure Condition
   */

failure:

  DDDMP_FREE (rel);
  DDDMP_FREE (rootsPtrPtr);

  return (DDDMP_FAILURE);
}











