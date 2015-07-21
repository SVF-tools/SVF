##===- projects/sample/lib/sample/Makefile -----------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

#
# Give the name of a library.  This will build a dynamic version.
#

#LIBRARYNAME=libdvf
LIBRARYNAME=libwpa

USEDLIBS = mssa.a mem.a ptautil.a cudd.a

BUILD_ARCHIVE := 1
SHARED_LIBRARY = 1
LOADABLE_MODULE := 1

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common
