SUBDIRS = screensavers

cappletname = screensaver
cappletgroup =
bin_PROGRAMS = gnome-screensaver-properties

gnome_screensaver_properties_LDADD = @OLD_CAPPLET_LIBS@ -lXt
gnome_screensaver_properties_SOURCES = \
	main.c \
	prefs-widget.c				prefs-widget.h\
	preferences.c				preferences.h\
	screensaver-prefs-dialog.c	screensaver-prefs-dialog.h\
	selection-dialog.c			selection-dialog.h \
	pref-file.c				pref-file.h\
	preview.c					preview.h\
	expr.c					expr.h\
	daemon.c					daemon.h\
	rc-parse.c				rc-parse.h\
	resources.c				resources.h\
	gnome-startup.c	gnome-startup.h \
	XScreenSaver_ad.h

pixmaps_DATA = no-hack.png blank-screen.png checked.xpm unchecked.xpm

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_DATA = screensaver.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) @OLD_CAPPLET_CFLAGS@
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(pixmaps_DATA)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapsdir = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop

