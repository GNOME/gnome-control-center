# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keybinding

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libkeybinding-properties.la

libkeybinding_properties_la_SOURCES =   \
	keybindings-module.c		\
	cc-keybindings-panel.c		\
	cc-keybindings-panel.h		\
	wm-common.c			\
	wm-common.h			\
	gnome-keybinding-properties.c	\
	eggcellrendererkeys.c		\
	eggcellrendererkeys.h		\
	eggaccelerators.c		\
	eggaccelerators.h

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-keybinding-properties.ui

desktopdir = $(datadir)/applications
Desktop_in_files = keybinding.desktop.in
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
