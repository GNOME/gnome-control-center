bin_PROGRAMS = gnome-theme-properties

gnome_theme_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_theme_properties_SOURCES = \
	theme-switcher.c
gnome_theme_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = gtk-theme-selector.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = theme-properties.glade

INCLUDES   = \
	$(GNOMECC_CAPPLETS_CFLAGS)	\
	-DGLADEDIR=\""$(gladedir)"\"

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(glade_DATA)
