AC_INIT([kurload], [0.1.0], [michal.lyszczek@bofc.pl])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([kurload.h])
AC_CHECK_FUNCS(sigaction sigfillset ftruncate usleep fchown stat)
AX_PTHREAD(, [AC_MSG_ERROR(pthread not found)])
AC_PROG_CC
AC_PROG_CC_C89
AC_CONFIG_FILES([Makefile src/Makefile tst/Makefile])


m4_include([m4/gcov.m4])
AC_TDD_GCOV
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

AC_OUTPUT
