SUBDIRS = screensavers

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

@INTLTOOL_DESKTOP_RULE@

pixmapsdir = $(GNOMECC_PIXMAPS_DIR)
pixmaps_DATA = no-hack.png blank-screen.png checked.xpm unchecked.xpm

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = screensaver-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = screensaver-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_DATA = screensaver.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) @OLD_CAPPLET_CFLAGS@
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(desktop_DATA) $(pixmaps_DATA)

