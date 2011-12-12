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
	cc-wacom-panel.h		\
	cc-wacom-page.c			\
	cc-wacom-page.h			\
	gsd-wacom-device.c		\
	gsd-wacom-device.h		\
	gsd-input-helper.c		\
	gsd-input-helper.h		\
	cc-wacom-nav-button.c		\
	cc-wacom-nav-button.h

libwacom_properties_la_LIBADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS)
libwacom_properties_la_LDFLAGS = $(PANEL_LDFLAGS)

noinst_PROGRAMS = test-wacom

test_wacom_SOURCES =			\
	test-wacom.c			\
	cc-wacom-page.c			\
	cc-wacom-page.h			\
	cc-wacom-nav-button.c		\
	cc-wacom-nav-button.h		\
	gsd-wacom-device.c		\
	gsd-wacom-device.h		\
	gsd-input-helper.c		\
	gsd-input-helper.h

test_wacom_CPPFLAGS = $(INCLUDES)
test_wacom_LDADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS)

@INTLTOOL_DESKTOP_RULE@

pixmapdir = $(pkgdatadir)/pixmaps
pixmap_DATA =				\
	wacom-tablet.svg		\
	wacom-stylus.svg		\
	wacom-stylus-airbrush.svg	\
	wacom-stylus-inking.svg		\
	wacom-tablet-cintiq.svg		\
	wacom-tablet-pc.svg

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-wacom-properties.ui

desktopdir = $(datadir)/applications
desktop_in_files = gnome-wacom-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
