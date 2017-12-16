include ../Makefile.am.coverage

bin_PROGRAMS = kurload
kurload_SOURCES = bnwlist.c \
				config.c \
				daemonize.c \
				main.c \
				server.c \
				globals.c \
				bnwlist.h \
				config.h \
				daemonize.h \
				globals.h \
				server.h \
				valid.h \
				version.h

kurload_CFLAGS = $(COVERAGE_CFLAGS)
kurload_LDFLAGS = $(COVERAGE_LDFLAGS)
kurload_LDADD = -lembedlog -lpthread
