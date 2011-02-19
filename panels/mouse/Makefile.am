# This is used in PANEL_CFLAGS
cappletname = mouse

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(MOUSE_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DPIXMAP_DIR=\""$(datadir)/gnome-control-center/pixmaps"\"	\
	$(NULL)


ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libmouse-properties.la

libmouse_properties_la_SOURCES =	\
	mouse-module.c			\
	cc-mouse-panel.c		\
	cc-mouse-panel.h		\
	gnome-mouse-properties.c	\
	gnome-mouse-properties.h	\
	gsd-input-helper.c		\
	gsd-input-helper.h

libmouse_properties_la_LIBADD = $(PANEL_LIBS) $(MOUSE_PANEL_LIBS)
libmouse_properties_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(pkgdatadir)/pixmaps
pixmap_DATA = \
	double-click-on.png		\
	double-click-off.png		\
	double-click-maybe.png

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-mouse-properties.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-mouse-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
