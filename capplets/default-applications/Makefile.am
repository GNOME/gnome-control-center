bin_PROGRAMS = gnome-default-applications-properties

gnome_default_applications_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_default_applications_properties_SOURCES = \
	gnome-default-applications-properties.c

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-default-applications-properties.glade

desktopdir = $(datadir)/applications
Desktop_in_files = default-applications.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(Glade_DATA) gnome-default-applications-properties-structs.c
