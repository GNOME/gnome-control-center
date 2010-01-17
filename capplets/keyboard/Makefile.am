NULL =

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keyboard

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(LIBGNOMEKBDUI_CFLAGS) \
	-I$(top_srcdir)/capplets/common/ \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_KEYBINDINGS_DIR="\"$(pkgdatadir)/keybindings\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\""

bin_PROGRAMS = gnome-keyboard-properties

ccmodulesdir = $(EXTENSIONSDIR)
ccmodules_LTLIBRARIES = libkeyboard.la

libkeyboard_la_SOURCES = \
	keyboard-module.c \
	eggcellrendererkeys.c \
	eggcellrendererkeys.h \
	eggaccelerators.c \
	eggaccelerators.h \
	cc-shortcuts-page.h \
	cc-shortcuts-page.c \
	cc-keyboard-page.h \
	cc-keyboard-page.c \
	cc-keyboard-panel.h \
	cc-keyboard-panel.c

libkeyboard_la_LDFLAGS =		\
	$(EXTENSION_LIBTOOL_FLAGS)	\
	$(NULL)

libkeyboard_la_LIBADD =			\
	$(EXTENSION_LIBS)		\
	$(EXTENSION_COMMON_LIBS)	\
	$(LIBGNOMEKBDUI_LIBS)		\
	$(NULL)

libkeyboard_la_CFLAGS =			\
	$(EXTENSION_CFLAGS)		\
	$(EXTENSION_COMMON_CFLAGS)	\
	$(NULL)

libkeyboard_la_LIBTOOLFLAGS = --tag=disable-static

gnome_keyboard_properties_SOURCES = \
	gnome-keyboard-properties-a11y.c \
	gnome-keyboard-properties-a11y.h \
	gnome-keyboard-properties-xkb.c \
	gnome-keyboard-properties-xkbmc.c \
	gnome-keyboard-properties-xkblt.c \
	gnome-keyboard-properties-xkbltadd.c \
	gnome-keyboard-properties-xkbot.c \
	gnome-keyboard-properties-xkbpv.c \
	gnome-keyboard-properties-xkb.h \
	gnome-keyboard-properties.c

gnome_keyboard_properties_LDADD = \
	$(top_builddir)/capplets/common/libcommon.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(LIBGNOMEKBDUI_LIBS)

gnome_keyboard_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
dist_ui_DATA = gnome-keyboard-properties-a11y-notifications.ui \
               gnome-keyboard-properties-dialog.ui \
               gnome-keyboard-properties-layout-chooser.ui \
               gnome-keyboard-properties-model-chooser.ui \
               gnome-keyboard-properties-options-dialog.ui

desktopdir = $(datadir)/applications
Desktop_in_files = keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_NOMERGE_RULE@

xmldir       = $(pkgdatadir)/keybindings
xml_in_files = 00-multimedia-key.xml.in 01-desktop-key.xml.in
xml_DATA     = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = gnome-keybindings.pc

CLEANFILES = \
	$(GNOMECC_CAPPLETS_CLEANFILES) \
	$(Desktop_in_files) \
	$(desktop_DATA) \
	$(xml_DATA) \
	$(NULL)

EXTRA_DIST = $(ui_DATA) $(xml_in_files) gnome-keybindings.pc.in

-include $(top_srcdir)/git.mk
