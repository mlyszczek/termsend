include Makefile.am.coverage
ACLOCAL_AMFLAGS=-I m4
SUBDIRS = src tst www

dist_sysconf_DATA = init.d/termsend.conf
init_ddir = $(sysconfdir)/init.d
dist_init_d_SCRIPTS = init.d/termsend
dist_man_MANS = termsend.1
EXTRA_DIST = init.d/termsend.openrc man2html.sh gen-download-page.sh readme.md tap-driver.sh

analyze:
	make analyze -C src

www:
	./gen-download-page.sh
	./man2html.sh
	make www -C www

.PHONY: analyze www
