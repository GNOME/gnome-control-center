# This is used in PANEL_CFLAGS
cappletname = keyboard

noinst_LTLIBRARIES = libkeyboard.la

BUILT_SOURCES =			\
	cc-keyboard-resources.c	\
	cc-keyboard-resources.h

libkeyboard_la_SOURCES =   \
	$(BUILT_SOURCES)		\
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
libkeyboard_la_LIBADD = $(PANEL_LIBS) $(KEYBOARD_PANEL_LIBS) $(top_builddir)/panels/common/liblanguage.la

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/keyboard.gresource.xml)
cc-keyboard-resources.c: keyboard.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_keyboard $<
cc-keyboard-resources.h: keyboard.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_keyboard $<

@INTLTOOL_DESKTOP_RULE@

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
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""

CLEANFILES = \
	$(Desktop_in_files) \
	$(desktop_DATA) \
	$(xml_DATA) \
	$(BUILT_SOURCES)
EXTRA_DIST = $(xml_in_files) gnome-keybindings.pc.in $(resource_files) keyboard.gresource.xml

-include $(top_srcdir)/git.mk
