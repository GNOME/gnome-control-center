bin_PROGRAMS = gnome-font-properties

gnome_font_properties_LDADD = $(FONT_CAPPLET_LIBS) $(top_builddir)/capplets/common/libcommon.la
gnome_font_properties_SOURCES = \
	main.c		
gnome_font_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = font-capplet.png

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA =					\
	subpixel-rgb.png			\
	subpixel-bgr.png			\
	subpixel-vrgb.png			\
	subpixel-vbgr.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = font-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = font-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = font-properties.glade

INCLUDES   = $(FONT_CAPPLET_CFLAGS)     \
             $(GNOMECC_CAPPLETS_CFLAGS) \
	     -DGLADEDIR=\""$(gladedir)"\" \
	     -DPIXMAPDIR=\""$(pixmapdir)"\"
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(icons_DATA) $(glade_DATA) $(pixmap_DATA)
