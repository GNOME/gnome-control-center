cappletname = default-apps

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(DEFAULT_APPS_PANEL_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DDATADIR="\"$(datadir)\""			\
	-DBINDIR="\"$(bindir)\""			\
	$(NULL)

noinst_LTLIBRARIES = libdefault-apps.la

BUILT_SOURCES =				\
	cc-default-apps-resources.c	\
	cc-default-apps-resources.h

libdefault_apps_la_SOURCES =	\
	$(BUILT_SOURCES)	\
	cc-default-apps-panel.c	\
	cc-default-apps-panel.h

libdefault_apps_la_LIBADD = $(PANEL_LIBS) $(DEFAULT_APPS_PANEL_CFLAGS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/default-apps.gresource.xml)
cc-default-apps-resources.c: default-apps.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_default_apps $<
cc-default-apps-resources.h: default-apps.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_default_apps $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-default-apps-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) default-apps.gresource.xml

-include $(top_srcdir)/git.mk
