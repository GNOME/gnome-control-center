cappletname = search

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(SEARCH_PANEL_CFLAGS)				\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(datadir)\""			\
	-I$(top_srcdir)/panels/common/			\
	$(NULL)

noinst_LTLIBRARIES = libsearch.la

libsearch_la_SOURCES =		\
	cc-search-locations-dialog.c \
	cc-search-locations-dialog.h \
	cc-search-panel.c	\
	cc-search-panel.h

libsearch_la_LIBADD = $(PANEL_LIBS) $(SEARCH_PANEL_LIBS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = 		\
	search.ui 	\
	search-locations-dialog.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-search-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
