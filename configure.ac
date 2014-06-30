AC_INIT([luamm], [0.1 foreign])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(../m4)

AM_INIT_AUTOMAKE([1.14 no-define foreign])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])
AC_LANG([C++])
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11
AM_PROG_AR
AC_PROG_RANLIB

AC_LANG_PUSH([C])
AC_CHECK_HEADERS([lua.h luaconf.h], [],
                     [AC_MSG_ERROR(You need the lua C headers.)])
AC_LANG_POP([C])

AC_CHECK_HEADERS([lua.hpp],
                 [], [AC_MSG_ERROR(You need the lua c++ header.)])

AC_CHECK_HEADERS([boost/preprocessor.hpp],
                 [], [AC_MSG_ERROR(You need boost PP header.)])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
