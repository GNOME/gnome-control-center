bin_PROGRAMS = gnome-keyboard-properties

gnome_keyboard_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)

gnome_keyboard_properties_SOURCES =		\
	keyboard-properties.c

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = keyboard-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = keyboard-capplet.png

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)

