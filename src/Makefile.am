include ../Makefile.am.coverage

source = bnwlist.c \
	config.c \
	daemonize.c \
	main.c \
	server.c \
	globals.c

bin_PROGRAMS = kurload
kurload_SOURCES = $(source) \
	bnwlist.h \
	config.h \
	daemonize.h \
	globals.h \
	server.h \
	valid.h \
	version.h

kurload_CFLAGS = -I$(top_srcdir) \
	-I/usr/local/include \
	-I/usr/include \
	$(COVERAGE_CFLAGS)

kurload_LDFLAGS = -L/usr/local/lib \
	-L/usr/lib \
	$(COVERAGE_LDFLAGS)

kurload_LDADD = -lembedlog -lpthread

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -I.. $< -o $@

analyze: $(analyze_plists)

endif
