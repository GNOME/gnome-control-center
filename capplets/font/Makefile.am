bin_PROGRAMS = gnome2-font-properties

gnome2_font_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome2_font_properties_SOURCES = \
	main.c		
gnome2_font_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = font-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = font-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = font-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = font-properties.glade

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(icons_DATA) $(glade_DATA)
