WALLPAPER_DATADIR = $(datadir)/gnome-background-properties

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DWALLPAPER_DATADIR=\""$(WALLPAPER_DATADIR)"\"

bin_PROGRAMS = gnome-background-properties

noinst_LTLIBRARIES = libgnomewp.la

gnome_background_properties_LDADD = \
	$(GNOMECC_CAPPLETS_LIBS) \
	libgnomewp.la

gnome_background_properties_SOURCES = \
	gnome-wp-capplet.c gnome-wp-capplet.h \
	gnome-wp-info.c gnome-wp-info.h

libgnomewp_la_SOURCES = \
	gnome-wp-item.c gnome-wp-item.h \
	gnome-wp-utils.c gnome-wp-utils.h \
	gnome-wp-xml.c gnome-wp-xml.h

libgnomewp_la_LIBADD = \
	$(GNOMECC_CAPPLETS_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = background.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = gnome-background-properties.glade

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(WALLPAPER_DATADIR)

EXTRA_DIST = \
	$(desktop_in_files) \
	$(glade_DATA)

CLEANFILES = \
	$(desktop_DATA)

MAINTAINERCLEANFILES = \
	Makefile.in

