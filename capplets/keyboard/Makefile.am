# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keyboard

bin_PROGRAMS = gnome-keyboard-properties

gnome_keyboard_properties_SOURCES = \
	gnome-keyboard-properties.c \
	gnome-keyboard-properties-a11y.c \
	gnome-keyboard-properties-a11y.h \
	gnome-keyboard-properties-xkb.c \
	gnome-keyboard-properties-xkbmc.c \
	gnome-keyboard-properties-xkblt.c \
	gnome-keyboard-properties-xkbltadd.c \
	gnome-keyboard-properties-xkbot.c \
	gnome-keyboard-properties-xkbpv.c \
	gnome-keyboard-properties-xkb.h

gnome_keyboard_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(LIBGNOMEKBDUI_LIBS)

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(pkgdatadir)/glade
Glade_DATA = gnome-keyboard-properties.glade

pixmapsdir   = $(pkgdatadir)/pixmaps
pixmaps_DATA = \
	keyboard-repeat.png		\
	keyboard-cursor.png		\
	keyboard-volume.png		\
	keyboard-bell.png

desktopdir = $(datadir)/applications
Desktop_in_files = keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(LIBGNOMEKBDUI_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\""
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(Glade_DATA) $(pixmaps_DATA)
