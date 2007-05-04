# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = appearance

WALLPAPER_DATADIR = $(datadir)/gnome-background-properties

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
	appearance-ui.c \
	appearance-ui.h \
	gnome-wp-info.c \
	gnome-wp-info.h \
	gnome-wp-item.c \
	gnome-wp-item.h \
	gnome-wp-utils.c \
	gnome-wp-utils.h \
	gnome-wp-xml.c \
	gnome-wp-xml.h

gnome_appearance_properties_LDADD = \
	$(GNOMECC_CAPPLETS_LIBS) 					\
	$(FONT_CAPPLET_LIBS) 						\
	$(METACITY_LIBS)						\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la  \
	$(top_builddir)/capplets/common/libcommon.la
gnome_appearance_properties_LDFLAGS = -export-dynamic

gladedir = $(pkgdatadir)/glade
glade_DATA = appearance.glade

pixmapdir = $(pkgdatadir)/pixmaps
pixmap_DATA =                                   \
        subpixel-rgb.png                        \
        subpixel-bgr.png                        \
        subpixel-vrgb.png                       \
        subpixel-vbgr.png

INCLUDES = 				\
	$(METACITY_CFLAGS)		\
	$(GNOMECC_CAPPLETS_CFLAGS)	\
	-DGNOMECC_GLADE_DIR=\""$(gladedir)"\"	\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	-DWALLPAPER_DATADIR=\""$(WALLPAPER_DATADIR)"\"

EXTRA_DIST = $(glade_DATA) $(pixmap_DATA)
