# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keyboard

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""
	$(NULL)


ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libkeyboard-properties.la

libkeyboard_properties_la_SOURCES =	\
		keyboard-module.c \
		cc-keyboard-panel.c \
		cc-keyboard-panel.h \
	        gnome-keyboard-properties.c \
	        gnome-keyboard-properties-a11y.c \
	        gnome-keyboard-properties-a11y.h \
	        gnome-keyboard-properties-xkb.c \
	        gnome-keyboard-properties-xkbmc.c \
	        gnome-keyboard-properties-xkblt.c \
	        gnome-keyboard-properties-xkbltadd.c \
	        gnome-keyboard-properties-xkbot.c \
	        gnome-keyboard-properties-xkbpv.c \
	        gnome-keyboard-properties-xkb.h

libkeyboard_properties_la_LIBADD = $(PANEL_LIBS) $(LIBGNOMEKBDUI_LIBS)

libkeyboard_properties_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-keyboard-properties-a11y-notifications.ui \
		gnome-keyboard-properties-dialog.ui \
		gnome-keyboard-properties-layout-chooser.ui \
		gnome-keyboard-properties-model-chooser.ui \
		gnome-keyboard-properties-options-dialog.ui

desktopdir = $(datadir)/applications
Desktop_in_files = keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES = \
           $(GNOMECC_CAPPLETS_CFLAGS) \
           $(LIBGNOMEKBDUI_CFLAGS) \
           -DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
           -DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
           -DGNOMECC_UI_DIR="\"$(uidir)\""
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA)


-include $(top_srcdir)/git.mk
