bin_PROGRAMS = gnome2-ui-properties

gnome2_ui_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome2_ui_properties_SOURCES = \
	prefs-widget.c			prefs-widget.h\
	prefs-widget-mdi.c		prefs-widget-mdi.h\
	prefs-widget-dialogs.c	prefs-widget-dialogs.h\
	prefs-widget-app.c		prefs-widget-app.h\
	preferences.c			preferences.h\
	main.c

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DATA_DIR)/capplets/Advanced
Desktop_in_files = behavior.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
Gladedir   = $(GNOMECC_DATA_DIR)/interfaces
Glade_DATA = behavior-properties.glade
INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(Desktop_in_files)
