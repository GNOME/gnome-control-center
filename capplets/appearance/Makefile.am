SUBDIRS = data

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = appearance

module_flags = -export_dynamic -avoid-version -module -no-undefined -export-symbols-regex '^g_io_module_(load|unload)'

INCLUDES = \
	$(METACITY_CFLAGS) \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(FONT_CAPPLET_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\"" \
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	-DWALLPAPER_DATADIR="\"$(wallpaperdir)\""

AM_CFLAGS = -DGNOME_DESKTOP_USE_UNSTABLE_API

noinst_LTLIBRARIES = libappearance-common.la

bin_PROGRAMS = gnome-appearance-properties

libappearance_common_la_SOURCES = \
	theme-installer.c \
	theme-installer.h \
	theme-save.c \
	theme-save.h \
	theme-util.c \
	theme-util.h

ccmodulesdir = $(libdir)/control-center-1/extensions
ccmodules_LTLIBRARIES = libappearance.la

libappearance_la_SOURCES = \
	appearance-module.c \
	cc-backgrounds-monitor.h \
	cc-backgrounds-monitor.c \
	cc-background-item.h \
	cc-background-item.c \
	cc-background-page.h \
	cc-background-page.c \
	cc-appearance-panel.h \
	cc-appearance-panel.c

libappearance_la_LDFLAGS = \
	$(module_flags)

libappearance_la_LIBADD = \
	libappearance-common.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(LIBGNOMEKBDUI_LIBS)

libappearance_la_CFLAGS = \
	-I$(top_builddir)/shell \
	-I$(top_srcdir)/shell

gnome_appearance_properties_SOURCES = \
	appearance.h \
	appearance-desktop.c \
	appearance-desktop.h \
	appearance-font.c \
	appearance-font.h \
	appearance-themes.c \
	appearance-themes.h \
	appearance-style.c \
	appearance-style.h \
	gedit-message-area.c \
	gedit-message-area.h \
	gnome-wp-info.c \
	gnome-wp-info.h \
	gnome-wp-item.c \
	gnome-wp-item.h \
	gnome-wp-xml.c \
	gnome-wp-xml.h \
	appearance-main.c

gnome_appearance_properties_LDADD = \
	libappearance-common.la \
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la \
	$(top_builddir)/capplets/common/libcommon.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(FONT_CAPPLET_LIBS) \
	$(METACITY_LIBS)
gnome_appearance_properties_LDFLAGS = -export-dynamic

uidir = $(pkgdatadir)/ui
pixmapdir = $(pkgdatadir)/pixmaps
wallpaperdir = $(datadir)/gnome-background-properties

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)

-include $(top_srcdir)/git.mk
