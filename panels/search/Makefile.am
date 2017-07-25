cappletname = search

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(SEARCH_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DDATADIR="\"$(datadir)\""			\
	-I$(top_srcdir)/panels/common/			\
	$(NULL)

noinst_LTLIBRARIES = libsearch.la

BUILT_SOURCES =			\
	cc-search-resources.c	\
	cc-search-resources.h

libsearch_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-search-locations-dialog.c \
	cc-search-locations-dialog.h \
	cc-search-panel.c	\
	cc-search-panel.h

libsearch_la_LIBADD = $(PANEL_LIBS) $(SEARCH_PANEL_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/search.gresource.xml)
cc-search-resources.c: search.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_search $<
cc-search-resources.h: search.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_search $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-search-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) search.gresource.xml meson.build

-include $(top_srcdir)/git.mk
