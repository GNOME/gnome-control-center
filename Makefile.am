
always_built_SUBDIRS = control-center capplets idl doc

SUBDIRS = intl po macros \
	 $(always_built_SUBDIRS)

#DIST_SUBDIRS = intl po macros $(always_built_SUBDIRS)


EXTRA_DIST = control-center.spec.in xml-i18n-extract.in xml-i18n-merge.in xml-i18n-update.in

dist-hook: control-center.spec
	cp control-center.spec $(distdir)

## to automatically rebuild aclocal.m4 if any of the macros in `macros/' change
@MAINT@include macros/macros.dep
@MAINT@macros/macros.dep: macros/Makefile.am
@MAINT@	cd macros && $(MAKE) macros.dep
