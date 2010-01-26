NULL =

SUBDIRS = data

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = appearance

INCLUDES = \
	$(METACITY_CFLAGS) \
	-I$(top_srcdir)/libgnome-control-center-extension	\
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

ccmodulesdir = $(EXTENSIONSDIR)
ccmodules_LTLIBRARIES = libappearance.la

libappearance_la_SOURCES =		\
	appearance-module.c		\
	cc-backgrounds-monitor.h	\
	cc-backgrounds-monitor.c	\
	cc-background-item.h		\
	cc-background-item.c		\
	cc-background-page.h		\
	cc-background-page.c		\
	cc-theme-save-dialog.h		\
	cc-theme-save-dialog.c		\
	cc-theme-page.h			\
	cc-theme-page.c			\
	cc-appearance-panel.h		\
	cc-appearance-panel.c		\
	$(NULL)

libappearance_la_LDFLAGS =		\
	$(EXTENSION_LIBTOOL_FLAGS)	\
	$(NULL)

libappearance_la_LIBADD =		\
	libappearance-common.la		\
	$(EXTENSION_LIBS)		\
	$(EXTENSION_COMMON_LIBS)	\
	$(NULL)

libappearance_la_CFLAGS =		\
	$(EXTENSION_CFLAGS)		\
	$(EXTENSION_COMMON_CFLAGS)	\
	$(NULL)

libappearance_la_LIBTOOLFLAGS = --tag=disable-static

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
	$(top_builddir)/libgnome-control-center-extension/libcommon.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(FONT_CAPPLET_LIBS) \
	$(METACITY_LIBS)
gnome_appearance_properties_LDFLAGS = -export-dynamic

uidir = $(pkgdatadir)/ui
pixmapdir = $(pkgdatadir)/pixmaps
wallpaperdir = $(datadir)/gnome-background-properties

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)

-include $(top_srcdir)/git.mk
