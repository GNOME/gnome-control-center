bin_PROGRAMS = gnome-display-properties

gnome_display_properties_LDADD = $(DISPLAY_CAPPLET_LIBS) $(top_builddir)/capplets/common/libcommon.la
gnome_display_properties_SOURCES = \
	main.c		
gnome_display_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = display-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = display-capplet.png

desktopdir = $(datadir)/applications
Desktop_in_files = display-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(DISPLAY_CAPPLET_CFLAGS)     \
             $(GNOMECC_CAPPLETS_CFLAGS) \
	     -DGLADEDIR=\""$(gladedir)"\" \
	     -DPIXMAPDIR=\""$(pixmapdir)"\"

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(icons_DATA)
