include Makefile.am.coverage

check_PROGRAMS = test
test_SOURCES  = main.c \
				bnwlist.c \
				mtest.h \
				test-group-list.h

test_SOURCES += ../src/bnwlist.c \
				../src/config.c \
				../src/server.c \
				../src/globals.c

test_CFLAGS = -I$(top_srcdir)/src \
			  -I/usr/local/include \
			  -O0 -g3 -ggdb \
			  $(COVERAGE_CFLAGS)

test_LDFLAGS = -L/usr/local/lib \
			   $(COVERAGE_LDFLAGS)
test_LDADD = -lembedlog -lconfuse -lpthread

TESTS = $(check_PROGRAMS)
LOG_DRIVER = $(top_srcdir)/tap-driver
