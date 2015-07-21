/**CFile***********************************************************************

  FileName    [cuddApa.c]

  PackageName [cudd]

  Synopsis    [Arbitrary precision arithmetic functions.]

  Description [External procedures included in this module:
		<ul>
		<li> Cudd_ApaNumberOfDigits()
		<li> Cudd_NewApaNumber()
		<li> Cudd_ApaCopy()
		<li> Cudd_ApaAdd()
		<li> Cudd_ApaSubtract()
		<li> Cudd_ApaShortDivision()
		<li> Cudd_ApaIntDivision()
		<li> Cudd_ApaShiftRight()
		<li> Cudd_ApaSetToLiteral()
		<li> Cudd_ApaPowerOfTwo()
		<li> Cudd_ApaCompare()
		<li> Cudd_ApaCompareRatios()
		<li> Cudd_ApaPrintHex()
		<li> Cudd_ApaPrintDecimal()
		<li> Cudd_ApaPrintExponential()
		<li> Cudd_ApaCountMinterm()
		<li> Cudd_ApaPrintMinterm()
		<li> Cudd_ApaPrintMintermExp()
		<li> Cudd_ApaPrintDensity()
		</ul>
	Static procedures included in this module:
		<ul>
		<li> cuddApaCountMintermAux()
		<li> cuddApaStCountfree()
		</ul>]

  Author      [Fabio Somenzi]

  Copyright   [Copyright (c) 1995-2012, Regents of the University of Colorado

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

******************************************************************************/

#include "CUDD/util.h"
#include "CUDD/cuddInt.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] DD_UNUSED = "$Id: cuddApa.c,v 1.20 2012/02/05 01:07:18 fabio Exp $";
#endif

static	DdNode	*background, *zero;

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/

static DdApaNumber cuddApaCountMintermAux (DdNode * node, int digits, DdApaNumber max, DdApaNumber min, st_table * table);
static enum st_retval cuddApaStCountfree (char * key, char * value, char * arg);

/**AutomaticEnd***************************************************************/

#ifdef __cplusplus
} /* end of extern "C" */
#endif


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Finds the number of digits for an arbitrary precision
  integer.]

  Description [Finds the number of digits for an arbitrary precision
  integer given the maximum number of binary digits.  The number of
  binary digits should be positive. Returns the number of digits if
  successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_ApaNumberOfDigits(
  int  binaryDigits)
{
    int digits;

    digits = binaryDigits / DD_APA_BITS;
    if ((digits * DD_APA_BITS) != binaryDigits)
	digits++;
    return(digits);

} /* end of Cudd_ApaNumberOfDigits */


/**Function********************************************************************

  Synopsis    [Allocates memory for an arbitrary precision integer.]

  Description [Allocates memory for an arbitrary precision
  integer. Returns a pointer to the allocated memory if successful;
  NULL otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
DdApaNumber
Cudd_NewApaNumber(
  int  digits)
{
    return(ALLOC(DdApaDigit, digits));

} /* end of Cudd_NewApaNumber */


/**Function********************************************************************

  Synopsis    [Makes a copy of an arbitrary precision integer.]

  Description [Makes a copy of an arbitrary precision integer.]

  SideEffects [Changes parameter <code>dest</code>.]

  SeeAlso     []

******************************************************************************/
void
Cudd_ApaCopy(
  int  digits,
  DdApaNumber  source,
  DdApaNumber  dest)
{
    int i;

    for (i = 0; i < digits; i++) {
	dest[i] = source[i];
    }

} /* end of Cudd_ApaCopy */


/**Function********************************************************************

  Synopsis    [Adds two arbitrary precision integers.]

  Description [Adds two arbitrary precision integers.  Returns the
  carry out of the most significant digit.]

  SideEffects [The result of the sum is stored in parameter <code>sum</code>.]

  SeeAlso     []

******************************************************************************/
DdApaDigit
Cudd_ApaAdd(
  int  digits,
  DdApaNumber  a,
  DdApaNumber  b,
  DdApaNumber  sum)
{
    int i;
    DdApaDoubleDigit partial = 0;

    for (i = digits - 1; i >= 0; i--) {
	partial = a[i] + b[i] + DD_MSDIGIT(partial);
	sum[i] = (DdApaDigit) DD_LSDIGIT(partial);
    }
    return((DdApaDigit) DD_MSDIGIT(partial));

} /* end of Cudd_ApaAdd */


/**Function********************************************************************

  Synopsis    [Subtracts two arbitrary precision integers.]

  Description [Subtracts two arbitrary precision integers.  Returns the
  borrow out of the most significant digit.]

  SideEffects [The result of the subtraction is stored in parameter
  <code>diff</code>.]

  SeeAlso     []

******************************************************************************/
DdApaDigit
Cudd_ApaSubtract(
  int  digits,
  DdApaNumber  a,
  DdApaNumber  b,
  DdApaNumber  diff)
{
    int i;
    DdApaDoubleDigit partial = DD_APA_BASE;

    for (i = digits - 1; i >= 0; i--) {
	partial = DD_MSDIGIT(partial) + DD_APA_MASK + a[i] - b[i];
	diff[i] = (DdApaDigit) DD_LSDIGIT(partial);
    }
    return((DdApaDigit) DD_MSDIGIT(partial) - 1);

} /* end of Cudd_ApaSubtract */


/**Function********************************************************************

  Synopsis    [Divides an arbitrary precision integer by a digit.]

  Description [Divides an arbitrary precision integer by a digit.]

  SideEffects [The quotient is returned in parameter <code>quotient</code>.]

  SeeAlso     []

******************************************************************************/
DdApaDigit
Cudd_ApaShortDivision(
  int  digits,
  DdApaNumber  dividend,
  DdApaDigit  divisor,
  DdApaNumber  quotient)
{
    int i;
    DdApaDigit remainder;
    DdApaDoubleDigit partial;

    remainder = 0;
    for (i = 0; i < digits; i++) {
	partial = remainder * DD_APA_BASE + dividend[i];
	quotient[i] = (DdApaDigit) (partial/(DdApaDoubleDigit)divisor);
	remainder = (DdApaDigit) (partial % divisor);
    }

    return(remainder);

} /* end of Cudd_ApaShortDivision */


/**Function********************************************************************

  Synopsis    [Divides an arbitrary precision integer by an integer.]

  Description [Divides an arbitrary precision integer by a 32-bit
  unsigned integer. Returns the remainder of the division. This
  procedure relies on the assumption that the number of bits of a
  DdApaDigit plus the number of bits of an unsigned int is less the
  number of bits of the mantissa of a double. This guarantees that the
  product of a DdApaDigit and an unsigned int can be represented
  without loss of precision by a double. On machines where this
  assumption is not satisfied, this procedure will malfunction.]

  SideEffects [The quotient is returned in parameter <code>quotient</code>.]

  SeeAlso     [Cudd_ApaShortDivision]

******************************************************************************/
unsigned int
Cudd_ApaIntDivision(
  int  digits,
  DdApaNumber dividend,
  unsigned int divisor,
  DdApaNumber quotient)
{
    int i;
    double partial;
    unsigned int remainder = 0;
    double ddiv = (double) divisor;

    for (i = 0; i < digits; i++) {
	partial = (double) remainder * DD_APA_BASE + dividend[i];
	quotient[i] = (DdApaDigit) (partial / ddiv);
	remainder = (unsigned int) (partial - ((double)quotient[i] * ddiv));
    }

    return(remainder);

} /* end of Cudd_ApaIntDivision */


/**Function********************************************************************

  Synopsis [Shifts right an arbitrary precision integer by one binary
  place.]

  Description [Shifts right an arbitrary precision integer by one
  binary place. The most significant binary digit of the result is
  taken from parameter <code>in</code>.]

  SideEffects [The result is returned in parameter <code>b</code>.]

  SeeAlso     []

******************************************************************************/
void
Cudd_ApaShiftRight(
  int  digits,
  DdApaDigit  in,
  DdApaNumber  a,
  DdApaNumber  b)
{
    int i;

    for (i = digits - 1; i > 0; i--) {
	b[i] = (a[i] >> 1) | ((a[i-1] & 1) << (DD_APA_BITS - 1));
    }
    b[0] = (a[0] >> 1) | (in << (DD_APA_BITS - 1));

} /* end of Cudd_ApaShiftRight */


/**Function********************************************************************

  Synopsis    [Sets an arbitrary precision integer to a one-digit literal.]

  Description [Sets an arbitrary precision integer to a one-digit literal.]

  SideEffects [The result is returned in parameter <code>number</code>.]

  SeeAlso     []

******************************************************************************/
void
Cudd_ApaSetToLiteral(
  int  digits,
  DdApaNumber  number,
  DdApaDigit  literal)
{
    int i;

    for (i = 0; i < digits - 1; i++)
	number[i] = 0;
    number[digits - 1] = literal;

} /* end of Cudd_ApaSetToLiteral */


/**Function********************************************************************

  Synopsis    [Sets an arbitrary precision integer to a power of two.]

  Description [Sets an arbitrary precision integer to a power of
  two. If the power of two is too large to be represented, the number
  is set to 0.]

  SideEffects [The result is returned in parameter <code>number</code>.]

  SeeAlso     []

******************************************************************************/
void
Cudd_ApaPowerOfTwo(
  int  digits,
  DdApaNumber  number,
  int  power)
{
    int i;
    int index;

    for (i = 0; i < digits; i++)
	number[i] = 0;
    i = digits - 1 - power / DD_APA_BITS;
    if (i < 0) return;
    index = power & (DD_APA_BITS - 1);
    number[i] = 1 << index;

} /* end of Cudd_ApaPowerOfTwo */


/**Function********************************************************************

  Synopsis    [Compares two arbitrary precision integers.]

  Description [Compares two arbitrary precision integers. Returns 1 if
  the first number is larger; 0 if they are equal; -1 if the second
  number is larger.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_ApaCompare(
  int digitsFirst,
  DdApaNumber  first,
  int digitsSecond,
  DdApaNumber  second)
{
    int i;
    int firstNZ, secondNZ;

    /* Find first non-zero in both numbers. */
    for (firstNZ = 0; firstNZ < digitsFirst; firstNZ++)
	if (first[firstNZ] != 0) break;
    for (secondNZ = 0; secondNZ < digitsSecond; secondNZ++)
	if (second[secondNZ] != 0) break;
    if (digitsFirst - firstNZ > digitsSecond - secondNZ) return(1);
    else if (digitsFirst - firstNZ < digitsSecond - secondNZ) return(-1);
    for (i = 0; i < digitsFirst - firstNZ; i++) {
	if (first[firstNZ + i] > second[secondNZ + i]) return(1);
	else if (first[firstNZ + i] < second[secondNZ + i]) return(-1);
    }
    return(0);

} /* end of Cudd_ApaCompare */


/**Function********************************************************************

  Synopsis    [Compares the ratios of two arbitrary precision integers to two
  unsigned ints.]

  Description [Compares the ratios of two arbitrary precision integers
  to two unsigned ints. Returns 1 if the first number is larger; 0 if
  they are equal; -1 if the second number is larger.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_ApaCompareRatios(
  int digitsFirst,
  DdApaNumber firstNum,
  unsigned int firstDen,
  int digitsSecond,
  DdApaNumber secondNum,
  unsigned int secondDen)
{
    int result;
    DdApaNumber first, second;
    unsigned int firstRem, secondRem;

    first = Cudd_NewApaNumber(digitsFirst);
    firstRem = Cudd_ApaIntDivision(digitsFirst,firstNum,firstDen,first);
    second = Cudd_NewApaNumber(digitsSecond);
    secondRem = Cudd_ApaIntDivision(digitsSecond,secondNum,secondDen,second);
    result = Cudd_ApaCompare(digitsFirst,first,digitsSecond,second);
    FREE(first);
    FREE(second);
    if (result == 0) {
	if ((double)firstRem/firstDen > (double)secondRem/secondDen)
	    return(1);
	else if ((double)firstRem/firstDen < (double)secondRem/secondDen)
	    return(-1);
    }
    return(result);

} /* end of Cudd_ApaCompareRatios */


/**Function********************************************************************

  Synopsis    [Prints an arbitrary precision integer in hexadecimal format.]

  Description [Prints an arbitrary precision integer in hexadecimal format.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_ApaPrintDecimal Cudd_ApaPrintExponential]

******************************************************************************/
int
Cudd_ApaPrintHex(
  FILE * fp,
  int  digits,
  DdApaNumber  number)
{
    int i, result;

    for (i = 0; i < digits; i++) {
	result = fprintf(fp,DD_APA_HEXPRINT,number[i]);
	if (result == EOF)
	    return(0);
    }
    return(1);

} /* end of Cudd_ApaPrintHex */


/**Function********************************************************************

  Synopsis    [Prints an arbitrary precision integer in decimal format.]

  Description [Prints an arbitrary precision integer in decimal format.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_ApaPrintHex Cudd_ApaPrintExponential]

******************************************************************************/
int
Cudd_ApaPrintDecimal(
  FILE * fp,
  int  digits,
  DdApaNumber  number)
{
    int i, result;
    DdApaDigit remainder;
    DdApaNumber work;
    unsigned char *decimal;
    int leadingzero;
    int decimalDigits = (int) (digits * log10((double) DD_APA_BASE)) + 1;

    work = Cudd_NewApaNumber(digits);
    if (work == NULL)
	return(0);
    decimal = ALLOC(unsigned char, decimalDigits);
    if (decimal == NULL) {
	FREE(work);
	return(0);
    }
    Cudd_ApaCopy(digits,number,work);
    for (i = decimalDigits - 1; i >= 0; i--) {
	remainder = Cudd_ApaShortDivision(digits,work,(DdApaDigit) 10,work);
	decimal[i] = (unsigned char) remainder;
    }
    FREE(work);

    leadingzero = 1;
    for (i = 0; i < decimalDigits; i++) {
	leadingzero = leadingzero && (decimal[i] == 0);
	if ((!leadingzero) || (i == (decimalDigits - 1))) {
	    result = fprintf(fp,"%1d",decimal[i]);
	    if (result == EOF) {
		FREE(decimal);
		return(0);
	    }
	}
    }
    FREE(decimal);
    return(1);

} /* end of Cudd_ApaPrintDecimal */


/**Function********************************************************************

  Synopsis    [Prints an arbitrary precision integer in exponential format.]

  Description [Prints an arbitrary precision integer in exponential format.
  Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_ApaPrintHex Cudd_ApaPrintDecimal]

******************************************************************************/
int
Cudd_ApaPrintExponential(
  FILE * fp,
  int  digits,
  DdApaNumber  number,
  int precision)
{
    int i, first, last, result;
    DdApaDigit remainder;
    DdApaNumber work;
    unsigned char *decimal;
    int decimalDigits = (int) (digits * log10((double) DD_APA_BASE)) + 1;

    work = Cudd_NewApaNumber(digits);
    if (work == NULL)
	return(0);
    decimal = ALLOC(unsigned char, decimalDigits);
    if (decimal == NULL) {
	FREE(work);
	return(0);
    }
    Cudd_ApaCopy(digits,number,work);
    first = decimalDigits - 1;
    for (i = decimalDigits - 1; i >= 0; i--) {
	remainder = Cudd_ApaShortDivision(digits,work,(DdApaDigit) 10,work);
	decimal[i] = (unsigned char) remainder;
	if (remainder != 0) first = i; /* keep track of MS non-zero */
    }
    FREE(work);
    last = ddMin(first + precision, decimalDigits);

    for (i = first; i < last; i++) {
	result = fprintf(fp,"%s%1d",i == first+1 ? "." : "", decimal[i]);
	if (result == EOF) {
	    FREE(decimal);
	    return(0);
	}
    }
    FREE(decimal);
    result = fprintf(fp,"e+%d",decimalDigits - first - 1);
    if (result == EOF) {
	return(0);
    }
    return(1);

} /* end of Cudd_ApaPrintExponential */


/**Function********************************************************************

  Synopsis    [Counts the number of minterms of a DD.]

  Description [Counts the number of minterms of a DD. The function is
  assumed to depend on nvars variables. The minterm count is
  represented as an arbitrary precision unsigned integer, to allow for
  any number of variables CUDD supports.  Returns a pointer to the
  array representing the number of minterms of the function rooted at
  node if successful; NULL otherwise.]

  SideEffects [The number of digits of the result is returned in
  parameter <code>digits</code>.]

  SeeAlso     [Cudd_CountMinterm]

******************************************************************************/
DdApaNumber
Cudd_ApaCountMinterm(
  DdManager * manager,
  DdNode * node,
  int  nvars,
  int * digits)
{
    DdApaNumber	max, min;
    st_table	*table;
    DdApaNumber	i,count;

    background = manager->background;
    zero = Cudd_Not(manager->one);

    *digits = Cudd_ApaNumberOfDigits(nvars+1);
    max = Cudd_NewApaNumber(*digits);
    if (max == NULL) {
	return(NULL);
    }
    Cudd_ApaPowerOfTwo(*digits,max,nvars);
    min = Cudd_NewApaNumber(*digits);
    if (min == NULL) {
	FREE(max);
	return(NULL);
    }
    Cudd_ApaSetToLiteral(*digits,min,0);
    table = st_init_table(st_ptrcmp,st_ptrhash);
    if (table == NULL) {
	FREE(max);
	FREE(min);
	return(NULL);
    }
    i = cuddApaCountMintermAux(Cudd_Regular(node),*digits,max,min,table);
    if (i == NULL) {
	FREE(max);
	FREE(min);
	st_foreach(table, cuddApaStCountfree, NULL);
	st_free_table(table);
	return(NULL);
    }
    count = Cudd_NewApaNumber(*digits);
    if (count == NULL) {
	FREE(max);
	FREE(min);
	st_foreach(table, cuddApaStCountfree, NULL);
	st_free_table(table);
	if (Cudd_Regular(node)->ref == 1) FREE(i);
	return(NULL);
    }
    if (Cudd_IsComplement(node)) {
	(void) Cudd_ApaSubtract(*digits,max,i,count);
    } else {
	Cudd_ApaCopy(*digits,i,count);
    }
    FREE(max);
    FREE(min);
    st_foreach(table, cuddApaStCountfree, NULL);
    st_free_table(table);
    if (Cudd_Regular(node)->ref == 1) FREE(i);
    return(count);

} /* end of Cudd_ApaCountMinterm */


/**Function********************************************************************

  Synopsis    [Prints the number of minterms of a BDD or ADD using
  arbitrary precision arithmetic.]

  Description [Prints the number of minterms of a BDD or ADD using
  arbitrary precision arithmetic. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_ApaPrintMintermExp]

******************************************************************************/
int
Cudd_ApaPrintMinterm(
  FILE * fp,
  DdManager * dd,
  DdNode * node,
  int  nvars)
{
    int digits;
    int result;
    DdApaNumber count;

    count = Cudd_ApaCountMinterm(dd,node,nvars,&digits);
    if (count == NULL)
	return(0);
    result = Cudd_ApaPrintDecimal(fp,digits,count);
    FREE(count);
    if (fprintf(fp,"\n") == EOF) {
	return(0);
    }
    return(result);

} /* end of Cudd_ApaPrintMinterm */


/**Function********************************************************************

  Synopsis    [Prints the number of minterms of a BDD or ADD in exponential
  format using arbitrary precision arithmetic.]

  Description [Prints the number of minterms of a BDD or ADD in
  exponential format using arbitrary precision arithmetic. Parameter
  precision controls the number of signficant digits printed. Returns
  1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     [Cudd_ApaPrintMinterm]

******************************************************************************/
int
Cudd_ApaPrintMintermExp(
  FILE * fp,
  DdManager * dd,
  DdNode * node,
  int  nvars,
  int precision)
{
    int digits;
    int result;
    DdApaNumber count;

    count = Cudd_ApaCountMinterm(dd,node,nvars,&digits);
    if (count == NULL)
	return(0);
    result = Cudd_ApaPrintExponential(fp,digits,count,precision);
    FREE(count);
    if (fprintf(fp,"\n") == EOF) {
	return(0);
    }
    return(result);

} /* end of Cudd_ApaPrintMintermExp */


/**Function********************************************************************

  Synopsis    [Prints the density of a BDD or ADD using
  arbitrary precision arithmetic.]

  Description [Prints the density of a BDD or ADD using
  arbitrary precision arithmetic. Returns 1 if successful; 0 otherwise.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
int
Cudd_ApaPrintDensity(
  FILE * fp,
  DdManager * dd,
  DdNode * node,
  int  nvars)
{
    int digits;
    int result;
    DdApaNumber count,density;
    unsigned int size, remainder, fractional;

    count = Cudd_ApaCountMinterm(dd,node,nvars,&digits);
    if (count == NULL)
	return(0);
    size = Cudd_DagSize(node);
    density = Cudd_NewApaNumber(digits);
    remainder = Cudd_ApaIntDivision(digits,count,size,density);
    result = Cudd_ApaPrintDecimal(fp,digits,density);
    FREE(count);
    FREE(density);
    fractional = (unsigned int)((double)remainder / size * 1000000);
    if (fprintf(fp,".%u\n", fractional) == EOF) {
	return(0);
    }
    return(result);

} /* end of Cudd_ApaPrintDensity */


/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/


/**Function********************************************************************

  Synopsis    [Performs the recursive step of Cudd_ApaCountMinterm.]

  Description [Performs the recursive step of Cudd_ApaCountMinterm.
  It is based on the following identity. Let |f| be the
  number of minterms of f. Then:
  <xmp>
    |f| = (|f0|+|f1|)/2
  </xmp>
  where f0 and f1 are the two cofactors of f.
  Uses the identity <code>|f'| = max - |f|</code>.
  The procedure expects the argument "node" to be a regular pointer, and
  guarantees this condition is met in the recursive calls.
  For efficiency, the result of a call is cached only if the node has
  a reference count greater than 1.
  Returns the number of minterms of the function rooted at node.]

  SideEffects [None]

******************************************************************************/
static DdApaNumber
cuddApaCountMintermAux(
  DdNode * node,
  int  digits,
  DdApaNumber  max,
  DdApaNumber  min,
  st_table * table)
{
    DdNode      *Nt, *Ne;
    DdApaNumber	mint, mint1, mint2;
    DdApaDigit	carryout;

    if (cuddIsConstant(node)) {
	if (node == background || node == zero) {
	    return(min);
	} else {
	    return(max);
	}
    }
    if (node->ref > 1 && st_lookup(table, node, &mint)) {
	return(mint);
    }

    Nt = cuddT(node); Ne = cuddE(node);

    mint1 = cuddApaCountMintermAux(Nt,  digits, max, min, table);
    if (mint1 == NULL) return(NULL);
    mint2 = cuddApaCountMintermAux(Cudd_Regular(Ne), digits, max, min, table);
    if (mint2 == NULL) {
	if (Nt->ref == 1) FREE(mint1);
	return(NULL);
    }
    mint = Cudd_NewApaNumber(digits);
    if (mint == NULL) {
	if (Nt->ref == 1) FREE(mint1);
	if (Cudd_Regular(Ne)->ref == 1) FREE(mint2);
	return(NULL);
    }
    if (Cudd_IsComplement(Ne)) {
	(void) Cudd_ApaSubtract(digits,max,mint2,mint);
	carryout = Cudd_ApaAdd(digits,mint1,mint,mint);
    } else {
	carryout = Cudd_ApaAdd(digits,mint1,mint2,mint);
    }
    Cudd_ApaShiftRight(digits,carryout,mint,mint);
    /* If the refernce count of a child is 1, its minterm count
    ** hasn't been stored in table.  Therefore, it must be explicitly
    ** freed here. */
    if (Nt->ref == 1) FREE(mint1);
    if (Cudd_Regular(Ne)->ref == 1) FREE(mint2);

    if (node->ref > 1) {
	if (st_insert(table, (char *)node, (char *)mint) == ST_OUT_OF_MEM) {
	    FREE(mint);
	    return(NULL);
	}
    }
    return(mint);

} /* end of cuddApaCountMintermAux */


/**Function********************************************************************

  Synopsis [Frees the memory used to store the minterm counts recorded
  in the visited table.]

  Description [Frees the memory used to store the minterm counts
  recorded in the visited table. Returns ST_CONTINUE.]

  SideEffects [None]

******************************************************************************/
static enum st_retval
cuddApaStCountfree(
  char * key,
  char * value,
  char * arg)
{
    DdApaNumber	d;

    d = (DdApaNumber) value;
    FREE(d);
    return(ST_CONTINUE);

} /* end of cuddApaStCountfree */
