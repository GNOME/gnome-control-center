bin_PROGRAMS = gnome-network-preferences

gnome_network_preferences_SOURCES = gnome-network-preferences.c
gnome_network_preferences_LDADD =				\
	$(GNOMECC_CAPPLETS_LIBS)

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(GNOMECC_GLADE_DIR)
glade_DATA = gnome-network-preferences.glade

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_in_files = gnome-network-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(glade_DATA) $(desktop_in_files)
