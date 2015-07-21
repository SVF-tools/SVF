/**CHeaderFile*****************************************************************

  FileName     [dddmpInt.h]

  PackageName  [dddmp]

  Synopsis     [Low level functions to read in and write out bdds to file]

  Description  [A set of internal low-level routines of the dddmp package
    doing:
    <ul>
      <li> read and write of node codes in binary mode,
      <li> read and write of integers in binary mode,
      <li> marking/unmarking nodes as visited,
      <li> numbering nodes.
    </ul>
    ]

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

#ifndef _DDDMPINT
#define _DDDMPINT

#include "dddmp.h"
#include "cuddInt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/* constants for code fields */
#define DDDMP_TERMINAL      0
#define DDDMP_ABSOLUTE_ID   1
#define DDDMP_RELATIVE_ID   2
#define DDDMP_RELATIVE_1    3

#define DDDMP_MAXSTRLEN 500

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Structure declarations                                                     */
/*---------------------------------------------------------------------------*/

/**Struct**********************************************************************
 Synopsis    [used in binary mode to store code info of a dd node]
 Description [V , T , E store the mode used to represent variable, Then 
              and Else indexes. An index is either an absolute
	      ( DDDMP_ABSOLUTE_ID ),
              a relative numbers ( DDDMP_RELATIVE_ID , DDDMP_RELATIVE_1 ) or 
              a terminal node ( DDDMP_TERMINAL ) .
	      Ecomp is used for the complemented edge attribute.
             ]
 SideEffect  [none]
 SeeAlso     [DddmpWriteCode DddmpReadCode] 
******************************************************************************/

struct binary_dd_code {
  unsigned  Unused : 1;
  unsigned  V      : 2;
  unsigned  T      : 2;
  unsigned  Ecompl : 1;
  unsigned  E      : 2;
};

/**Struct*********************************************************************

 Synopsis    [BDD file header]

 Description [Structure containing the BDD header file infos]

******************************************************************************/

struct Dddmp_Hdr_s {
  char *ver;
  char mode;
  Dddmp_DecompType ddType;
  Dddmp_VarInfoType varinfo;
  char *dd;
  int nnodes;
  int nVars;
  int nsuppvars;
  char **orderedVarNames;
  char **suppVarNames;
  int *ids;
  int *permids;
  int *auxids;
  int *cnfids;
  int nRoots;
  int *rootids;
  char **rootnames;
  int nAddedCnfVar;
  int nVarsCnf;
  int nClausesCnf;  
};	

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**Macro***********************************************************************

  Synopsis     [Memory Allocation Macro for DDDMP]

  Description  []

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

#ifdef ALLOC
#  define DDDMP_ALLOC(type, num)	ALLOC(type,num)
#else
#  define DDDMP_ALLOC(type, num)	\
     ((type *) malloc(sizeof(type) * (num)))
#endif

/**Macro***********************************************************************

  Synopsis     [Memory Free Macro for DDDMP]

  Description  []

  SideEffects  [None]

  SeeAlso      []

******************************************************************************/

#ifdef FREE
#define DDDMP_FREE(p)  (FREE(p))
#else
#define DDDMP_FREE(p)	\
    ((p)!=NULL)?(free(p)):0)
#endif


/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern int DddmpWriteCode(FILE *fp, struct binary_dd_code code);
extern int DddmpReadCode(FILE *fp, struct binary_dd_code *pcode);
extern int DddmpWriteInt(FILE *fp, int id);
extern int DddmpReadInt(FILE *fp, int *pid);
extern int DddmpNumberAddNodes(DdManager *ddMgr, DdNode **f, int n);
extern void DddmpUnnumberAddNodes(DdManager *ddMgr, DdNode **f, int n);
extern void DddmpWriteNodeIndexAdd(DdNode *f, int id);
extern int DddmpReadNodeIndexAdd(DdNode *f);
extern int DddmpVisitedAdd(DdNode *f);
extern void DddmpSetVisitedAdd(DdNode *f);
extern void DddmpClearVisitedAdd(DdNode *f);
extern int DddmpNumberBddNodes(DdManager *ddMgr, DdNode **f, int n);
extern void DddmpUnnumberBddNodes(DdManager *ddMgr, DdNode **f, int n);
extern void DddmpWriteNodeIndexBdd(DdNode *f, int id);
extern int DddmpReadNodeIndexBdd(DdNode *f);
extern int DddmpVisitedBdd(DdNode *f);
extern void DddmpSetVisitedBdd(DdNode *f);
extern void DddmpClearVisitedBdd(DdNode *f);
extern int DddmpNumberDdNodesCnf(DdManager *ddMgr, DdNode **f, int rootN, int *cnfIds, int id);
extern int DddmpDdNodesCountEdgesAndNumber(DdManager *ddMgr, DdNode **f, int rootN, int edgeInTh, int pathLengthTh, int *cnfIds, int id);
extern void DddmpUnnumberDdNodesCnf(DdManager *ddMgr, DdNode **f, int rootN);
extern int DddmpPrintBddAndNext(DdManager *ddMgr, DdNode **f, int rootN);
extern int DddmpWriteNodeIndexCnf(DdNode *f, int id);
extern int DddmpVisitedCnf(DdNode *f);
extern void DddmpSetVisitedCnf(DdNode *f);
extern int DddmpReadNodeIndexCnf(DdNode *f);
extern int DddmpCuddDdArrayStoreBdd(Dddmp_DecompType ddType, DdManager *ddMgr, char *ddname, int nRoots, DdNode **f, char **rootnames, char **varnames, int *auxids, int mode, Dddmp_VarInfoType varinfo, char *fname, FILE *fp);
extern int DddmpCuddBddArrayStore(Dddmp_DecompType ddType, DdManager *ddMgr, char *ddname, int nRoots, DdNode **f, char **rootnames, char **varnames, int *auxids, int mode, Dddmp_VarInfoType varinfo, char *fname, FILE *fp);
extern int QsortStrcmp(const void *ps1, const void *ps2);
extern int FindVarname(char *name, char **array, int n);
extern char * DddmpStrDup(char *str);
extern char ** DddmpStrArrayDup(char **array, int n);
extern char ** DddmpStrArrayRead(FILE *fp, int n);
extern int DddmpStrArrayWrite(FILE *fp, char **array, int n);
extern void DddmpStrArrayFree(char **array, int n);
extern int * DddmpIntArrayDup(int *array, int n);
extern int * DddmpIntArrayRead(FILE *fp, int n);
extern int DddmpIntArrayWrite(FILE *fp, int *array, int n);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

#endif
