#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mochi-json-viz], [0.1], [robl@mcs.anl.gov])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects silent-rules])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/json-viz.cc])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.
PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG" == "x"; then
    AC_MSG_ERROR([Could not find pkg-config utility!])
fi

PKG_CHECK_MODULES([JSON],[nlohmann_json], [],
		  AC_MSG_ERROR([Could not find working nlohman-json installation]))
CXXFLAGS="$JSON_CFLAGS $CXXFLAGS"

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
