/**CHeaderFile*****************************************************************

  FileName    [epd.h]

  PackageName [epd]

  Synopsis    [The University of Colorado extended double precision package.]

  Description [arithmetic functions with extended double precision.]

  SeeAlso     []

  Author      [In-Ho Moon]

  Copyright   [Copyright (c) 1995-2004, Regents of the University of Colorado

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  Neither the name of the University of Colorado nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.]

  Revision    [$Id: epd.h,v 1.9 2004/08/13 18:20:30 fabio Exp $]

******************************************************************************/

#ifndef _EPD
#define _EPD

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

#define	EPD_MAX_BIN	1023
#define	EPD_MAX_DEC	308
#define	EPD_EXP_INF	0x7ff

/*---------------------------------------------------------------------------*/
/* Structure declarations                                                    */
/*---------------------------------------------------------------------------*/

/**Struct**********************************************************************

  Synopsis    [IEEE double struct.]

  Description [IEEE double struct.]

  SeeAlso     []

******************************************************************************/
#ifdef	EPD_BIG_ENDIAN
struct IeeeDoubleStruct  	/* BIG_ENDIAN */
{
    unsigned int sign: 1;
    unsigned int exponent: 11;
    unsigned int mantissa0: 20;
    unsigned int mantissa1: 32;
};
#else
struct IeeeDoubleStruct  	/* LITTLE_ENDIAN */
{
    unsigned int mantissa1: 32;
    unsigned int mantissa0: 20;
    unsigned int exponent: 11;
    unsigned int sign: 1;
};
#endif

/**Struct**********************************************************************

  Synopsis    [IEEE double NaN struct.]

  Description [IEEE double NaN struct.]

  SeeAlso     []

******************************************************************************/
#ifdef	EPD_BIG_ENDIAN
struct IeeeNanStruct  	/* BIG_ENDIAN */
{
    unsigned int sign: 1;
    unsigned int exponent: 11;
    unsigned int quiet_bit: 1;
    unsigned int mantissa0: 19;
    unsigned int mantissa1: 32;
};
#else
struct IeeeNanStruct  	/* LITTLE_ENDIAN */
{
    unsigned int mantissa1: 32;
    unsigned int mantissa0: 19;
    unsigned int quiet_bit: 1;
    unsigned int exponent: 11;
    unsigned int sign: 1;
};
#endif

/**Struct**********************************************************************

  Synopsis    [Extended precision double to keep very large value.]

  Description [Extended precision double to keep very large value.]

  SeeAlso     []

******************************************************************************/
union EpTypeUnion
{
    double			value;
    struct IeeeDoubleStruct	bits;
    struct IeeeNanStruct		nan;
};

struct EpDoubleStruct
{
    union EpTypeUnion		type;
    int				exponent;
};

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/
typedef struct EpDoubleStruct EpDouble;
typedef struct IeeeDoubleStruct IeeeDouble;
typedef struct IeeeNanStruct IeeeNan;
typedef union EpTypeUnion EpType;

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Function prototypes                                                       */
/*---------------------------------------------------------------------------*/

extern EpDouble *EpdAlloc(void);
extern void EpdFree(EpDouble *epd);
extern void EpdConvert(double value, EpDouble *epd);
extern void EpdMultiply(EpDouble *epd1, double value);
extern void EpdMultiply3(EpDouble *epd1, EpDouble *epd2, EpDouble *epd3);
extern void EpdMultiply3Decimal(EpDouble *epd1, EpDouble *epd2, EpDouble *epd3);
extern void EpdAdd3(EpDouble *epd1, EpDouble *epd2, EpDouble *epd3);
extern void EpdSubtract3(EpDouble *epd1, EpDouble *epd2, EpDouble *epd3);
extern void EpdPow2(int n, EpDouble *epd);
extern void EpdPow2Decimal(int n, EpDouble *epd);
extern void EpdNormalize(EpDouble *epd);
extern void EpdNormalizeDecimal(EpDouble *epd);
extern int EpdGetExponent(double value);
extern int EpdGetExponentDecimal(double value);
extern void EpdMakeInf(EpDouble *epd, int sign);
extern void EpdMakeZero(EpDouble *epd, int sign);
extern void EpdMakeNan(EpDouble *epd);
extern void EpdCopy(EpDouble *from, EpDouble *to);
extern int EpdIsInf(EpDouble *epd);
extern int EpdIsNan(EpDouble *epd);
extern int IsInfDouble(double value);
extern int IsNanDouble(double value);
extern int IsNanOrInfDouble(double value);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _EPD */
