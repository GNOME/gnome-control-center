SUBDIRS = data

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = appearance

bin_PROGRAMS = gnome-appearance-properties

gnome_appearance_properties_SOURCES = \
	appearance.h \
	appearance-desktop.c \
	appearance-desktop.h \
	appearance-font.c \
	appearance-font.h \
	appearance-main.c \
	appearance-themes.c \
	appearance-themes.h \
	appearance-style.c \
	appearance-style.h \
	appearance-ui.c \
	appearance-ui.h \
	gedit-message-area.c \
	gedit-message-area.h \
	gnome-wp-info.c \
	gnome-wp-info.h \
	gnome-wp-item.c \
	gnome-wp-item.h \
	gnome-wp-xml.c \
	gnome-wp-xml.h \
	theme-installer.c \
	theme-installer.h \
	theme-save.c \
	theme-save.h \
	theme-util.c \
	theme-util.h \
	wp-cellrenderer.c \
	wp-cellrenderer.h

gnome_appearance_properties_LDADD = \
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la \
	$(top_builddir)/capplets/common/libcommon.la \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(FONT_CAPPLET_LIBS) \
	$(METACITY_LIBS)
gnome_appearance_properties_LDFLAGS = -export-dynamic

gladedir = $(pkgdatadir)/glade
pixmapdir = $(pkgdatadir)/pixmaps
wallpaperdir = $(datadir)/gnome-background-properties

INCLUDES = \
	$(METACITY_CFLAGS) \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(FONT_CAPPLET_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(gladedir)\"" \
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	-DWALLPAPER_DATADIR="\"$(wallpaperdir)\""

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
