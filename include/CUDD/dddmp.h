/**CHeaderFile*****************************************************************

  FileName     [dddmp.h]

  PackageName  [dddmp]

  Synopsis     [Functions to read in and write out BDDs, ADDs
    and CNF formulas from and to files.]

  Description  []

  Author       [Gianpiero Cabodi and Stefano Quer]

  Copyright    [
    Copyright (c) 2002 by Politecnico di Torino.
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

#ifndef _DDDMP
#define _DDDMP

#if 0
#define DDDMP_DEBUG
#endif

/*---------------------------------------------------------------------------*/
/* Nested includes                                                           */
/*---------------------------------------------------------------------------*/
     
#include "util.h"
#include "cudd.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/* 
 *  Dddmp format version 
 */

#define DDDMP_VERSION           "DDDMP-2.0"

/* 
 *  Returned values (for theorically ALL the function of the package)
 */

#define DDDMP_FAILURE ((int) 0)
#define DDDMP_SUCCESS ((int) 1)

/* 
 *  Format modes for DD (BDD and ADD) files
 */

#define DDDMP_MODE_TEXT           ((int)'A')
#define DDDMP_MODE_BINARY         ((int)'B')
#define DDDMP_MODE_DEFAULT        ((int)'D')

/*---------------------------------------------------------------------------*/
/* Structure declarations                                                    */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/**Enum************************************************************************

  Synopsis    [Format modes for storing CNF files]

  Description [Type supported for storing BDDs into CNF
    formulas.
    Used internally to select the proper storing format:
      DDDMP_CNF_MODE_NODE: create a CNF temporary variables for
                           each BDD node
      DDDMP_CNF_MODE_MAXTERM: no temporary variables
      DDDMP_CNF_MODE_BEST: trade-off between the two previous methods
    ]

******************************************************************************/

typedef enum {
  DDDMP_CNF_MODE_NODE,
  DDDMP_CNF_MODE_MAXTERM,
  DDDMP_CNF_MODE_BEST
} Dddmp_DecompCnfStoreType;

/**Enum************************************************************************

  Synopsis    [Format modes for loading CNF files.]

  Description [Type supported for loading CNF formulas into BDDs.
    Used internally to select the proper returning format:
    ]

******************************************************************************/

typedef enum {
  DDDMP_CNF_MODE_NO_CONJ,
  DDDMP_CNF_MODE_NO_QUANT,
  DDDMP_CNF_MODE_CONJ_QUANT
} Dddmp_DecompCnfLoadType;

/**Enum************************************************************************

  Synopsis    [Type for supported decomposition types.]

  Description [Type for supported decomposition types.
    Used internally to select the proper type (bdd, add, ...).
    Given externally as information fule content.
    ]

******************************************************************************/

typedef enum {
  DDDMP_BDD,
  DDDMP_ADD,
  DDDMP_CNF,
  DDDMP_NONE
} Dddmp_DecompType;


/**Enum************************************************************************

  Synopsis    [Type for variable extra info.]

  Description [Type for variable extra info. Used to specify info stored
    in text mode.]

******************************************************************************/

typedef enum {
  DDDMP_VARIDS,
  DDDMP_VARPERMIDS,
  DDDMP_VARAUXIDS,
  DDDMP_VARNAMES,
  DDDMP_VARDEFAULT
} Dddmp_VarInfoType;

/**Enum************************************************************************

  Synopsis    [Type for variable matching in BDD load.]

  Description []

******************************************************************************/

typedef enum {
  DDDMP_VAR_MATCHIDS,
  DDDMP_VAR_MATCHPERMIDS,
  DDDMP_VAR_MATCHAUXIDS,
  DDDMP_VAR_MATCHNAMES,
  DDDMP_VAR_COMPOSEIDS
} Dddmp_VarMatchType;

/**Enum************************************************************************

  Synopsis    [Type for BDD root matching in BDD load.]

  Description []

******************************************************************************/

typedef enum {
  DDDMP_ROOT_MATCHNAMES,
  DDDMP_ROOT_MATCHLIST
} Dddmp_RootMatchType;

typedef struct Dddmp_Hdr_s Dddmp_Hdr_t;

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**Macro***********************************************************************
 
  Synopsis    [Checks for fatal bugs]
 
  Description [Conditional safety assertion. It prints out the file
    name and line number where the fatal error occurred.
    Messages are printed out on stderr.
    ]
 
  SideEffects [None]
 
  SeeAlso     []
 
******************************************************************************/
 
#ifdef DDDMP_DEBUG
#  define Dddmp_Assert(expr,errMsg) \
     { \
     if ((expr) == 0) { \
       fprintf (stderr, "FATAL ERROR: %s\n", errMsg); \
       fprintf (stderr, "             File %s -> Line %d\n", \
         __FILE__, __LINE__); \
       fflush (stderr); \
       exit (DDDMP_FAILURE); \
       } \
     }
#else
#  define Dddmp_Assert(expr,errMsg) \
     {}
#endif

/**Macro***********************************************************************
 
  Synopsis    [Checks for Warnings: If expr==1 it prints out the warning
    on stderr.]
 
  Description []
 
  SideEffects [None]
 
  SeeAlso     []
 
******************************************************************************/
 
#define Dddmp_Warning(expr,errMsg) \
  { \
  if ((expr) == 1) { \
    fprintf (stderr, "WARNING: %s\n", errMsg); \
    fprintf (stderr, "         File %s -> Line %d\n", \
      __FILE__, __LINE__); \
    fflush (stderr); \
    } \
  }

/**Macro***********************************************************************
 
  Synopsis    [Checks for fatal bugs and return the DDDMP_FAILURE flag.]
 
  Description []
 
  SideEffects [None]
 
  SeeAlso     []
 
******************************************************************************/
 
#define Dddmp_CheckAndReturn(expr,errMsg) \
  { \
  if ((expr) == 1) { \
    fprintf (stderr, "FATAL ERROR: %s\n", errMsg); \
    fprintf (stderr, "             File %s -> Line %d\n", \
      __FILE__, __LINE__); \
    fflush (stderr); \
    return (DDDMP_FAILURE); \
    } \
  }

/**Macro***********************************************************************
 
  Synopsis    [Checks for fatal bugs and go to the label to deal with
    the error.
    ]
 
  Description []
 
  SideEffects [None]
 
  SeeAlso     []
 
******************************************************************************/
 
#define Dddmp_CheckAndGotoLabel(expr,errMsg,label) \
  { \
  if ((expr) == 1) { \
    fprintf (stderr, "FATAL ERROR: %s\n", errMsg); \
    fprintf (stderr, "             File %s -> Line %d\n", \
      __FILE__, __LINE__); \
    fflush (stderr); \
    goto label; \
    } \
  }

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern int Dddmp_Text2Bin(char *filein, char *fileout);
extern int Dddmp_Bin2Text(char *filein, char *fileout);
extern int Dddmp_cuddBddDisplayBinary(char *fileIn, char *fileOut);
extern DdNode * Dddmp_cuddBddLoad(DdManager *ddMgr, Dddmp_VarMatchType varMatchMode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp);
extern int Dddmp_cuddBddArrayLoad(DdManager *ddMgr, Dddmp_RootMatchType rootMatchMode, char **rootmatchnames, Dddmp_VarMatchType varMatchMode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp, DdNode ***pproots);
extern DdNode * Dddmp_cuddAddLoad(DdManager *ddMgr, Dddmp_VarMatchType varMatchMode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp);
extern int Dddmp_cuddAddArrayLoad(DdManager *ddMgr, Dddmp_RootMatchType rootMatchMode, char **rootmatchnames, Dddmp_VarMatchType varMatchMode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp, DdNode ***pproots);
extern int Dddmp_cuddHeaderLoad (Dddmp_DecompType *ddType, int *nVars, int *nsuppvars, char ***suppVarNames, char ***orderedVarNames, int **varIds, int **composeIds, int **auxIds, int *nRoots, char *file, FILE *fp);
extern int Dddmp_cuddBddLoadCnf(DdManager *ddMgr, Dddmp_VarMatchType varmatchmode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp, DdNode ***rootsPtrPtr, int *nRoots);
extern int Dddmp_cuddBddArrayLoadCnf(DdManager *ddMgr, Dddmp_RootMatchType rootmatchmode, char **rootmatchnames, Dddmp_VarMatchType varmatchmode, char **varmatchnames, int *varmatchauxids, int *varcomposeids, int mode, char *file, FILE *fp, DdNode ***rootsPtrPtr, int *nRoots);
extern int Dddmp_cuddHeaderLoadCnf (int *nVars, int *nsuppvars, char ***suppVarNames, char ***orderedVarNames, int **varIds, int **composeIds, int **auxIds, int *nRoots, char *file, FILE *fp);
extern int Dddmp_cuddAddStore(DdManager *ddMgr, char *ddname, DdNode *f, char **varnames, int *auxids, int mode, Dddmp_VarInfoType varinfo, char *fname, FILE *fp);
extern int Dddmp_cuddAddArrayStore(DdManager *ddMgr, char *ddname, int nRoots, DdNode **f, char **rootnames, char **varnames, int *auxids, int mode, Dddmp_VarInfoType varinfo, char *fname, FILE *fp);
extern int Dddmp_cuddBddStore(DdManager *ddMgr, char *ddname, DdNode *f, char **varnames, int *auxids, int mode, Dddmp_VarInfoType varinfo, char *fname, FILE *fp);
extern int Dddmp_cuddBddArrayStore(DdManager *ddMgr, char *ddname, int nRoots, DdNode **f, char **rootnames, char **varnames, int *auxids, int mode, Dddmp_VarInfoType varinfo, char *fname, FILE *fp);
extern int Dddmp_cuddBddStoreCnf(DdManager *ddMgr, DdNode *f, Dddmp_DecompCnfStoreType mode, int noHeader, char **varNames, int *bddIds, int *bddAuxIds, int *cnfIds, int idInitial, int edgeInTh, int pathLengthTh, char *fname, FILE *fp, int *clauseNPtr, int *varNewNPtr);
extern int Dddmp_cuddBddArrayStoreCnf(DdManager *ddMgr, DdNode **f, int rootN, Dddmp_DecompCnfStoreType mode, int noHeader, char **varNames, int *bddIds, int *bddAuxIds, int *cnfIds, int idInitial, int edgeInTh, int pathLengthTh, char *fname, FILE *fp, int *clauseNPtr, int *varNewNPtr);
extern int Dddmp_cuddBddStorePrefix(DdManager *ddMgr, int nRoots, DdNode *f, char **inputNames, char **outputNames, char *modelName, char *fileName, FILE *fp);
extern int Dddmp_cuddBddArrayStorePrefix(DdManager *ddMgr, int nroots, DdNode **f, char **inputNames, char **outputNames, char *modelName, char *fname, FILE *fp);
extern int Dddmp_cuddBddStoreBlif(DdManager *ddMgr, int nRoots, DdNode *f, char **inputNames, char **outputNames, char *modelName, char *fileName, FILE *fp);
extern int Dddmp_cuddBddArrayStoreBlif(DdManager *ddMgr, int nroots, DdNode **f, char **inputNames, char **outputNames, char *modelName, char *fname, FILE *fp);
extern int Dddmp_cuddBddStoreSmv(DdManager *ddMgr, int nRoots, DdNode *f, char **inputNames, char **outputNames, char *modelName, char *fileName, FILE *fp);
extern int Dddmp_cuddBddArrayStoreSmv(DdManager *ddMgr, int nroots, DdNode **f, char **inputNames, char **outputNames, char *modelName, char *fname, FILE *fp);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

#endif
