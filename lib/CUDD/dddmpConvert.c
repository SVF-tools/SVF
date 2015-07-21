/**CFile**********************************************************************

  FileName     [dddmpConvert.c]

  PackageName  [dddmp]

  Synopsis     [Conversion between ASCII and binary formats]

  Description  [Conversion between ASCII and binary formats is presently 
    supported by loading a BDD in the source format and storing it 
    in the target one. We plan to introduce ad hoc procedures
    avoiding explicit BDD node generation.
    ]

  Author       [Gianpiero Cabodi and Stefano Quer]

  Copyright   [
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


/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Converts from ASCII to binary format]

  Description [Converts from ASCII to binary format. A BDD array is loaded and
    and stored to the target file.]

  SideEffects [None]

  SeeAlso     [Dddmp_Bin2Text()]

******************************************************************************/

int
Dddmp_Text2Bin (
  char *filein   /* IN: name of ASCII file */,
  char *fileout  /* IN: name of binary file */
  )
{
  DdManager *ddMgr;      /* pointer to DD manager */
  DdNode **roots;        /* array of BDD roots to be loaded */
  int nRoots;            /* number of BDD roots */
  int retValue;
 
  ddMgr = Cudd_Init(0,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0);
  if (ddMgr == NULL) {
    return (0);
  }

  nRoots = Dddmp_cuddBddArrayLoad(ddMgr,DDDMP_ROOT_MATCHLIST,NULL,
    DDDMP_VAR_MATCHIDS,NULL,NULL,NULL,
    DDDMP_MODE_TEXT,filein,NULL,&roots);

  Dddmp_CheckAndGotoLabel (nRoots<=0,
    "Negative Number of Roots.", failure);

  retValue = Dddmp_cuddBddArrayStore (ddMgr,NULL,nRoots,roots,NULL,
    NULL,NULL,DDDMP_MODE_BINARY,DDDMP_VARIDS,fileout,NULL);

  Dddmp_CheckAndGotoLabel (retValue<=0,
    "Error code returned.", failure);
   
  Cudd_Quit(ddMgr);
  return (1);

  failure:
    printf("error converting BDD format\n");
    Cudd_Quit(ddMgr);
    return (0);
}

/**Function********************************************************************

  Synopsis    [Converts from binary to ASCII format]

  Description [Converts from binary to ASCII format. A BDD array is loaded and
    and stored to the target file.]

  SideEffects [None]

  SeeAlso     [Dddmp_Text2Bin()]

******************************************************************************/

int
Dddmp_Bin2Text (
  char *filein   /* IN: name of binary file */,
  char *fileout  /* IN: name of ASCII file */
  )
{
  DdManager *ddMgr;      /* pointer to DD manager */
  DdNode **roots;        /* array of BDD roots to be loaded */
  int nRoots;            /* number of BDD roots */
  int retValue;

  ddMgr = Cudd_Init(0,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0);
  if (ddMgr == NULL) {
    return (0);
  }

  nRoots = Dddmp_cuddBddArrayLoad(ddMgr,DDDMP_ROOT_MATCHLIST,NULL,
    DDDMP_VAR_MATCHIDS,NULL,NULL,NULL,
    DDDMP_MODE_BINARY,filein,NULL,&roots);

  Dddmp_CheckAndGotoLabel (nRoots<=0,
    "Negative Number of Roots.", failure);

  retValue = Dddmp_cuddBddArrayStore (ddMgr,NULL,nRoots,roots,NULL,
    NULL,NULL,DDDMP_MODE_TEXT,DDDMP_VARIDS,fileout,NULL);

  Dddmp_CheckAndGotoLabel (retValue<=0,
    "Error code returned.", failure);
   
  Cudd_Quit(ddMgr);
  return (1);

  failure:
    printf("error converting BDD format\n");
    Cudd_Quit(ddMgr);
    return (0);
}

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/


