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
	gnome-wp-info.c \
	gnome-wp-info.h \
	gnome-wp-item.c \
	gnome-wp-item.h \
	gnome-wp-utils.c \
	gnome-wp-utils.h \
	gnome-wp-xml.c \
	gnome-wp-xml.h \
	wp-cellrenderer.c \
	wp-cellrenderer.h

gnome_appearance_properties_LDADD = \
	$(GNOMECC_CAPPLETS_LIBS) 					\
	$(FONT_CAPPLET_LIBS) 						\
	$(METACITY_LIBS)						\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la  \
	$(top_builddir)/capplets/common/libcommon.la
gnome_appearance_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

gladedir = $(pkgdatadir)/glade
glade_DATA = appearance.glade

pixmapdir = $(pkgdatadir)/pixmaps
pixmap_DATA =             \
        theme-thumbnailing.png  \        
        subpixel-bgr.png  \
        subpixel-rgb.png  \
        subpixel-vbgr.png \
        subpixel-vrgb.png

wallpaperdir = $(datadir)/gnome-background-properties

desktopdir = $(datadir)/applications
desktop_in_files = gnome-appearance-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

INCLUDES = 						\
	$(METACITY_CFLAGS)				\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	$(FONT_CAPPLET_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_GLADE_DIR="\"$(gladedir)\""		\
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\""		\
	-DWALLPAPER_DATADIR="\"$(wallpaperdir)\""

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(glade_DATA) $(pixmap_DATA)
