WALLPAPER_DATADIR = $(datadir)/gnome-wallpaper-properties

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) -DWALLPAPER_DATADIR=\""$(WALLPAPER_DATADIR)"\"

bin_PROGRAMS = gnome-wallpaper-properties

noinst_LTLIBRARIES = libgnomewp.la

gnome_wallpaper_properties_LDADD = \
	$(GNOMECC_CAPPLETS_LIBS) \
	libgnomewp.la

gnome_wallpaper_properties_SOURCES = \
	gnome-wp-capplet.c gnome-wp-capplet.h \
	gnome-wp-info.c gnome-wp-info.h

libgnomewp_la_SOURCES = \
	gnome-wp-item.c gnome-wp-item.h \
	gnome-wp-utils.c gnome-wp-utils.h \
	gnome-wp-xml.c gnome-wp-xml.h

libgnomewp_la_LIBADD = \
	$(GNOMECC_CAPPLETS_LIBS)

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_in_files = background.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

install-data-local:
	$(mkinstalldirs) $(WALLPAPER_DATADIR)

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

MAINTAINERCLEANFILES = \
	Makefile.in

