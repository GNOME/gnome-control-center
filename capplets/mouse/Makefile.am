bin_PROGRAMS = gnome2-mouse-properties

gnome2_mouse_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome2_mouse_properties_SOURCES = gnome-mouse-properties.c

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	double-click-on.png	\
	double-click-off.png	\
	double-click-maybe.png	\
	mouse-left.png		\
	mouse-right.png

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-mouse-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = mouse-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_DATA = mouse.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(desktop_DATA) $(pixmap_DATA)
