
always_built_SUBDIRS = control-center capplets idl

SUBDIRS = intl po \
	 $(always_built_SUBDIRS)

#DIST_SUBDIRS = intl po macros $(always_built_SUBDIRS)


EXTRA_DIST = control-center.spec.in

dist-hook: control-center.spec
	cp control-center.spec $(distdir)


