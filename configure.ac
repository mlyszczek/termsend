AC_INIT([termsend], [1.1.1], [michal.lyszczek@bofc.pl])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([termsend.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CHECK_FUNCS(sigaction sigfillset ftruncate usleep fchown stat)
AC_PROG_CC
AC_PROG_SED
AC_CANONICAL_HOST
AC_CONFIG_FILES([Makefile src/Makefile tst/Makefile www/Makefile])
AC_CONFIG_LINKS([tst/test-server.sh:tst/test-server.sh])
AC_CONFIG_LINKS([tst/test-server.cert.pem:tst/test-server.cert.pem])
AC_CONFIG_LINKS([tst/test-server.key.pass:tst/test-server.key.pass])
AC_CONFIG_LINKS([tst/test-server.key.pem:tst/test-server.key.pem])
AC_CONFIG_LINKS([tst/mtest.sh:tst/mtest.sh])
AC_CHECK_HEADERS([linux/limits.h sys/select.h])

LDFLAGS="$LDFLAGS -L/usr/local/lib -L/usr/lib"
CFLAGS="$CFLAGS -I/usr/local/include -I/usr/include"

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)


###
# solaris has some serious design problem, since we enabled POSIX
# >=200112, solaris will not let use c89 compiler even though we
# are c89 compatible (with some header extension) So force c99 on
# solaris sytem.
#

case ${host_os} in
    solaris*)
        CFLAGS="-std=c99 ${CFLAGS}"
        ;;
esac

# by default ssl is disabled

AC_DEFINE([HAVE_SSL], [0], [Define to 1 if any of the ssl backend is enabled])

# --enable-openssl

AC_ARG_ENABLE([openssl],
    AS_HELP_STRING([--enable-openssl], [Enable openssl support]),
    [enable_openssl="yes"], [enable_openssl="no"])
AM_CONDITIONAL([ENABLE_OPENSSL], [test "x$enable_openssl" = "xyes"])

AS_IF([test "x$enable_openssl" = "xyes"],
[
    AC_DEFINE([HAVE_SSL], [1], [Define to 1 if any of the ssl backend is enabled])
    AC_SEARCH_LIBS([SSL_CTX_new], [ssl], [], [], [-lcrypto])
    AC_SEARCH_LIBS([ERR_clear_error], [crypto])
])

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
