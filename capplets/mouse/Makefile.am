bin_PROGRAMS = gnome2-mouse-properties

gnome2_mouse_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome2_mouse_properties_SOURCES = gnome-mouse-properties.c

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	double-click-on.png		\
	double-click-off.png		\
	double-click-maybe.png		\
	mouse-left.png			\
	mouse-right.png			\
	mouse-cursor-normal.png		\
	mouse-cursor-white.png		\
	mouse-cursor-normal-large.png	\
	mouse-cursor-white-large.png

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-mouse-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = mouse-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = mouse-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = mouse.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files) $(pixmap_DATA)

