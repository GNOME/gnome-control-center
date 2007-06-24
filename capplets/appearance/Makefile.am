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
	theme-installer.c \
	theme-installer.h \
	theme-save.c \
	theme-save.h \
	theme-util.c \
	theme-util.h \
	wp-cellrenderer.c \
	wp-cellrenderer.h

gnome_appearance_properties_LDADD = \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(FONT_CAPPLET_LIBS) \
	$(METACITY_LIBS) \
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la \
	$(top_builddir)/capplets/common/libcommon.la
gnome_appearance_properties_LDFLAGS = -export-dynamic

gladedir = $(pkgdatadir)/glade
dist_glade_DATA = appearance.glade

pixmapdir = $(pkgdatadir)/pixmaps
dist_pixmap_DATA = \
        subpixel-bgr.png \
        subpixel-rgb.png \
        subpixel-vbgr.png \
        subpixel-vrgb.png \
        theme-thumbnailing.png

wallpaperdir = $(datadir)/gnome-background-properties

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = \
	gnome-appearance-properties.desktop.in \
	gnome-theme-installer.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_RULE@

xml_in_files = gnome-theme-package.xml.in

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_in_files:.xml.in=.xml)

INCLUDES = \
	$(METACITY_CFLAGS) \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(FONT_CAPPLET_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(gladedir)\"" \
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	-DWALLPAPER_DATADIR="\"$(wallpaperdir)\""

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_in_files) $(desktop_DATA) $(mime_DATA)
EXTRA_DIST = $(xml_in_files)

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif
