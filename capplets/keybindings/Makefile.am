# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keybinding

bin_PROGRAMS = gnome-keybinding-properties

gnome_keybinding_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_keybinding_properties_SOURCES = 	\
	gnome-keybinding-properties.c	\
	eggcellrendererkeys.c		\
	eggcellrendererkeys.h		\
	eggaccelerators.c		\
	eggaccelerators.h

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(pkgdatadir)/glade
Glade_DATA = gnome-keybinding-properties.glade

desktopdir = $(datadir)/applications
Desktop_in_files = keybinding.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\""
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(Glade_DATA)

