include ../Makefile.am.coverage

check_SCRIPTS = test-server.sh
check_PROGRAMS = test
test_SOURCES  = main.c \
	test-bnwlist.c \
	test-config.c \
	mtest.h \
	test-group-list.h \
	bnwlist.c \
	config.c \
	globals.c \
	getopt.c

if ENABLE_OPENSSL
test_SOURCES += openssl.c
else
test_SOURCES += nonessl.c
endif

test_CFLAGS = -I$(top_srcdir)/src \
	-I$(top_srcdir)/src/ssl \
	-I/usr/local/include \
	-I/usr/include \
	$(COVERAGE_CFLAGS) \
	$(PTHREAD_CFLAGS)

test_LDFLAGS = -L/usr/local/lib \
	-L/usr/lib \
	$(COVERAGE_LDFLAGS)

test_LDADD = -lembedlog $(PTHREAD_LIBS)

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/tap-driver.sh
EXTRA_DIST = mtest.sh test-server.sh test-server.sh \
	test-server.cert.pem test-server.key.pass test-server.key.pem
