bin_PROGRAMS = gnome-keybinding-properties

gnome_keybinding_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_keybinding_properties_SOURCES = 	\
	gnome-keybinding-properties.c	\
	eggcellrendererkeys.c		\
	eggcellrendererkeys.h		\
	eggaccelerators.c		\
	eggaccelerators.h

@INTLTOOL_DESKTOP_RULE@


Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-keybinding-properties.glade

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = keyboard-shortcut.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = keybinding.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(Desktop_in_files) $(pixmaps_DATA)

