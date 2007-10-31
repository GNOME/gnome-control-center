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

@INTLTOOL_XML_NOMERGE_RULE@

xmldir       = $(pkgdatadir)/keybindings
xml_in_files = 00-multimedia-key.xml.in 01-desktop-key.xml.in
xml_DATA     = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = gnome-keybindings.pc

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\"" \
	-DGNOMECC_KEYBINDINGS_DIR="\"$(pkgdatadir)/keybindings\""
CLEANFILES = \
	$(GNOMECC_CAPPLETS_CLEANFILES) \
	$(Desktop_in_files) \
	$(desktop_DATA) \
	$(xml_DATA)
EXTRA_DIST = $(Glade_DATA) $(xml_in_files) gnome-keybindings.pc.in

