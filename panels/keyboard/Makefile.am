# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keyboard

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libkeyboard.la

libkeyboard_la_SOURCES =   \
	keyboard-module.c		\
	cc-keyboard-panel.c		\
	cc-keyboard-panel.h		\
	wm-common.c			\
	wm-common.h			\
	gnome-keyboard-panel.c		\
	gnome-keyboard-panel.h		\
	eggcellrendererkeys.c		\
	eggcellrendererkeys.h		\
	eggaccelerators.c		\
	eggaccelerators.h

libkeyboard_la_CFLAGS = $(PANEL_CFLAGS)
libkeyboard_la_LIBADD = $(PANEL_LIBS)
libkeyboard_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-keyboard-panel.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-keyboard-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_NOMERGE_RULE@

xmldir       = $(pkgdatadir)/keybindings
xml_in_files = 00-multimedia-key.xml.in 01-desktop-key.xml.in
xml_DATA     = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = gnome-keybindings.pc

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\"" \
	-DGNOMECC_KEYBINDINGS_DIR="\"$(pkgdatadir)/keybindings\""
CLEANFILES = \
	$(GNOMECC_CAPPLETS_CLEANFILES) \
	$(Desktop_in_files) \
	$(desktop_DATA) \
	$(xml_DATA)
EXTRA_DIST = $(ui_DATA) $(xml_in_files) gnome-keybindings.pc.in


-include $(top_srcdir)/git.mk
