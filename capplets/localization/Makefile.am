bin_PROGRAMS = gnome-localization-properties

gnome_localization_properties_SOURCES =	\
	gnome-localization-properties.c

gnome_localization_properties_LDADD =	\
	$(GNOMECC_CAPPLETS_LIBS)	\
	$(LOCALIZATION_CAPPLET_LIBS)

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(GNOMECC_GLADE_DIR)
glade_DATA = gnome-localization-properties.glade

pixmapsdir   = $(GNOMECC_PIXMAPS_DIR)
pixmaps_DATA = \
	localization-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_in_files = localization.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(glade_DATA) $(pixmaps_DATA) $(desktop_in_files)
