bin_PROGRAMS = gnome2-ui-properties

gnome2_ui_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome2_ui_properties_SOURCES = \
	gnome-ui-properties.c

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)/Advanced
Desktop_in_files = behavior.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome2-ui-properties.glade
INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(Desktop_in_files)
