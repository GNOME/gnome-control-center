
always_built_SUBDIRS = new-control-center libcapplet capplets idl

SUBDIRS = intl \
	 $(always_built_SUBDIRS) po

#DIST_SUBDIRS = intl po macros $(always_built_SUBDIRS)


EXTRA_DIST = control-center.spec.in

dist-hook: control-center.spec
	cp control-center.spec $(distdir)


