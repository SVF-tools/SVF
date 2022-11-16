#ifndef _STD_TESTCASE_H
#define _STD_TESTCASE_H

/* This file exists in order to:
 * 1) Include lots of standardized headers in one place
 * 2) To avoid #include-ing things in the middle of your code
 *    #include-ing in the middle of a C/C++ file is apt to cause compiler errors
 * 3) To get good #define's in place
 *
 * In reality you need a complex interaction of scripts of build processes to do
 * this correctly (i.e., autoconf)
 */

#ifdef _WIN32
/* Ensure the CRT does not disable the "insecure functions"
 * Ensure not to generate warnings about ANSI C functions.
 */
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNING 1

/* We do not use _malloca as it sometimes allocates memory on the heap and we
 * do not want this to happen in test cases that expect stack-allocated memory
 * Also, _alloca_s cannot be used as it is deprecated and has been replaced
 * with _malloca */
#define ALLOCA _alloca

/* disable warnings about use of POSIX names for functions like execl()
 Visual Studio wants you to use the ISO C++ name, such as _execl() */
#pragma warning(disable:4996)

#else
/* Linux/GNU wants this macro, otherwise stdint.h and limits.h are mostly useless */
# define __STDC_LIMIT_MACROS 1

#define ALLOCA alloca
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
/* SIZE_MAX, int64_t, etc. are in this file on Linux */
# include <stdint.h>
#endif
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h> /* for open/close etc */
#endif

#include <inttypes.h> // for PRId64
#include <wctype.h>

#ifndef _WIN32
#include <wchar.h>
#endif


#ifdef __cplusplus
#include <new> // for placement new

/* classes used in some test cases as a custom type */
class TwoIntsClass 
{
    public: // Needed to access variables from label files
        int intOne;
        int intTwo;
};

class OneIntClass 
{
    public: // Needed to access variables from label files
        int intOne;
};

#endif

#ifndef __cplusplus
/* Define true and false, which are included in C++, but not in C */
#define true 1
#define false 0

#endif /* end ifndef __cplusplus */

/* rand only returns 15 bits, so we xor 3 calls together to get the full result (13 bits overflow, but that is okay) */
// shifting signed values might overflow and be undefined
#define URAND31() (((unsigned)rand()<<30) ^ ((unsigned)rand()<<15) ^ rand())
// choose to produce a positive or a negative number. Note: conditional only evaluates one URAND31
#define RAND32() ((int)(rand() & 1 ? URAND31() : -URAND31() - 1))

/* rand only returns 15 bits, so we xor 5 calls together to get the full result (11 bits overflow, but that is okay) */
// shifting signed values might overflow and be undefined
#define URAND63() (((uint64_t)rand()<<60) ^ ((uint64_t)rand()<<45) ^ ((uint64_t)rand()<<30) ^ ((uint64_t)rand()<<15) ^ rand())
// choose to produce a positive or a negative number. Note: conditional only evaluates one URAND63
#define RAND64() ((int64_t)(rand() & 1 ? URAND63() : -URAND63() - 1))

/* struct used in some test cases as a custom type */
typedef struct _twoIntsStruct
{
    int intOne;
    int intTwo;
} twoIntsStruct;

#ifdef __cplusplus
extern "C" {
#endif

/* The variables below are declared "const", so a tool should
   be able to identify that reads of these will always return their 
   initialized values. */
extern const int GLOBAL_CONST_TRUE; /* true */
extern const int GLOBAL_CONST_FALSE; /* false */
extern const int GLOBAL_CONST_FIVE; /* 5 */

/* The variables below are not defined as "const", but are never
   assigned any other value, so a tool should be able to identify that
   reads of these will always return their initialized values. */
extern int globalTrue; /* true */
extern int globalFalse; /* false */
extern int globalFive; /* 5 */

#ifdef __cplusplus
}
#endif

/* header file to define functions in io.c.  Not named io.h
   because that name is already taken by a system header on 
   Windows */


#ifdef __cplusplus
extern "C" {
#endif

void printLine (const char * line)
{
    if(line != NULL) 
    {
        printf("%s\n", line);
    }
}

void printWLine (const wchar_t * line)
{
    if(line != NULL) 
    {
        wprintf(L"%ls\n", line);
    }
}

void printIntLine (int intNumber)
{
    printf("%d\n", intNumber);
}

void printShortLine (short shortNumber)
{
    printf("%hd\n", shortNumber);
}

void printFloatLine (float floatNumber)
{
    printf("%f\n", floatNumber);
}

void printLongLine (long longNumber)
{
    printf("%ld\n", longNumber);
}

void printLongLongLine (int64_t longLongIntNumber)
{
    printf("%" PRId64 "\n", longLongIntNumber);
}

void printSizeTLine (size_t sizeTNumber)
{
    printf("%zu\n", sizeTNumber);
}

void printHexCharLine (char charHex)
{
    printf("%02x\n", charHex);
}

void printWcharLine(wchar_t wideChar) 
{
    /* ISO standard dictates wchar_t can be ref'd only with %ls, so we must make a
     * string to print a wchar */
    wchar_t s[2];
        s[0] = wideChar;
        s[1] = L'\0';
    printf("%ls\n", s);
}

void printUnsignedLine(unsigned unsignedNumber) 
{
    printf("%u\n", unsignedNumber);
}

void printHexUnsignedCharLine(unsigned char unsignedCharacter) 
{
    printf("%02x\n", unsignedCharacter);
}

void printDoubleLine(double doubleNumber) 
{
    printf("%g\n", doubleNumber);
}

void printStructLine (const twoIntsStruct * structTwoIntsStruct)
{
    printf("%d -- %d\n", structTwoIntsStruct->intOne, structTwoIntsStruct->intTwo);
}

void printBytesLine(const unsigned char * bytes, size_t numBytes)
{
    size_t i;
    for (i = 0; i < numBytes; ++i)
    {
        printf("%02x", bytes[i]);
    }
    puts("");   /* output newline */
}

/* Decode a string of hex characters into the bytes they represent.  The second
 * parameter specifies the length of the output buffer.  The number of bytes
 * actually written to the output buffer is returned. */
size_t decodeHexChars(unsigned char * bytes, size_t numBytes, const char * hex)
{
    size_t numWritten = 0;

    /* We can't sscanf directly into the byte array since %02x expects a pointer to int,
     * not a pointer to unsigned char.  Also, since we expect an unbroken string of hex
     * characters, we check for that before calling sscanf; otherwise we would get a
     * framing error if there's whitespace in the input string. */
    while (numWritten < numBytes && isxdigit(hex[2 * numWritten]) && isxdigit(hex[2 * numWritten + 1]))
    {
        int byte;
        sscanf(&hex[2 * numWritten], "%02x", &byte);
        bytes[numWritten] = (unsigned char) byte;
        ++numWritten;
    }

    return numWritten;
}

/* Decode a string of hex characters into the bytes they represent.  The second
 * parameter specifies the length of the output buffer.  The number of bytes
 * actually written to the output buffer is returned. */
 size_t decodeHexWChars(unsigned char * bytes, size_t numBytes, const wchar_t * hex)
 {
    size_t numWritten = 0;

    /* We can't swscanf directly into the byte array since %02x expects a pointer to int,
     * not a pointer to unsigned char.  Also, since we expect an unbroken string of hex
     * characters, we check for that before calling swscanf; otherwise we would get a
     * framing error if there's whitespace in the input string. */
    while (numWritten < numBytes && iswxdigit(hex[2 * numWritten]) && iswxdigit(hex[2 * numWritten + 1]))
    {
        int byte;
        swscanf(&hex[2 * numWritten], L"%02x", &byte);
        bytes[numWritten] = (unsigned char) byte;
        ++numWritten;
    }

    return numWritten;
}

/* The two functions always return 1 or 0, so a tool should be able to 
   identify that uses of these functions will always return these values */
int globalReturnsTrue() 
{
    return 1;
}

int globalReturnsFalse() 
{
    return 0;
}

int globalReturnsTrueOrFalse() 
{
    return (rand() % 2);
}

/* The variables below are declared "const", so a tool should
   be able to identify that reads of these will always return their 
   initialized values. */
const int GLOBAL_CONST_TRUE = 1; /* true */
const int GLOBAL_CONST_FALSE = 0; /* false */
const int GLOBAL_CONST_FIVE = 5; 

/* The variables below are not defined as "const", but are never
   assigned any other value, so a tool should be able to identify that
   reads of these will always return their initialized values. */
int globalTrue = 1; /* true */
int globalFalse = 0; /* false */
int globalFive = 5; 

/* define a bunch of these as empty functions so that if a test case forgets
   to make their's statically scoped, we'll get a linker error */
void good1() { }
void good2() { }
void good3() { }
void good4() { }
void good5() { }
void good6() { }
void good7() { }
void good8() { }
void good9() { }

/* shouldn't be used, but just in case */
void bad1() { }
void bad2() { }
void bad3() { }
void bad4() { }
void bad5() { }
void bad6() { }
void bad7() { }
void bad8() { }
void bad9() { }

/* define global argc and argv */

#ifdef __cplusplus
extern "C" {
#endif

int globalArgc = 0;
char** globalArgv = NULL;


/* Define some global variables that will get argc and argv */
extern int globalArgc;
extern char** globalArgv;

#ifdef __cplusplus
}
#endif


#endif
