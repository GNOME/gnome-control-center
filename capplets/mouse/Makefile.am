bin_PROGRAMS = gnome-mouse-properties

gnome_mouse_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)

gnome_mouse_properties_SOURCES = \
	mouse-properties-capplet.c

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	double-click-on.png	\
	double-click-off.png	\
	double-click-maybe.png	\
	mouse-left.png		\
	mouse-right.png

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-mouse-properties.glade

##
## You should not need to modify anything below this line
##
@INTLTOOL_DESKTOP_RULE@

## install-data-am: install-data-local

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
