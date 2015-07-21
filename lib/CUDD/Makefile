##===- projects/sample/lib/sample/Makefile -----------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

## note that, use -DSIZEOF_VOID_P=8 -DSIZEOF_LONG=8 if this is an x86_64 machine
CFLAGS=-O3 -w -DHAVE_IEEE_754 -DSIZEOF_VOID_P=8 -DSIZEOF_LONG=8
#
# Give the name of a library.  This will build a dynamic version.
#

#LIBRARYNAME=libdvf
LIBRARYNAME=cudd

BUILD_ARCHIVE := 1

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common
