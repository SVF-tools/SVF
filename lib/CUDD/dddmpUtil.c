/**CFile**********************************************************************

  FileName     [dddmpUtil.c]

  PackageName  [dddmp]

  Synopsis     [Util Functions for the dddmp package]

  Description  [Functions to manipulate arrays.]

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


/**AutomaticEnd***************************************************************/

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [String compare for qsort]

  Description [String compare for qsort]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

int
QsortStrcmp(
  const void *ps1   /* IN: pointer to the first string */,
  const void *ps2   /* IN: pointer to the second string */
  )
{
  return (strcmp (*((char**)ps1),*((char **)ps2)));
}

/**Function********************************************************************

  Synopsis    [Performs binary search of a name within a sorted array]

  Description [Binary search of a name within a sorted array of strings.
    Used when matching names of variables.
    ]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

int
FindVarname (
  char *name   /* IN: name to look for */,
  char **array /* IN: search array */,
  int n        /* IN: size of the array */
  )
{
  int d, m, u, t;

  d = 0; u = n-1;

  while (u>=d) {
    m = (u+d)/2;
    t=strcmp(name,array[m]);
    if (t==0)
      return m;
    if (t<0)
      u=m-1;
    else
      d=m+1;
  }

  return (-1);
}


/**Function********************************************************************

  Synopsis    [Duplicates a string]

  Description [Allocates memory and copies source string] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

char *
DddmpStrDup (
  char *str   /* IN: string to be duplicated */
  )
{
  char *str2;

  str2 = DDDMP_ALLOC(char,strlen(str)+1);
  if (str2 != NULL) {
    strcpy (str2,str);
  }

  return (str2);
}

/**Function********************************************************************

  Synopsis    [Duplicates an array of strings]

  Description [Allocates memory and copies source array] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

char **
DddmpStrArrayDup (
  char **array    /* IN: array of strings to be duplicated */,
  int n           /* IN: size of the array */
  )
{
  char **array2;
  int i;

  array2 = DDDMP_ALLOC(char *, n);
  if (array2 == NULL) {
    (void) fprintf (stderr, "DddmpStrArrayDup: Error allocating memory\n");
    fflush (stderr);
    return NULL;
  }

  /*
   * initialize all slots to NULL for fair FREEing in case of failure
   */

  for (i=0; i<n; i++) {
    array2[i] = NULL;
  }

  for (i=0; i<n; i++) { 
    if (array[i] != NULL) {
      if ((array2[i]=DddmpStrDup(array[i]))==NULL) {
        DddmpStrArrayFree (array2, n);
        return (NULL);
      }
    }
  }

  return (array2);
}

/**Function********************************************************************

  Synopsis    [Inputs an array of strings]

  Description [Allocates memory and inputs source array] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

char **
DddmpStrArrayRead (
  FILE *fp         /* IN: input file */,
  int n            /* IN: size of the array */
  )
{
  char buf[DDDMP_MAXSTRLEN];
  char **array;
  int i;

  assert(fp!=NULL);

  array = DDDMP_ALLOC(char *, n);
  if (array == NULL) {
    (void) fprintf (stderr, "DddmpStrArrayRead: Error allocating memory\n");
    fflush (stderr);
    return NULL;
  }

  /*
   * initialize all slots to NULL for fair FREEing in case of failure
   */
  for (i=0; i<n; i++) 
    array[i] = NULL;

  for (i=0; i < n; i++) { 
    if (fscanf (fp, "%s", buf)==EOF) {
      fprintf (stderr, "DddmpStrArrayRead: Error reading file - EOF found\n");
      fflush (stderr);
      DddmpStrArrayFree (array, n);
      return (NULL);
    }
    if ((array[i]=DddmpStrDup(buf))==NULL) {
      DddmpStrArrayFree (array, n);
      return (NULL);
    }
  }

  return (array);
}

/**Function********************************************************************

  Synopsis    [Outputs an array of strings]

  Description [Outputs an array of strings to a specified file] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

int
DddmpStrArrayWrite (
  FILE *fp          /* IN: output file */,
  char **array      /* IN: array of strings */,
  int n             /* IN: size of the array */
  )
{
  int i;

  assert(fp!=NULL);

  for (i=0; i<n; i++) { 
    if (fprintf (fp, " %s", array[i]) == EOF) {
      fprintf (stderr, "DddmpStrArrayWrite: Error writing to file\n");
      fflush (stderr);
      return (EOF);
    }
  }

  return (n);
}


/**Function********************************************************************

  Synopsis    [Frees an array of strings]

  Description [Frees memory for strings and the array of pointers] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

void
DddmpStrArrayFree (
  char **array      /* IN: array of strings */,
  int n             /* IN: size of the array */
  )
{
  int i;

  if (array == NULL) {
    return;
  }

  for (i=0; i<n; i++) {
    DDDMP_FREE (array[i]);
  }

  DDDMP_FREE (array);

  return;
}

/**Function********************************************************************

  Synopsis    [Duplicates an array of ints]

  Description [Allocates memory and copies source array] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

int *
DddmpIntArrayDup (
  int *array      /* IN: array of ints to be duplicated */,
  int n           /* IN: size of the array */
  )
{
  int *array2;
  int i;

  array2 = DDDMP_ALLOC(int, n);
  if (array2 == NULL) {
    (void) fprintf (stderr, "DddmpIntArrayDup: Error allocating memory\n");
    fflush (stderr);
    return (NULL);
  }

  for (i=0; i<n; i++) { 
    array2[i] = array[i];
  }

  return (array2);
}


/**Function********************************************************************

  Synopsis    [Inputs an array of ints]

  Description [Allocates memory and inputs source array] 

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

int *
DddmpIntArrayRead (
  FILE *fp          /* IN: input file */,
  int n             /* IN: size of the array */
  )
{
  int *array;
  int i;

  assert(fp!=NULL);

  array = DDDMP_ALLOC(int, n);
  if (array == NULL) {
    (void) fprintf (stderr, "DddmpIntArrayRead: Error allocating memory\n");
    fflush (stderr);
    return NULL;
  }

  for (i=0; i < n; i++) { 
    if (fscanf (fp, "%d", &array[i])==EOF) {
      (void) fprintf (stderr,
        "DddmpIntArrayRead: Error reading file - EOF found\n");
      fflush (stderr);
      DDDMP_FREE (array);
      return (NULL);
    }
  }

  return (array);
}

/**Function********************************************************************

  Synopsis    [Outputs an array of ints]

  Description [Outputs an array of ints to a specified file]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/

int
DddmpIntArrayWrite (
  FILE *fp          /* IN: output file */,
  int  *array       /* IN: array of ints */,
  int n              /* IN: size of the array */
  )
{
  int i;

  assert(fp!=NULL);

  for (i=0; i<n; i++) { 
    if (fprintf (fp, " %d", array[i]) == EOF) {
      (void) fprintf (stderr, "DddmpIntArrayWrite: Error writing to file\n");
      fflush (stderr);
      return EOF;
    }
  }

  return (n);
}

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

