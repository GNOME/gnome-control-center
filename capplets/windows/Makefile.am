bin_PROGRAMS = gnome-window-properties

gnome_window_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) 	\
		$(top_builddir)/libwindow-settings/libwindow-settings.la

gnome_window_properties_SOURCES = 		\
	gnome-window-properties.c		\
	wm-properties.h				\
	wm-exec.c				\
	wm-list.c

wms_flags = -export_dynamic -avoid-version
wmsdir = $(libdir)/window-manager-settings/

wms_LTLIBRARIES =			\
	libmetacity.la

libmetacity_la_SOURCES = 		\
	metacity-window-manager.c	\
	metacity-window-manager.h


libmetacity_la_LDFLAGS = $(wms_flags)
libmetacity_la_LIBADD =


@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA =

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-window-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = window-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = window-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = window-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(DESKTOP_ITEM_CFLAGS) 					\
	     -I $(top_builddir)/libwindow-settings \
	     -DGNOME_WINDOW_MANAGER_MODULE_PATH=\""$(libdir)/window-manager-settings"\"
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files) $(pixmap_DATA)

