SUBDIRS = icons

cappletname = color

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(COLOR_PANEL_CFLAGS)				\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DBINDIR="\"$(bindir)\""			\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libcolor.la

libcolor_la_SOURCES =		\
	color-module.c		\
	cc-color-panel.c	\
	cc-color-panel.h

libcolor_la_LIBADD = $(PANEL_LIBS) $(COLOR_PANEL_LIBS)
libcolor_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = color.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-color-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
