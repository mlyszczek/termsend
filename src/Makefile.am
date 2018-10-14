include ../Makefile.am.coverage

source = bnwlist.c \
	config.c \
	daemonize.c \
	main.c \
	server.c \
	globals.c \
	getopt.c

bin_PROGRAMS = kurload
kurload_SOURCES = $(source) \
	bnwlist.h \
	config.h \
	daemonize.h \
	globals.h \
	server.h \
	valid.h \
	feature.h \
	getopt.h

kurload_CFLAGS = -I$(top_srcdir) \
	$(COVERAGE_CFLAGS)

kurload_LDFLAGS = $(COVERAGE_LDFLAGS)

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -I.. $< -o $@

analyze: $(analyze_plists)

endif
