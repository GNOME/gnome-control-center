cappletname = universal-access

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(UNIVERSAL_ACCESS_PANEL_CFLAGS)		\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/panels/common/			\
	$(NULL)

noinst_LTLIBRARIES = libuniversal-access.la

BUILT_SOURCES =			\
	cc-ua-resources.c	\
	cc-ua-resources.h

libuniversal_access_la_SOURCES =		\
	$(BUILT_SOURCES)			\
	cc-ua-panel.c				\
	cc-ua-panel.h				\
	zoom-options.c				\
	zoom-options.h

libuniversal_access_la_LIBADD = $(PANEL_LIBS) $(UNIVERSAL_ACCESS_PANEL_LIBS) $(LIBM)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/universal-access.gresource.xml)
cc-ua-resources.c: universal-access.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_universal_access $<
cc-ua-resources.h: universal-access.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_universal_access $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-universal-access-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) universal-access.gresource.xml meson.build

-include $(top_srcdir)/git.mk
