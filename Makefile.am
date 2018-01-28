SUBDIRS = src tst www

sysconf_DATA = init.d/kurload.conf
init_ddir = $(sysconfdir)/init.d
dist_init_d_SCRIPTS = init.d/kurload
EXTRA_DIST = init.d/kurload.conf init.d/kurload.openrc man2html.sh gen-download-page.sh readme.md

analyze:
	make analyze -C src

www:
	./gen-download-page.sh
	./man2html.sh
	make www -C www

.PHONY: analyze www
