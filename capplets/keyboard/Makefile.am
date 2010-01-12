# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = keyboard

module_flags = -export_dynamic -avoid-version -module -no-undefined -export-symbols-regex '^g_io_module_(load|unload)'

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(LIBGNOMEKBDUI_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\""

noinst_LTLIBRARIES = libkeyboard-common.la

bin_PROGRAMS = gnome-keyboard-properties

libkeyboard_common_la_SOURCES = \
	gnome-keyboard-properties-a11y.c \
	gnome-keyboard-properties-a11y.h \
	gnome-keyboard-properties-xkb.c \
	gnome-keyboard-properties-xkbmc.c \
	gnome-keyboard-properties-xkblt.c \
	gnome-keyboard-properties-xkbltadd.c \
	gnome-keyboard-properties-xkbot.c \
	gnome-keyboard-properties-xkbpv.c \
	gnome-keyboard-properties-xkb.h

libkeyboard_common_la_LDFLAGS = \
       -no-undefined -export_dynamic -avoid-version

ccmodulesdir = $(libdir)/control-center-1/extensions
ccmodules_LTLIBRARIES = libkeyboard.la

libkeyboard_la_SOURCES = \
	keyboard-module.c \
	cc-keyboard-panel.h \
	cc-keyboard-panel.c

libkeyboard_la_LDFLAGS = \
	$(module_flags)

libkeyboard_la_LIBADD = \
	libkeyboard-common.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(LIBGNOMEKBDUI_LIBS)

libkeyboard_la_CFLAGS = \
	-I$(top_builddir)/shell \
	-I$(top_srcdir)/shell

gnome_keyboard_properties_SOURCES = \
	gnome-keyboard-properties.c

gnome_keyboard_properties_LDADD = \
	libkeyboard-common.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(LIBGNOMEKBDUI_LIBS)

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

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA)

-include $(top_srcdir)/git.mk
