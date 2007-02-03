# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = network

bin_PROGRAMS = gnome-network-preferences

gnome_network_preferences_SOURCES = gnome-network-preferences.c
gnome_network_preferences_LDADD =				\
	$(GNOMECC_CAPPLETS_LIBS)

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-network-preferences.glade

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\""
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(glade_DATA)
