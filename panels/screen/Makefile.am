cappletname = screen

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(SCREEN_PANEL_CFLAGS)			\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libscreen.la

libscreen_la_SOURCES =		\
	screen-module.c		\
	cc-screen-panel.c	\
	cc-screen-panel.h

libscreen_la_LIBADD = $(PANEL_LIBS) $(SCREEN_PANEL_LIBS)
libscreen_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = screen.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-screen-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
