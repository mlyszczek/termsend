AC_INIT([kurload], [1.0.0], [michal.lyszczek@bofc.pl])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([kurload.h])
AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_CONFIG_MACRO_DIR([m4])
AC_CHECK_FUNCS(sigaction sigfillset ftruncate usleep fchown stat)
AC_PROG_CC
AC_PROG_CC_C89
AC_CONFIG_FILES([Makefile src/Makefile tst/Makefile www/Makefile])
AC_CONFIG_LINKS([tst/test-server.sh:tst/test-server.sh])
AC_CONFIG_LINKS([tst/mtest.sh:tst/mtest.sh])

LDFLAGS="$LDFLAGS -L/usr/local/lib -L/usr/lib"
CFLAGS="$CFLAGS -I/usr/local/include -I/usr/include"

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

###
# --enable-analyzer
#

AC_ARG_ENABLE([analyzer],
    AS_HELP_STRING([--enable-analyzer], [Enable static code analyzer]),
    [enable_analyzer="yes"], [enable_analyzer="no"])
AM_CONDITIONAL([ENABLE_ANALYZER], [test "x$enable_analyzer" = "xyes"])


AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([gethostent], [nsl])
AC_SEARCH_LIBS([el_init], [embedlog])


AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AC_OUTPUT
