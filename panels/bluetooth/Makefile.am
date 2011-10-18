# This is used in PANEL_CFLAGS
cappletname = bluetooth

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libbluetooth.la

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(BLUETOOTH_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DPKGDATADIR="\"$(pkgdatadir)\""		\
	$(NULL)

libbluetooth_la_SOURCES =			\
	cc-bluetooth-panel.c			\
	cc-bluetooth-panel.h

libbluetooth_la_LIBADD = $(PANEL_LIBS) $(BLUETOOTH_LIBS)
libbluetooth_la_LDFLAGS = $(PANEL_LDFLAGS)

desktopdir = $(datadir)/applications
desktop_in_in_files = bluetooth-properties.desktop.in.in
desktop_in_files = bluetooth-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

ui_DATA = bluetooth.ui
uidir = $(pkgdatadir)

CLEANFILES = $(desktop_DATA)
EXTRA_DIST = $(man_MANS) $(desktop_in_in_files) $(ui_DATA)

-include $(top_srcdir)/git.mk
