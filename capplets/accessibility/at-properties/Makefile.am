bin_PROGRAMS = gnome-at-properties

gnome_at_properties_LDADD = $(AT_CAPPLET_LIBS) $(top_builddir)/capplets/common/libcommon.la
gnome_at_properties_SOURCES = \
	main.c		\
	at-startup-session.h	\
	at-startup-session.c
gnome_at_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = at-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA =					\
	at-startup.png				\
	at-support.png

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = at-enable-dialog.glade

INCLUDES   = $(AT_CAPPLET_CFLAGS)     \
             $(GNOMECC_CAPPLETS_CFLAGS) \
	     -DGLADEDIR=\""$(gladedir)"\" \
	     -DPIXMAPDIR=\""$(pixmapdir)"\"
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(glade_DATA) $(pixmap_DATA) $(Desktop_in_files)
