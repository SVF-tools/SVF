# generated automatically by aclocal 1.15 -*- Autoconf -*-

# Copyright (C) 1996-2014 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

m4_ifndef([AC_CONFIG_MACRO_DIRS], [m4_defun([_AM_CONFIG_MACRO_DIRS], [])m4_defun([AC_CONFIG_MACRO_DIRS], [_AM_CONFIG_MACRO_DIRS($@)])])
#
# Configure a Makefile without clobbering it if it exists and is not out of
# date.  This macro is unique to LLVM.
#
AC_DEFUN([AC_CONFIG_MAKEFILE],
[AC_CONFIG_COMMANDS($1,
  [${llvm_src}/autoconf/mkinstalldirs `dirname $1`
   ${SHELL} ${llvm_src}/autoconf/install-sh -m 0644 -c ${srcdir}/$1 $1])
])

#
# Provide the arguments and other processing needed for an LLVM project
#
AC_DEFUN([LLVM_CONFIG_PROJECT],
  [AC_ARG_WITH([llvmsrc],
    AS_HELP_STRING([--with-llvmsrc],[Location of LLVM Source Code]),
    [llvm_src="$withval"],[llvm_src="]$1["])
  AC_SUBST(LLVM_SRC,$llvm_src)
  AC_ARG_WITH([llvmobj],
    AS_HELP_STRING([--with-llvmobj],[Location of LLVM Object Code]),
    [llvm_obj="$withval"],[llvm_obj="]$2["])
  AC_SUBST(LLVM_OBJ,$llvm_obj)
  AC_CONFIG_COMMANDS([setup],,[llvm_src="${LLVM_SRC}"])
])

