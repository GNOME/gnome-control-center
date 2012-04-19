# This is used in PANEL_CFLAGS
cappletname = wacom

SUBDIRS = calibrator

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(WACOM_PANEL_CFLAGS)				\
	-I$(srcdir)/calibrator				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	$(NULL)


ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libwacom-properties.la

libwacom_properties_la_SOURCES =	\
	wacom-module.c			\
	cc-wacom-panel.c		\
	cc-wacom-panel.h		\
	cc-wacom-page.c			\
	cc-wacom-page.h			\
	cc-wacom-stylus-page.c		\
	cc-wacom-stylus-page.h		\
	gsd-wacom-device.c		\
	gsd-wacom-device.h		\
	gsd-input-helper.c		\
	gsd-input-helper.h		\
	cc-wacom-nav-button.c		\
	cc-wacom-nav-button.h		\
	cc-wacom-mapping-panel.c	\
	cc-wacom-mapping-panel.h	\
	gsd-enums.h

libwacom_properties_la_LIBADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS) $(builddir)/calibrator/libwacom-calibrator.la
libwacom_properties_la_LDFLAGS = $(PANEL_LDFLAGS)

noinst_PROGRAMS = test-wacom

test_wacom_SOURCES =			\
	test-wacom.c			\
	cc-wacom-page.c			\
	cc-wacom-page.h			\
	cc-wacom-stylus-page.c		\
	cc-wacom-stylus-page.h		\
	cc-wacom-nav-button.c		\
	cc-wacom-nav-button.h		\
	cc-wacom-mapping-panel.c	\
	cc-wacom-mapping-panel.h	\
	gsd-wacom-device.c		\
	gsd-wacom-device.h		\
	gsd-input-helper.c		\
	gsd-input-helper.h		\
	gsd-enums.h

test_wacom_CPPFLAGS = $(INCLUDES)
test_wacom_LDADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS) $(builddir)/calibrator/libwacom-calibrator.la

@INTLTOOL_DESKTOP_RULE@

pixmapdir = $(pkgdatadir)/ui
pixmap_DATA =				\
	wacom-tablet.svg		\
	wacom-stylus.svg		\
	wacom-stylus-airbrush.svg	\
	wacom-stylus-inking.svg		\
	wacom-stylus-art-pen.svg	\
	wacom-stylus-classic.svg	\
	wacom-tablet-cintiq.svg		\
	wacom-tablet-pc.svg

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-wacom-properties.ui wacom-stylus-page.ui button-mapping.ui

desktopdir = $(datadir)/applications
desktop_in_files = gnome-wacom-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

WACOMDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/wacom/
WACOMFILES=gsd-wacom-device.c gsd-wacom-device.h
COMMONDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/common/
COMMONFILES=gsd-input-helper.c gsd-input-helper.h
update-from-gsd:
	FILES="$(WACOMFILES)" DIR="$(WACOMDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	FILES="$(COMMONFILES)" DIR="$(COMMONDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	git commit -m "wacom: Update from gnome-settings-daemon" $(WACOMFILES) $(COMMONFILES)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
