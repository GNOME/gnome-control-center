cappletname = privacy

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(PRIVACY_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-I$(top_srcdir)/panels/common/			\
	$(NULL)

noinst_LTLIBRARIES = libprivacy.la

BUILT_SOURCES =			\
	cc-privacy-resources.c	\
	cc-privacy-resources.h

libprivacy_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-privacy-panel.c	\
	cc-privacy-panel.h

libprivacy_la_LIBADD = $(PANEL_LIBS) $(PRIVACY_PANEL_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/privacy.gresource.xml)
cc-privacy-resources.c: privacy.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_privacy $<
cc-privacy-resources.h: privacy.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_privacy $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-privacy-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) privacy.gresource.xml meson.build

-include $(top_srcdir)/git.mk
