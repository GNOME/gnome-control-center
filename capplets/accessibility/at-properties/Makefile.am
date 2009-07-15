bin_PROGRAMS = gnome-at-properties

gnome_at_properties_LDADD = $(AT_CAPPLET_LIBS) $(GNOMECC_CAPPLETS_LIBS) $(top_builddir)/capplets/common/libcommon.la
gnome_at_properties_SOURCES = \
	main.c
gnome_at_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = at-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

pixmapdir  = $(pkgdatadir)/pixmaps
pixmap_DATA =					\
	at-startup.png				\
	at-support.png

uidir = $(pkgdatadir)/ui
ui_DATA = at-enable-dialog.ui

INCLUDES   = $(AT_CAPPLET_CFLAGS)     \
             $(GNOMECC_CAPPLETS_CFLAGS) \
	     -DUIDIR=\""$(uidir)"\" \
	     -DPIXMAPDIR=\""$(pixmapdir)"\" \
	     -DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	     -DGNOMELOCALEDIR="\"$(datadir)/locale\""
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
