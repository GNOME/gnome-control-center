bin_PROGRAMS = gnome2-keyboard-properties

gnome2_keyboard_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome2_keyboard_properties_SOURCES = gnome-keyboard-properties.c

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-keyboard-properties.glade

iconsdir   = $(GNOMECC_PIXMAPS_DIR)
icons_DATA = keyboard-capplet.png	\
	keyboard-repeat.png		\
	keyboard-cursor.png		\
	keyboard-volume.png		\
	keyboard-bell.png

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = keyboard-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files)
