# This is used in PANEL_CFLAGS
cappletname = wacom

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(WACOM_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DPIXMAP_DIR=\""$(datadir)/gnome-control-center/pixmaps"\"	\
	$(NULL)


ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libwacom-properties.la

libwacom_properties_la_SOURCES =	\
	wacom-module.c			\
	cc-wacom-panel.c		\
	cc-wacom-panel.h

libwacom_properties_la_LIBADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS)
libwacom_properties_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

pixmapdir = $(pkgdatadir)/pixmaps
pixmap_DATA = \
	wacom-tablet.svg \
	wacom-stylus.svg

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-wacom-properties.ui

desktopdir = $(datadir)/applications
desktop_in_files = gnome-wacom-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
