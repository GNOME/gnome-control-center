bin_PROGRAMS = gnome2-default-applications-properties

gnome_default_applications_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_default_applications_properties_SOURCES = \
	gnome-default-applications-properties.c

@INTLTOOL_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = default-applications-capplet.png


Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-default-applications-properties.glade

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_DATA = default-applications.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(desktop_DATA)
