bin_PROGRAMS = gnome-ui-properties

gnome_ui_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_ui_properties_SOURCES = \
	gnome-ui-properties.c

@INTLTOOL_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = behavior-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = behavior-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = behavior.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-ui-properties.glade
INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files)
