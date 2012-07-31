# This is used in PANEL_CFLAGS
cappletname = keyboard

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libkeyboard.la

libkeyboard_la_SOURCES =   \
	keyboard-module.c		\
	cc-keyboard-panel.c		\
	cc-keyboard-panel.h		\
	cc-keyboard-item.c		\
	cc-keyboard-item.h		\
	cc-keyboard-option.c		\
	cc-keyboard-option.h		\
	wm-common.c			\
	wm-common.h			\
	keyboard-general.c		\
	keyboard-general.h		\
	keyboard-shortcuts.c		\
	keyboard-shortcuts.h

libkeyboard_la_CFLAGS = $(PANEL_CFLAGS) $(KEYBOARD_PANEL_CFLAGS) -I$(top_srcdir)/panels/common/
libkeyboard_la_LIBADD = $(PANEL_LIBS) $(KEYBOARD_PANEL_LIBS)
libkeyboard_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-keyboard-panel.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-keyboard-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_NOMERGE_RULE@

xmldir       = $(pkgdatadir)/keybindings
xml_in_files = 			\
	00-multimedia.xml.in 	\
	01-input-sources.xml.in	\
	01-launchers.xml.in 	\
	01-screenshot.xml.in 	\
	01-system.xml.in 	\
	50-accessibility.xml.in
xml_DATA     = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = gnome-keybindings.pc

INCLUDES = \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\"" \
	-DGNOMECC_KEYBINDINGS_DIR="\"$(pkgdatadir)/keybindings\""

CLEANFILES = \
	$(Desktop_in_files) \
	$(desktop_DATA) \
	$(xml_DATA)
EXTRA_DIST = $(ui_DATA) $(xml_in_files) gnome-keybindings.pc.in


-include $(top_srcdir)/git.mk
