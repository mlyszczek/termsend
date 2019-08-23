include ../Makefile.am.coverage

source = bnwlist.c \
	config.c \
	daemonize.c \
	main.c \
	server.c \
	globals.c \
	getopt.c

if ENABLE_OPENSSL
source += ssl/openssl.c
else
source += ssl/nonessl.c
endif

bin_PROGRAMS = termsend
termsend_SOURCES = $(source) \
	bnwlist.h \
	config.h \
	daemonize.h \
	globals.h \
	server.h \
	valid.h \
	feature.h \
	getopt.h \
	ssl/ssl.h

termsend_CFLAGS = -I$(top_srcdir) \
	$(COVERAGE_CFLAGS)

termsend_LDFLAGS = $(COVERAGE_LDFLAGS)

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -I.. -I. $< -o $@

analyze: $(analyze_plists)

endif
