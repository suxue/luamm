AC_INIT([luamm], [0.2 foreign])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(autoconf-common/m4)

MY_COMMON

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])
AC_LANG([C++])
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11
AM_PROG_AR

LT_INIT

AC_TDD_GCOV
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

AC_LANG_PUSH([C])
AC_CHECK_HEADERS([lua.h luaconf.h], [],
                     [AC_MSG_ERROR(You need the lua C headers.)])
AC_LANG_POP([C])

AC_CHECK_HEADERS([lua.hpp],
                 [], [AC_MSG_ERROR(You need the lua c++ header.)])

AC_CHECK_HEADERS([boost/preprocessor.hpp],
                 [], [AC_MSG_ERROR(You need boost PP header.)])


AC_ARG_ENABLE([demos],
  AS_HELP_STRING([--disable-demos], [disable building demos]),
  [build_demos=${enableval}],
  [build_demos=yes])

AC_CONFIG_FILES([Makefile demos/Makefile])
AC_OUTPUT
