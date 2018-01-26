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
	globals.c

test_CFLAGS = -I$(top_srcdir)/src -I/usr/local/include $(COVERAGE_CFLAGS)
test_LDFLAGS = -L/usr/local/lib $(COVERAGE_LDFLAGS)
test_LDADD = -lembedlog -lpthread

TESTS = $(check_PROGRAMS) $(check_SCRIPTS)
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/tap-driver.sh
EXTRA_DIST = mtest.sh
