bin_PROGRAMS = gnome-font-properties

gnome_font_properties_LDADD = \
	$(top_builddir)/capplets/common/libcommon.la \
	$(FONT_CAPPLET_LIBS)
gnome_font_properties_SOURCES = \
	main.c		
gnome_font_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA =					\
	subpixel-rgb.png			\
	subpixel-bgr.png			\
	subpixel-vrgb.png			\
	subpixel-vbgr.png

desktopdir = $(datadir)/applications
Desktop_in_files = font-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = font-properties.glade

INCLUDES   = $(FONT_CAPPLET_CFLAGS)     \
             $(GNOMECC_CAPPLETS_CFLAGS) \
	     -DGLADEDIR=\""$(gladedir)"\" \
	     -DPIXMAPDIR=\""$(pixmapdir)"\"
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(glade_DATA) $(pixmap_DATA)
