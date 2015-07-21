/**CFile**********************************************************************

  FileName    [dddmpBinary.c]

  PackageName [dddmp]

  Synopsis    [Input and output BDD codes and integers from/to file]

  Description [Input and output BDD codes and integers from/to file
    in binary mode.
    DD node codes are written as one byte.
    Integers of any length are written as sequences of "linked" bytes.
    For each byte 7 bits are used for data and one (MSBit) as link with
    a further byte (MSB = 1 means one more byte).
    Low level read/write of bytes filter <CR>, <LF> and <ctrl-Z>
    with escape sequences.
    ]

  Author      [Gianpiero Cabodi and Stefano Quer]

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

static int WriteByteBinary(FILE *fp, unsigned char c);
static int ReadByteBinary(FILE *fp, unsigned char *cp);

/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Writes 1 byte node code]

  Description [outputs a 1 byte node code using the following format:
     <pre>
     Unused      : 1 bit;
     V           : 2 bits;     (variable code)
     T           : 2 bits;     (Then code)
     Ecompl      : 1 bit;      (Else complemented)
     E           : 2 bits;     (Else code)
    </pre>
    Ecompl is set with complemented edges.
    ]

  SideEffects [None]

  SeeAlso     [DddmpReadCode()]

******************************************************************************/

int
DddmpWriteCode (
  FILE *fp                   /* IN: file where to write the code */,
  struct binary_dd_code code /* IN: the code to be written */
  )
{
  unsigned char c;
  int retValue;

  c = (code.Unused<<7)|(code.V<<5)|(code.T<<3)|
		(code.Ecompl<<2)|(code.E);

  retValue = WriteByteBinary (fp, c);

  return (retValue);
}

/**Function********************************************************************

  Synopsis    [Reads a 1 byte node code]

  Description [Reads a 1 byte node code. See DddmpWriteCode()
    for code description.]

  SideEffects [None]

  SeeAlso     [DddmpWriteCode()]

******************************************************************************/

int 
DddmpReadCode (
  FILE *fp                     /*  IN: file where to read the code */,
  struct binary_dd_code *pcode /* OUT: the read code */
  )
{
  unsigned char c;

  if (ReadByteBinary (fp, &c) == EOF) {
    return (0);
  }

  pcode->Unused =  c>>7;
  pcode->V      = (c>>5) & 3;
  pcode->T      = (c>>3) & 3;
  pcode->Ecompl = (c>>2) & 1;
  pcode->E      =  c     & 3;

  return (1);
}

/**Function********************************************************************

  Synopsis    [Writes a "packed integer"]

  Description [Writes an integer as a sequence of bytes (MSByte first).
    For each byte 7 bits are used for data and one (LSBit) as link 
    with a further byte (LSB = 1 means one more byte).
    ]

  SideEffects [None]

  SeeAlso     [DddmpReadInt()]

******************************************************************************/

int 
DddmpWriteInt (
  FILE *fp   /* IN: file where to write the integer */,
  int id     /* IN: integer to be written */
  )
{
  char cvet[4];
  int i;

  for (i=0; i<4; i++) {
    cvet[i] = (char)((id & 0x0000007f) << 1);
    id >>= 7;
  }

  for (i=3; (i>0) && (cvet[i] == 0); i--);

  for (; i>0; i--) {
    cvet[i] |= (char)1;
    if (WriteByteBinary (fp, cvet[i]) == EOF)
      return (0);
  }

  if (WriteByteBinary (fp, cvet[0]) == EOF) {
    return (0);
  }

  return (1);
}


/**Function********************************************************************

  Synopsis    [Reads a "packed integer"]

  Description [Reads an integer coded on a sequence of bytes. See
    DddmpWriteInt() for format.]

  SideEffects [None]

  SeeAlso     [DddmpWriteInt()]

******************************************************************************/

int
DddmpReadInt (
  FILE *fp   /*  IN: file where to read the integer */,
  int *pid   /* OUT: the read integer */
  )
{
  unsigned char c;
  int i;
  unsigned int id;

  id = 0;
  for (i=0; i<4; i++) {
    if (ReadByteBinary (fp, &c) == EOF)
      return (0);
    id = (id<<7) | (c>>1);
    if ((c & 1) == 0)
      break;
  }

 /* Check for correct format: last char should 
    be found before i = 4 */
  assert(i<4);

  *pid = id;

  return (i+1);
}

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Writes a byte to file filtering <CR>, <LF> and <ctrl-Z>]

  Description [outputs a byte to file fp. Uses 0x00 as escape character
    to filter <CR>, <LF> and <ctrl-Z>.
    This is done for compatibility between unix and dos/windows systems.
    ]

  SideEffects [None]

  SeeAlso     [ReadByteBinary()]

******************************************************************************/

static int
WriteByteBinary (
  FILE *fp         /* IN: file where to write the byte */,
  unsigned char c  /* IN: the byte to be written */
  )
{
  unsigned char BinaryEscape;
 
  switch (c) {

    case 0x00: /* Escape */
      BinaryEscape = 0x00;
      if (fwrite (&BinaryEscape, sizeof(char), 1, fp) != sizeof(char))
        return (0);
      c = 0x00;
      break;
    case 0x0a: /* <LF> */
      BinaryEscape = 0x00;
      if (fwrite (&BinaryEscape, sizeof(char), 1, fp) != sizeof(char))
        return (0);
      c = 0x01;
      break;
    case 0x0d: /* <CR> */
      BinaryEscape = 0x00;
      if (fwrite (&BinaryEscape, sizeof(char), 1, fp) != sizeof(char))
        return (0);
      c = 0x02;
      break;
    case 0x1a: /* <ctrl-Z> */
      BinaryEscape = 0x00;
      if (fwrite (&BinaryEscape, sizeof(char), 1, fp) != sizeof(char))
        return (0);
      c = 0x03;
      break;
  }
  if (fwrite (&c, sizeof(char), 1, fp) != sizeof(char))
    return (0);

  return (1);
}

/**Function********************************************************************

  Synopsis    [Reads a byte from file with escaped <CR>, <LF> and <ctrl-Z>]

  Description [inputs a byte to file fp. 0x00 has been used as escape character
    to filter <CR>, <LF> and <ctrl-Z>. This is done for
    compatibility between unix and dos/windows systems.
    ]

  SideEffects [None]

  SeeAlso     [WriteByteBinary()]

******************************************************************************/

static int
ReadByteBinary (
  FILE *fp           /*  IN: file where to read the byte */,
  unsigned char *cp  /* OUT: the read byte */
  )
{
 
  if (fread (cp, sizeof(char), 1, fp) != sizeof(char)) {
    return (0);
  }

  if (*cp == 0x00) { /* Escape */
    if (fread (cp, sizeof(char), 1, fp) != sizeof(char)) {
      return (0);
    }

    switch (*cp) {

      case 0x00: /* Escape */
        break;
      case 0x01: /* <LF> */
        *cp = 0x0a;
        break;
      case 0x02: /* <CR> */
        *cp = 0x0d;
        break;
      case 0x03: /* <ctrl-Z> */
        *cp = 0x1a;
        break;
    }
  }

  return (1);
}


