bin_PROGRAMS = gnome-background-properties

gnome_background_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(top_builddir)/libbackground/libbackground.la
gnome_background_properties_SOURCES = background-properties-capplet.c

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = background-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = background-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = background-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = background.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) -I$(top_srcdir)/libbackground
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files)

