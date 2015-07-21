/**CFile**********************************************************************

  FileName     [dddmpLoad.c]

  PackageName  [dddmp]

  Synopsis     [Functions to read in bdds to file]

  Description  [Functions to read in bdds to file.  BDDs
    are represended on file either in text or binary format under the
    following rules.  A file contains a forest of BDDs (a vector of
    Boolean functions).  BDD nodes are numbered with contiguous numbers,
    from 1 to NNodes (total number of nodes on a file). 0 is not used to
    allow negative node indexes for complemented edges.  A file contains
    a header, including information about variables and roots to BDD
    functions, followed by the list of nodes.  BDD nodes are listed
    according to their numbering, and in the present implementation
    numbering follows a post-order strategy, in such a way that a node
    is never listed before its Then/Else children.
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

#define matchkeywd(str,key) (strncmp(str,key,strlen(key))==0)

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static int DddmpCuddDdArrayLoad(Dddmp_DecompType ddType, DdManager *ddMgr, Dddmp_RootMatchType rootMatchMode, char **rootmatchnames, Dddmp_VarMatchType varMatchMode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp, DdNode ***pproots);
static Dddmp_Hdr_t * DddmpBddReadHeader(char *file, FILE *fp);
static void DddmpFreeHeader(Dddmp_Hdr_t *Hdr);

/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis     [Reads a dump file representing the argument BDD.]

  Description  [Reads a dump file representing the argument BDD.
    Dddmp_cuddBddArrayLoad is used through a dummy array (see this
    function's description for more details).
    Mode, the requested input file format, is checked against 
    the file format.
    The loaded BDDs is referenced before returning it.
    ]

  SideEffects  [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso      [Dddmp_cuddBddStore, Dddmp_cuddBddArrayLoad]

******************************************************************************/

DdNode *
Dddmp_cuddBddLoad (
  DdManager *ddMgr                /* IN: DD Manager */,
  Dddmp_VarMatchType varMatchMode /* IN: storing mode selector */,
  char **varmatchnames            /* IN: array of variable names - by IDs */,
  int *varmatchauxids             /* IN: array of variable auxids - by IDs */,
  int *varcomposeids              /* IN: array of new ids accessed - by IDs */,
  int mode                        /* IN: requested input file format */,
  char *file                      /* IN: file name */,
  FILE *fp                        /* IN: file pointer */
  )
{
  DdNode *f , **tmpArray;
  int i, nRoots;

  nRoots = Dddmp_cuddBddArrayLoad(ddMgr,DDDMP_ROOT_MATCHLIST,NULL,
    varMatchMode,varmatchnames,varmatchauxids,varcomposeids,
    mode,file,fp,&tmpArray);

  if (nRoots == 0) {
    return (NULL);
  } else {
    f = tmpArray[0];
    if (nRoots > 1) {
      fprintf (stderr,
        "Warning: %d BDD roots found in file. Only first retrieved.\n",
         nRoots);
      for (i=1; i<nRoots; i++) {
        Cudd_RecursiveDeref (ddMgr, tmpArray[i]);
      } 
    } 
    DDDMP_FREE (tmpArray);
    return (f);
  }
}

/**Function********************************************************************

  Synopsis    [Reads a dump file representing the argument BDDs.]

  Description [Reads a dump file representing the argument BDDs. The header is
    common to both text and binary mode. The node list is either 
    in text or binary format. A dynamic vector of DD pointers 
    is allocated to support conversion from DD indexes to pointers.
    Several criteria are supported for variable match between file
    and dd manager. Several changes/permutations/compositions are allowed
    for variables while loading DDs. Variable of the dd manager are allowed 
    to match with variables on file on ids, permids, varnames, 
    varauxids; also direct composition between ids and 
    composeids is supported. More in detail:
    <ol>
    <li> varMatchMode=DDDMP_VAR_MATCHIDS <p>
    allows the loading of a DD keeping variable IDs unchanged
    (regardless of the variable ordering of the reading manager); this
    is useful, for example, when swapping DDs to file and restoring them
    later from file, after possible variable reordering activations.
    
    <li> varMatchMode=DDDMP_VAR_MATCHPERMIDS <p>
    is used to allow variable match according to the position in the
    ordering.
    
    <li> varMatchMode=DDDMP_VAR_MATCHNAMES <p>
    requires a non NULL varmatchnames parameter; this is a vector of
    strings in one-to-one correspondence with variable IDs of the
    reading manager. Variables in the DD file read are matched with
    manager variables according to their name (a non NULL varnames
    parameter was required while storing the DD file).
    
    <li> varMatchMode=DDDMP_VAR_MATCHIDS <p>
    has a meaning similar to DDDMP_VAR_MATCHNAMES, but integer auxiliary
    IDs are used instead of strings; the additional non NULL
    varmatchauxids parameter is needed.
    
    <li> varMatchMode=DDDMP_VAR_COMPOSEIDS <p>
    uses the additional varcomposeids parameter is used as array of
    variable ids to be composed with ids stored in file.
    </ol>
    
    In the present implementation, the array varnames (3), varauxids (4)
    and composeids (5) need to have one entry for each variable in the 
    DD manager (NULL pointers are allowed for unused variables
    in varnames). Hence variables need to be already present in the 
    manager. All arrays are sorted according to IDs.

    All the loaded BDDs are referenced before returning them.
    ]

  SideEffects [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso     [Dddmp_cuddBddArrayStore]

******************************************************************************/

int
Dddmp_cuddBddArrayLoad (
  DdManager *ddMgr                  /* IN: DD Manager */,
  Dddmp_RootMatchType rootMatchMode /* IN: storing mode selector */,
  char **rootmatchnames             /* IN: sorted names for loaded roots */,
  Dddmp_VarMatchType varMatchMode   /* IN: storing mode selector */,
  char **varmatchnames              /* IN: array of variable names, by ids */,
  int *varmatchauxids               /* IN: array of variable auxids, by ids */,
  int *varcomposeids                /* IN: array of new ids, by ids */,
  int mode                          /* IN: requested input file format */,
  char *file                        /* IN: file name */,
  FILE *fp                          /* IN: file pointer */,
  DdNode ***pproots                 /* OUT: array of returned BDD roots */
  )
{
  int retValue;

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Load.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Load.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  retValue = DddmpCuddDdArrayLoad (DDDMP_BDD, ddMgr, rootMatchMode,
     rootmatchnames, varMatchMode, varmatchnames, varmatchauxids,
     varcomposeids, mode, file, fp, pproots);

#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During BDD Load.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During BDD Load.\n");
      fflush (stderr);
    }
  }
#endif
#endif

  return (retValue);
}

/**Function********************************************************************

  Synopsis    [Reads a dump file representing the argument ADD.]

  Description [Reads a dump file representing the argument ADD.
    Dddmp_cuddAddArrayLoad is used through a dummy array.
    ]

  SideEffects [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso     [Dddmp_cuddAddStore, Dddmp_cuddAddArrayLoad]

******************************************************************************/

DdNode *
Dddmp_cuddAddLoad (
  DdManager *ddMgr                /* IN: Manager */,
  Dddmp_VarMatchType varMatchMode /* IN: storing mode selector */,
  char **varmatchnames            /* IN: array of variable names by IDs */,
  int  *varmatchauxids            /* IN: array of variable auxids by IDs */,
  int  *varcomposeids             /* IN: array of new ids by IDs */,
  int mode                        /* IN: requested input file format */,
  char *file                      /* IN: file name */,
  FILE *fp                        /* IN: file pointer */
  )
{
  DdNode *f , **tmpArray;
  int i, nRoots;

  nRoots = Dddmp_cuddAddArrayLoad (ddMgr, DDDMP_ROOT_MATCHLIST,NULL,
    varMatchMode, varmatchnames, varmatchauxids, varcomposeids,
    mode, file, fp, &tmpArray);

  if (nRoots == 0) {
    return (NULL);
  } else {
    f = tmpArray[0];
    if (nRoots > 1) {
      fprintf (stderr,
        "Warning: %d BDD roots found in file. Only first retrieved.\n",
        nRoots);
      for (i=1; i<nRoots; i++) {
        Cudd_RecursiveDeref (ddMgr, tmpArray[i]);
      } 
    } 
    DDDMP_FREE (tmpArray);
    return (f);
  }
}

/**Function********************************************************************

  Synopsis    [Reads a dump file representing the argument ADDs.]

  Description [Reads a dump file representing the argument ADDs. See 
    BDD load functions for detailed explanation.
    ]

  SideEffects [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso     [Dddmp_cuddBddArrayStore]

******************************************************************************/

int
Dddmp_cuddAddArrayLoad (
  DdManager *ddMgr                  /* IN: DD Manager */,
  Dddmp_RootMatchType rootMatchMode /* IN: storing mode selector */,
  char **rootmatchnames             /* IN: sorted names for loaded roots */,
  Dddmp_VarMatchType varMatchMode   /* IN: storing mode selector */,
  char **varmatchnames              /* IN: array of variable names, by ids */,
  int *varmatchauxids               /* IN: array of variable auxids, by ids */,
  int *varcomposeids                /* IN: array of new ids, by ids */,
  int mode                          /* IN: requested input file format */,
  char *file                        /* IN: file name */,
  FILE *fp                          /* IN: file pointer */,
  DdNode ***pproots                 /* OUT: array of returned BDD roots */
  )
{

  int retValue;

#if 0
#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  int retValueBis;

  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During ADD Load.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During ADD Load.\n");
      fflush (stderr);
    }
  }
#endif
#endif
#endif

  retValue = DddmpCuddDdArrayLoad (DDDMP_ADD, ddMgr, rootMatchMode,
    rootmatchnames, varMatchMode, varmatchnames, varmatchauxids,
    varcomposeids, mode, file, fp, pproots);

#if 0
#ifdef DDDMP_DEBUG
#ifndef __alpha__  
  retValueBis = Cudd_DebugCheck (ddMgr);
  if (retValueBis == 1) {
    fprintf (stderr, "Inconsistency Found During ADD Load.\n");
    fflush (stderr);
  } else {
    if (retValueBis == CUDD_OUT_OF_MEM) {
      fprintf (stderr, "Out of Memory During ADD Load.\n");
      fflush (stderr);
    }
  }
#endif
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
Dddmp_cuddHeaderLoad (
  Dddmp_DecompType *ddType  /* OUT: selects the proper decomp type */,
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

  Hdr = DddmpBddReadHeader (NULL, fp);

  Dddmp_CheckAndGotoLabel (Hdr->nnodes==0, "Zero number of nodes.",
    failure);

  /*
   *  Type, number of variables (tot and support)
   */

  *ddType = Hdr->ddType;
  *nVars = Hdr->nVars;
  *nsuppvars = Hdr->nsuppvars;

  /*
   *  Support Varnames
   */

  if (Hdr->suppVarNames != NULL) {
    *suppVarNames = DDDMP_ALLOC (char *, *nsuppvars);
    Dddmp_CheckAndGotoLabel (*suppVarNames==NULL,
      "Error allocating memory.", failure);

    for (i=0; i<*nsuppvars; i++) {
      (*suppVarNames)[i] = DDDMP_ALLOC (char,
        (strlen (Hdr->suppVarNames[i]) + 1));
      Dddmp_CheckAndGotoLabel (Hdr->suppVarNames[i]==NULL,
        "Support Variable Name Missing in File.", failure);
      strcpy ((*suppVarNames)[i], Hdr->suppVarNames[i]);
    }
  } else {
    *suppVarNames = NULL;
  }

  /*
   *  Ordered Varnames
   */

  if (Hdr->orderedVarNames != NULL) {
    *orderedVarNames = DDDMP_ALLOC (char *, *nVars);
    Dddmp_CheckAndGotoLabel (*orderedVarNames==NULL,
      "Error allocating memory.", failure);

    for (i=0; i<*nVars; i++) {
      (*orderedVarNames)[i]  = DDDMP_ALLOC (char,
        (strlen (Hdr->orderedVarNames[i]) + 1));
      Dddmp_CheckAndGotoLabel (Hdr->orderedVarNames[i]==NULL,
        "Support Variable Name Missing in File.", failure);
      strcpy ((*orderedVarNames)[i], Hdr->orderedVarNames[i]);
    }
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
	
  DddmpFreeHeader(Hdr);

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

  Synopsis    [Reads a dump file representing the argument BDDs.]

  Description [Reads a dump file representing the argument BDDs. The header is
    common to both text and binary mode. The node list is either 
    in text or binary format. A dynamic vector of DD pointers 
    is allocated to support conversion from DD indexes to pointers.
    Several criteria are supported for variable match between file
    and dd manager. Several changes/permutations/compositions are allowed
    for variables while loading DDs. Variable of the dd manager are allowed 
    to match with variables on file on ids, permids, varnames, 
    varauxids; also direct composition between ids and 
    composeids is supported. More in detail:
    <ol>
    <li> varMatchMode=DDDMP_VAR_MATCHIDS <p>
    allows the loading of a DD keeping variable IDs unchanged
    (regardless of the variable ordering of the reading manager); this
    is useful, for example, when swapping DDs to file and restoring them
    later from file, after possible variable reordering activations.
    
    <li> varMatchMode=DDDMP_VAR_MATCHPERMIDS <p>
    is used to allow variable match according to the position in the ordering.
    
    <li> varMatchMode=DDDMP_VAR_MATCHNAMES <p>
    requires a non NULL varmatchnames parameter; this is a vector of
    strings in one-to-one correspondence with variable IDs of the
    reading manager. Variables in the DD file read are matched with
    manager variables according to their name (a non NULL varnames
    parameter was required while storing the DD file).
    
    <li> varMatchMode=DDDMP_VAR_MATCHIDS <p>
    has a meaning similar to DDDMP_VAR_MATCHNAMES, but integer auxiliary
    IDs are used instead of strings; the additional non NULL
    varmatchauxids parameter is needed.
    
    <li> varMatchMode=DDDMP_VAR_COMPOSEIDS <p>
    uses the additional varcomposeids parameter is used as array of
    variable ids to be composed with ids stored in file.
    </ol>
    
    In the present implementation, the array varnames (3), varauxids (4)
    and composeids (5) need to have one entry for each variable in the 
    DD manager (NULL pointers are allowed for unused variables
    in varnames). Hence variables need to be already present in the 
    manager. All arrays are sorted according to IDs.
    ]

  SideEffects [A vector of pointers to DD nodes is allocated and freed.]

  SeeAlso     [Dddmp_cuddBddArrayStore]

******************************************************************************/

static int
DddmpCuddDdArrayLoad (
  Dddmp_DecompType ddType           /* IN: Selects decomp type */,
  DdManager *ddMgr                  /* IN: DD Manager */,
  Dddmp_RootMatchType rootMatchMode /* IN: storing mode selector */,
  char **rootmatchnames             /* IN: sorted names for loaded roots */,
  Dddmp_VarMatchType varMatchMode   /* IN: storing mode selector */,
  char **varmatchnames              /* IN: array of variable names, by ids */,
  int *varmatchauxids               /* IN: array of variable auxids, by ids */,
  int *varcomposeids                /* IN: array of new ids, by ids */,
  int mode                          /* IN: requested input file format */,
  char *file                        /* IN: file name */,
  FILE *fp                          /* IN: file pointer */,
  DdNode ***pproots                 /* OUT: array BDD roots (by reference) */
  )
{
  Dddmp_Hdr_t *Hdr = NULL;
  DdNode *f = NULL;
  DdNode *T = NULL;
  DdNode *E = NULL;
  struct binary_dd_code code;
  char buf[DDDMP_MAXSTRLEN];
  int retValue, id, size, maxv;
  int i, j, k, maxaux, var, vT, vE, idT, idE;
  double addConstant;
  int *permsupport = NULL;
  int *convertids = NULL;
  int *invconvertids = NULL;
  int *invauxids = NULL;
  char **sortedvarnames = NULL;
  int  nddvars, nRoots;
  DdNode **pnodes = NULL;
  unsigned char *pvars1byte = NULL;
  unsigned short *pvars2byte = NULL;
  DdNode **proots = NULL;
  int fileToClose = 0;

  *pproots = NULL;

  if (fp == NULL) {
    fp = fopen (file, "r");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  nddvars = ddMgr->size;

  Hdr = DddmpBddReadHeader (NULL, fp);

  Dddmp_CheckAndGotoLabel (Hdr->nnodes==0, "Zero number of nodes.",
    failure);

  nRoots = Hdr->nRoots;

  if (Hdr->ddType != ddType) {
    (void) fprintf (stderr, "DdLoad Error: ddType mismatch\n");
 
   if (Hdr->ddType == DDDMP_BDD)
      (void) fprintf (stderr, "BDD found\n");
    if (Hdr->ddType == DDDMP_ADD)
      (void) fprintf (stderr, "ADD found\n");
    if (ddType == DDDMP_BDD)
      (void) fprintf (stderr, "when loading a BDD\n");
    if (ddType == DDDMP_ADD)
      (void) fprintf (stderr, "when loading an ADD\n");

    fflush (stderr);
    goto failure;
  }

  if (Hdr->mode != mode) {
    Dddmp_CheckAndGotoLabel (mode!=DDDMP_MODE_DEFAULT,
      "Mode Mismatch.", failure);
    mode = Hdr->mode;
  }

  /*
   *  For each variable in the support
   *  compute the relative position in the ordering
   *  (within the support only)
   */

  permsupport = DDDMP_ALLOC (int, Hdr->nsuppvars);
  Dddmp_CheckAndGotoLabel (permsupport==NULL, "Error allocating memory.",
    failure);
  for (i=0,k=0; i < Hdr->nVars; i++) { 
    for (j=0; j < Hdr->nsuppvars; j++) { 
      if (Hdr->permids[j] == i) {
        permsupport[j] = k++;
      }
    }
  }
  Dddmp_Assert (k==Hdr->nsuppvars, "k==Hdr->nsuppvars");

  if (Hdr->suppVarNames != NULL) {
    /*
     *  Varnames are sorted for binary search
     */

    sortedvarnames = DDDMP_ALLOC(char *, Hdr->nsuppvars);
    Dddmp_CheckAndGotoLabel (sortedvarnames==NULL, "Error allocating memory.",
      failure);
    for (i=0; i<Hdr->nsuppvars; i++) {
      Dddmp_CheckAndGotoLabel (Hdr->suppVarNames[i]==NULL,
        "Support Variable Name Missing in File.", failure);
      sortedvarnames[i] = Hdr->suppVarNames[i];
    }    
    
    qsort ((void *) sortedvarnames, Hdr->nsuppvars,
      sizeof(char *), QsortStrcmp);
    
  }

  /*
   *  Convertids is the array used to convert variable ids from positional
   *  (shrinked) ids used within the DD file.
   *  Positions in the file are from 0 to nsuppvars-1.
   */ 

  convertids = DDDMP_ALLOC (int, Hdr->nsuppvars);
  Dddmp_CheckAndGotoLabel (convertids==NULL, "Error allocating memory.",
    failure);

  again_matchmode:
  switch (varMatchMode) {
    case DDDMP_VAR_MATCHIDS:
      for (i=0; i<Hdr->nsuppvars; i++) {
        convertids[permsupport[i]] = Hdr->ids[i];
      }
      break;
    case DDDMP_VAR_MATCHPERMIDS:
      for (i=0; i<Hdr->nsuppvars; i++) {
        convertids[permsupport[i]] = Cudd_ReadInvPerm (ddMgr,
          Hdr->permids[i]);
      }
      break;
    case DDDMP_VAR_MATCHAUXIDS:
      if (Hdr->auxids == NULL) {
        (void) fprintf (stderr,
           "DdLoad Error: variable auxids matching requested\n");
        (void) fprintf (stderr, "but .auxids not found in BDD file\n");
        (void) fprintf (stderr, "Matching IDs forced.\n");
        fflush (stderr);
        varMatchMode = DDDMP_VAR_MATCHIDS;
        goto again_matchmode;
      }
      /* find max auxid value to alloc invaux array */
      for (i=0,maxaux= -1; i<nddvars; i++) {
        if (varmatchauxids[i]>maxaux) {
          maxaux = varmatchauxids[i];
        }
      }
      /* generate invaux array */
      invauxids = DDDMP_ALLOC (int, maxaux+1);
      Dddmp_CheckAndGotoLabel (invauxids==NULL, "Error allocating memory.",
        failure);

      for (i=0; i<=maxaux; i++) {
        invauxids[i] = -1;
      }

      for (i=0; i<Hdr->nsuppvars; i++) {
        invauxids[varmatchauxids[Hdr->ids[i]]] = Hdr->ids[i];
      }

      /* generate convertids array */
      for (i=0; i<Hdr->nsuppvars; i++) {
        if ((Hdr->auxids[i]>maxaux) || (invauxids[Hdr->auxids[i]]<0)) {
          (void) fprintf (stderr,
            "DdLoad Error: auxid %d not found in DD manager.\n", 
            Hdr->auxids[i]);
          (void) fprintf (stderr, "ID matching forced (%d).\n", i);
          (void) fprintf (stderr,
            "Beware of possible overlappings with other variables\n"); 
          fflush (stderr);
          convertids[permsupport[i]] = i;
        } else {
          convertids[permsupport[i]] = invauxids[Hdr->auxids[i]];
        }
      }
      break;
    case DDDMP_VAR_MATCHNAMES:
      if (Hdr->suppVarNames == NULL) {
        (void) fprintf (stderr,
          "DdLoad Error: variable names matching requested\n");
        (void) fprintf (stderr, "but .suppvarnames not found in BDD file\n");
        (void) fprintf (stderr, "Matching IDs forced.\n");
        fflush (stderr);
        varMatchMode = DDDMP_VAR_MATCHIDS;
        goto again_matchmode;
      }

      /* generate invaux array */
      invauxids = DDDMP_ALLOC (int, Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (invauxids==NULL, "Error allocating memory.",
        failure);

      for (i=0; i<Hdr->nsuppvars; i++) {
        invauxids[i] = -1;
      }

      for (i=0; i<nddvars; i++) {
        if (varmatchnames[i]==NULL) {
          (void) fprintf (stderr,
            "DdLoad Warning: NULL match variable name (id: %d). Ignored.\n",
            i);
          fflush (stderr);
        }
        else
          if ((j=FindVarname(varmatchnames[i],sortedvarnames,Hdr->nsuppvars))
               >=0) {
            Dddmp_Assert (j<Hdr->nsuppvars, "j<Hdr->nsuppvars");
            invauxids[j] = i;
          }
      }
      /* generate convertids array */
      for (i=0; i<Hdr->nsuppvars; i++) {
        Dddmp_Assert (Hdr->suppVarNames[i]!=NULL,
          "Hdr->suppVarNames[i] != NULL");
        j=FindVarname(Hdr->suppVarNames[i],sortedvarnames,Hdr->nsuppvars);
        Dddmp_Assert ((j>=0) && (j<Hdr->nsuppvars),
          "(j>=0) && (j<Hdr->nsuppvars)");
        if (invauxids[j]<0) {
          fprintf (stderr,
            "DdLoad Error: varname %s not found in DD manager.",
             Hdr->suppVarNames[i]);
          fprintf (stderr, "ID matching forced (%d)\n", i);
          fflush (stderr);
          convertids[permsupport[i]]=i;
        } else {
          convertids[permsupport[i]] = invauxids[j];
        }
      }
      break;
    case DDDMP_VAR_COMPOSEIDS:
      for (i=0; i<Hdr->nsuppvars; i++) {
        convertids[permsupport[i]] = varcomposeids[Hdr->ids[i]];
      }
      break;
  }

  maxv = (-1);
  for (i=0; i<Hdr->nsuppvars; i++) {
    if (convertids[i] > maxv) {
      maxv = convertids[i];
    }
  }
 
  invconvertids = DDDMP_ALLOC (int, maxv+1);
  Dddmp_CheckAndGotoLabel (invconvertids==NULL, "Error allocating memory.",
    failure);

  for (i=0; i<=maxv; i++) {
    invconvertids[i]= -1;
  }

  for (i=0; i<Hdr->nsuppvars; i++) {
    invconvertids[convertids[i]] = i;
  }

  pnodes = DDDMP_ALLOC(DdNode *,(Hdr->nnodes+1));
  Dddmp_CheckAndGotoLabel (pnodes==NULL, "Error allocating memory.",
    failure);

  if (Hdr->nsuppvars < 256) {
    pvars1byte = DDDMP_ALLOC(unsigned char,(Hdr->nnodes+1));
    Dddmp_CheckAndGotoLabel (pvars1byte==NULL, "Error allocating memory.",
      failure);
  }
  else if (Hdr->nsuppvars < 0xffff) {
    pvars2byte = DDDMP_ALLOC(unsigned short,(Hdr->nnodes+1));
    Dddmp_CheckAndGotoLabel (pvars2byte==NULL, "Error allocating memory.",
      failure);
  } else {
    (void) fprintf (stderr, 
       "DdLoad Error: more than %d variables. Not supported.\n", 0xffff);
    fflush (stderr);
    goto failure;
  }

  /*-------------- Deal With Nodes ... One Row File at a Time --------------*/
 
  for (i=1; i<=Hdr->nnodes; i++) {

    Dddmp_CheckAndGotoLabel (feof(fp),
      "Unexpected EOF While Reading DD Nodes.", failure);

    switch (mode) {

      /*
       *  Text FORMAT
       */

      case DDDMP_MODE_TEXT:

        switch (Hdr->varinfo) {
          case DDDMP_VARIDS:
          case DDDMP_VARPERMIDS:
          case DDDMP_VARAUXIDS:
          case DDDMP_VARNAMES:
            retValue = fscanf(fp, "%d %*s %s %d %d\n", &id, buf, &idT, &idE); 
            Dddmp_CheckAndGotoLabel (retValue<4,
              "Error Reading Nodes in Text Mode.", failure);
            break;
          case DDDMP_VARDEFAULT:
            retValue = fscanf(fp, "%d %s %d %d\n", &id, buf, &idT, &idE);
            Dddmp_CheckAndGotoLabel (retValue<4,
              "Error Reading Nodes in Text Mode.", failure);
            break;
        }
#ifdef DDDMP_DEBUG
        Dddmp_Assert (id==i, "id == i");
#endif
        if (idT==0 && idE==0) {
          /* leaf node: a constant */
          if (strcmp(buf, "1") == 0) {
            pnodes[i] = Cudd_ReadOne (ddMgr);       
          } else {
            /* this is an ADD constant ! */
            if (strcmp(buf, "0") == 0) {
              pnodes[i] = Cudd_ReadZero (ddMgr);       
            } else {
              addConstant = atof(buf);
              pnodes[i] = Cudd_addConst (ddMgr,
                (CUDD_VALUE_TYPE) addConstant);
            }
          }

          /* StQ 11.02.2004:
             Bug fixed --> Reference All Nodes for ADD */
          Cudd_Ref (pnodes[i]);       
          Dddmp_CheckAndGotoLabel (pnodes[i]==NULL, "NULL pnodes.",
            failure);
          continue;
        } else {
#ifdef DDDMP_DEBUG
          Dddmp_Assert (idT>0, "id > 0");
#endif
          var = atoi(buf);
          T = pnodes[idT];
          if(idE<0) {
            idE = -idE;
            E = pnodes[idE];
            E = Cudd_Not(E);
          } else {
            E = pnodes[idE];
          }
        }

        break;

      /*
       *  Binary FORMAT
       */

      case DDDMP_MODE_BINARY:

        Dddmp_CheckAndGotoLabel (DddmpReadCode(fp,&code) == 0,
          "Error Reading witn ReadCode.", failure);

        switch (code.V) {
        case DDDMP_TERMINAL:     
          /* only 1 terminal presently supported */    
          pnodes[i] = Cudd_ReadOne (ddMgr);       
          continue; 
          break;
        case DDDMP_RELATIVE_1:
          break;
        case DDDMP_RELATIVE_ID:
        case DDDMP_ABSOLUTE_ID:
          size = DddmpReadInt (fp, &var);
          Dddmp_CheckAndGotoLabel (size==0, "Error reading size.",
            failure);
          break;
        }

        switch (code.T) {
        case DDDMP_TERMINAL:     
          idT = 1;
          break;
        case DDDMP_RELATIVE_1:
          idT = i-1;
          break;
        case DDDMP_RELATIVE_ID:
          size = DddmpReadInt (fp, &id);
          Dddmp_CheckAndGotoLabel (size==0, "Error reading size.",
            failure);
          idT = i-id;
          break;
        case DDDMP_ABSOLUTE_ID:
          size = DddmpReadInt (fp, &idT);
          Dddmp_CheckAndGotoLabel (size==0, "Error reading size.",
            failure);
          break;
        }

        switch (code.E) {
        case DDDMP_TERMINAL:     
          idE = 1;
          break;
        case DDDMP_RELATIVE_1:
          idE = i-1;
          break;
        case DDDMP_RELATIVE_ID:
          size = DddmpReadInt (fp, &id);
          Dddmp_CheckAndGotoLabel (size==0, "Error reading size.",
            failure);
          idE = i-id;
          break;
        case DDDMP_ABSOLUTE_ID:
          size = DddmpReadInt (fp, &idE);
          Dddmp_CheckAndGotoLabel (size==0, "Error reading size.",
            failure);
          break;
        }

#ifdef DDDMP_DEBUG
      Dddmp_Assert (idT<i, "id<i");
#endif
      T = pnodes[idT];
      if (cuddIsConstant(T))
        vT = Hdr->nsuppvars;
      else {
        if (pvars1byte != NULL)
          vT = pvars1byte[idT];
        else if (pvars2byte != NULL)
          vT = pvars2byte[idT];
        else
          vT = invconvertids[T->index];
      }
#ifdef DDDMP_DEBUG
      Dddmp_Assert (vT>0, "vT > 0");
      Dddmp_Assert (vT<=Hdr->nsuppvars, "vT <= Hdr->nsuppvars");
#endif

#ifdef DDDMP_DEBUG
      Dddmp_Assert (idE<i, "idE < i");
#endif
      E = pnodes[idE];
      if (cuddIsConstant(E))
        vE = Hdr->nsuppvars;
      else {
        if (pvars1byte != NULL)
          vE = pvars1byte[idE];
        else if (pvars2byte != NULL)
          vE = pvars2byte[idE];
        else
          vE = invconvertids[E->index];
      }
#ifdef DDDMP_DEBUG
      Dddmp_Assert (vE>0, "vE > 0");
      Dddmp_Assert (vE<=Hdr->nsuppvars, "vE <= Hdr->nsuppvars");
#endif
  
      switch (code.V) {
        case DDDMP_TERMINAL:     
        case DDDMP_ABSOLUTE_ID:
          break;
        case DDDMP_RELATIVE_1:
          var = (vT<vE) ? vT-1 : vE-1;
          break;
        case DDDMP_RELATIVE_ID:
          var = (vT<vE) ? vT-var : vE-var;
          break;
      }

      if (code.Ecompl) {
        E = Cudd_Not(E);
      }

#ifdef DDDMP_DEBUG
      Dddmp_Assert (var<Hdr->nsuppvars, "var < Hdr->nsuppvars");
#endif

      break;
    }

    if (pvars1byte != NULL) {
      pvars1byte[i] = (unsigned char) var;
    } else {
      if (pvars2byte != NULL) {
        pvars2byte[i] = (unsigned short) var;
      }
    }

    var = convertids[var];
    switch (ddType) {
      case DDDMP_BDD: 
        pnodes[i] = Cudd_bddIte (ddMgr, Cudd_bddIthVar (ddMgr, var),
          T, E);
        break;
      case DDDMP_ADD: 
        { 
        DdNode *tmp = Cudd_addIthVar (ddMgr, var);
        Cudd_Ref (tmp);
        pnodes[i] = Cudd_addIte (ddMgr, tmp, T, E);
        Cudd_RecursiveDeref (ddMgr, tmp);
        break;
        }
      case DDDMP_CNF:
      case DDDMP_NONE:
        Dddmp_Warning (1, "Wrong DD Type.");
        break;
     }

    cuddRef (pnodes[i]);
  }

  /*------------------------ Deal With the File Tail -----------------------*/

  fgets (buf, DDDMP_MAXSTRLEN-1,fp);
  Dddmp_CheckAndGotoLabel (!matchkeywd(buf, ".end"),
    "Error .end not found.", failure);

  /* Close File IFF Necessary */
  if (fileToClose) {
    fclose (fp);
  }

  /* BDD Roots */
  proots = DDDMP_ALLOC(DdNode *,nRoots);
  Dddmp_CheckAndGotoLabel (proots==NULL, "Error allocating memory.",
    failure);

  for(i=0; i<nRoots; ++i) {
    switch (rootMatchMode) {
      case DDDMP_ROOT_MATCHNAMES:
        for (j=0; j<nRoots; j++) {
          if (strcmp(rootmatchnames[i], Hdr->rootnames[j]) == 0)
            break;
        }
        if (j>=nRoots) {
          /* rootname not found */
          fprintf (stderr, "Warning: unable to match root name <%s>\n",
            rootmatchnames[i]);
        }
        break; 
      case DDDMP_ROOT_MATCHLIST:
        j = i;
        break;
    }

    id = Hdr->rootids[i];
    if (id==0) {
      (void) fprintf (stderr, "DdLoad Warning: NULL root found in file\n");
      fflush (stderr);
      f = NULL;
    } else {
      if (id<0) {
        f = Cudd_Not(pnodes[-id]);
      } else {
        f = pnodes[id];
      }
    }
    proots[i] = f;

    cuddRef (f);
  } /* end for i = 0..nRoots */

  /*
   *  Decrease Reference for all Nodes
   */

  /* StQ 11.02.2004:
     Bug fixed --> De-Reference All Nodes for ADD */
  for (i=1; i<=Hdr->nnodes; i++) {
    f = pnodes[i];
    Cudd_RecursiveDeref (ddMgr, f);
  }

  /*
   *  Free Memory: load_end label
   */

load_end:

  DddmpFreeHeader(Hdr);

  DDDMP_FREE (pnodes);
  DDDMP_FREE (pvars1byte);
  DDDMP_FREE (pvars2byte);

  /* variable names are not freed because they were shared with varnames */
  DDDMP_FREE (sortedvarnames);

  DDDMP_FREE (permsupport);
  DDDMP_FREE (convertids);
  DDDMP_FREE (invconvertids);
  DDDMP_FREE (invauxids);

  *pproots = proots;
  return (nRoots);

  /*
   *  Failure Condition
   */

failure:

  if (fileToClose) {
    fclose (fp);
  }

  nRoots = 0; /* return 0 on error ! */

  DDDMP_FREE (proots);

  goto load_end; /* this is done to free memory */
}

/**Function********************************************************************

  Synopsis    [Reads a the header of a dump file representing the
    argument BDDs.
    ]

  Description [Reads the header of a dump file. Builds a Dddmp_Hdr_t struct
    containing all infos in the header, for next manipulations.
    ]

  SideEffects [none]

  SeeAlso     []

******************************************************************************/

static Dddmp_Hdr_t *
DddmpBddReadHeader (
  char *file	    /* IN: file name */,
  FILE *fp          /* IN: file pointer */
  )
{
  Dddmp_Hdr_t *Hdr = NULL;
  char buf[DDDMP_MAXSTRLEN];
  int retValue, fileToClose = 0;

  if (fp == NULL) {
    fp = fopen (file, "r");
    Dddmp_CheckAndGotoLabel (fp==NULL, "Error opening file.",
      failure);
    fileToClose = 1;
  }

  /* START HEADER */

  Hdr = DDDMP_ALLOC (Dddmp_Hdr_t,1);
  if (Hdr == NULL) {
    return NULL;
  }
  Hdr->ver = NULL;
  Hdr->mode = 0;
  Hdr->ddType = DDDMP_BDD;
  Hdr->varinfo = DDDMP_VARIDS;
  Hdr->dd = NULL;
  Hdr->nnodes = 0;
  Hdr->nVars = 0;
  Hdr->nsuppvars = 0;
  Hdr->suppVarNames = NULL;
  Hdr->orderedVarNames = NULL;
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

  while (fscanf(fp, "%s", buf)!=EOF) {

    /* comment */
    if (buf[0] == '#') {
      fgets(buf,DDDMP_MAXSTRLEN,fp);
      continue;
    }

    Dddmp_CheckAndGotoLabel (buf[0] != '.',
      "Error; line must begin with '.' or '#'.",
        failure);

    if (matchkeywd(buf, ".ver")) {    
      /* this not checked so far: only read */
      retValue = fscanf (fp, "%s", buf);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading from file.",
        failure);

      Hdr->ver=DddmpStrDup(buf);
      Dddmp_CheckAndGotoLabel (Hdr->ver==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".add")) {    
      Hdr->ddType = DDDMP_ADD;
      continue;
    }

    if (matchkeywd(buf, ".bdd")) {    
      Hdr->ddType = DDDMP_BDD;
      continue;
    }

    if (matchkeywd(buf, ".mode")) {    
      retValue = fscanf (fp, "%s", buf);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading to file.",
        failure);

      Hdr->mode = buf[0];
      continue;
    }

    if (matchkeywd(buf, ".varinfo")) {
      int readMe;
      retValue = fscanf (fp, "%d", &readMe);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);
      Hdr->varinfo = (Dddmp_VarInfoType) readMe;

      continue;
    }

    if (matchkeywd(buf, ".dd")) {    
      retValue = fscanf (fp, "%s", buf);
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      Hdr->dd = DddmpStrDup (buf);
      Dddmp_CheckAndGotoLabel (Hdr->dd==NULL, "Error allocating memory.",
        failure);

      continue;
    }

    if (matchkeywd(buf, ".nnodes")) {
      retValue = fscanf (fp, "%d", &(Hdr->nnodes));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd(buf, ".nvars")) {   
      retValue = fscanf (fp, "%d", &(Hdr->nVars));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd(buf, ".nsuppvars")) {
      retValue = fscanf (fp, "%d", &(Hdr->nsuppvars));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd(buf, ".orderedvarnames")) {
      Hdr->orderedVarNames = DddmpStrArrayRead (fp, Hdr->nVars);
      Dddmp_CheckAndGotoLabel (Hdr->orderedVarNames==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".suppvarnames") ||
      ((strcmp (Hdr->ver, "DDDMP-1.0") == 0) &&
      matchkeywd (buf, ".varnames"))) {
      Hdr->suppVarNames = DddmpStrArrayRead (fp, Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->suppVarNames==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if matchkeywd(buf, ".ids") {
      Hdr->ids = DddmpIntArrayRead(fp,Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->ids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".permids")) {
      Hdr->permids = DddmpIntArrayRead(fp,Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->permids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".auxids")) {
      Hdr->auxids = DddmpIntArrayRead(fp,Hdr->nsuppvars);
      Dddmp_CheckAndGotoLabel (Hdr->auxids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".nroots")) {
      retValue = fscanf (fp, "%d", &(Hdr->nRoots));
      Dddmp_CheckAndGotoLabel (retValue==EOF, "Error reading file.",
        failure);

      continue;
    }

    if (matchkeywd(buf, ".rootids")) {
      Hdr->rootids = DddmpIntArrayRead(fp,Hdr->nRoots);
      Dddmp_CheckAndGotoLabel (Hdr->rootids==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".rootnames")) {
      Hdr->rootnames = DddmpStrArrayRead(fp,Hdr->nRoots);
      Dddmp_CheckAndGotoLabel (Hdr->rootnames==NULL,
        "Error allocating memory.", failure);

      continue;
    }

    if (matchkeywd(buf, ".nodes")) {
      fgets(buf,DDDMP_MAXSTRLEN,fp);
      break;
    }

  }

  /* END HEADER */

  return (Hdr);

failure:

  if (fileToClose == 1) {
    fclose (fp);
  }

  DddmpFreeHeader(Hdr);

  return (NULL);
}


/**Function********************************************************************

  Synopsis    [Frees the internal header structure.]

  Description [Frees the internal header structureby freeing all internal
    fields first.
    ]

  SideEffects []

  SeeAlso     []

******************************************************************************/

static void
DddmpFreeHeader (
  Dddmp_Hdr_t *Hdr   /* IN: pointer to header */
  )
{
  DDDMP_FREE (Hdr->ver);
  DDDMP_FREE (Hdr->dd);
  DddmpStrArrayFree (Hdr->orderedVarNames, Hdr->nVars);
  DddmpStrArrayFree (Hdr->suppVarNames, Hdr->nsuppvars);
  DDDMP_FREE (Hdr->ids);
  DDDMP_FREE (Hdr->permids);
  DDDMP_FREE (Hdr->auxids);
  DDDMP_FREE (Hdr->rootids);
  DddmpStrArrayFree (Hdr->rootnames, Hdr->nRoots);

  DDDMP_FREE (Hdr);

  return;
}



