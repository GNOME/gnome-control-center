bin_PROGRAMS = gnome-window-properties

gnome_window_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) 	\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la

gnome_window_properties_SOURCES = 		\
	gnome-window-properties.c

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA =

gladedir   = $(GNOMECC_GLADE_DIR)
glade_DATA = gnome-window-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = window-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = window-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = window-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)					\
	     -I $(top_srcdir)/libwindow-settings \
	     -DGNOME_WINDOW_MANAGER_MODULE_PATH=\""$(libdir)/window-manager-settings"\" \
	     -DGLADEDIR=\""$(gladedir)"\"  	\
	     -DPIXMAPDIR=\""$(pixmapdir)"\"

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(glade_DATA) $(icons_DATA) $(Desktop_in_files) $(pixmap_DATA)

