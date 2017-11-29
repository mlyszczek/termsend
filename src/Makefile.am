include ../Makefile.am.coverage

bin_PROGRAMS = kurload
kurload_SOURCES = bnwlist.c \
				  config.c \
				  daemonize.c \
				  main.c \
				  server.c \
				  globals.c \
				  valid.h

kurload_CFLAGS = $(COVERAGE_CFLAGS)
kurload_LDFLAGS = $(COVERAGE_LDFLAGS)
kurload_LDADD = -lconfuse -lembedlog -lpthread
