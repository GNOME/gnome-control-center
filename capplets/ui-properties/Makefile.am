bin_PROGRAMS = ui-properties

ui_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
ui_properties_SOURCES = \
	prefs-widget.c			prefs-widget.h\
	prefs-widget-mdi.c		prefs-widget-mdi.h\
	prefs-widget-dialogs.c	prefs-widget-dialogs.h\
	prefs-widget-app.c		prefs-widget-app.h\
	preferences.c			preferences.h\
	main.c

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DATA_DIR)/capplets/Advanced
desktop_DATA = behavior.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(desktop_DATA)
