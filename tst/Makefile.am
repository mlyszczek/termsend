include ../Makefile.am.coverage

check_PROGRAMS = test-bnw
test_bnw_SOURCES  = main.c \
				bnwlist.c \
				mtest.h \
				test-group-list.h

test_bnw_SOURCES += ../src/bnwlist.c
test_bnw_CFLAGS = -I$(top_srcdir)/src -I/usr/local/include $(COVERAGE_CFLAGS)
test_bnw_LDFLAGS = -L/usr/local/lib $(COVERAGE_LDFLAGS)
test_bnw_LDADD = -lembedlog -lpthread

TESTS = $(check_PROGRAMS) test-server.sh
LOG_DRIVER = $(top_srcdir)/tap-driver
EXTRA_DIST = test-server.sh mtest.sh test-config.conf
