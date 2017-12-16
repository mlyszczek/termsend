include ../Makefile.am.coverage

check_PROGRAMS = test
test_SOURCES  = main.c \
				bnwlist.c \
				config.c \
				mtest.h \
				test-group-list.h

test_SOURCES += ../src/bnwlist.c \
				../src/config.c \
				../src/globals.c

test_CFLAGS = -I$(top_srcdir)/src -I/usr/local/include $(COVERAGE_CFLAGS)
test_LDFLAGS = -L/usr/local/lib $(COVERAGE_LDFLAGS)
test_LDADD = -lembedlog -lpthread -lconfuse

TESTS = $(check_PROGRAMS) test-server.sh
LOG_DRIVER = $(top_srcdir)/tap-driver
EXTRA_DIST = test-server.sh mtest.sh test-config.conf
