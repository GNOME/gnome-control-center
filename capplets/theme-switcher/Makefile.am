SUBDIRS = control

bin_PROGRAMS = gnome-font-and-theme-properties

gnome_font_and_theme_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_font_and_theme_properties_SOURCES = \
	gnome-font-and-theme-properties.c
gnome_font_and_theme_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = gtk-theme-selector.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = gtk-theme-selector.glade

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(glade_DATA)
