bin_PROGRAMS = gnome-theme-manager

pixmapdir = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	theme-thumbnailing.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = gnome-ccthemes.png

gnome_theme_manager_LDADD = 						\
	$(GNOMECC_CAPPLETS_LIBS) 					\
	$(METACITY_LIBS)						\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la  \
	$(top_builddir)/capplets/common/libcommon.la

gnome_theme_manager_SOURCES =	\
	gnome-theme-save.c	\
	gnome-theme-save.h	\
	gnome-theme-manager.c	\
	gnome-theme-manager.h	\
	gnome-theme-details.c	\
	gnome-theme-details.h	\
	gnome-theme-installer.c	\
	gnome-theme-installer.h

gnome_theme_manager_LDFLAGS = -export-dynamic


@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = gtk-theme-selector.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = theme-properties.glade theme-install.glade

INCLUDES   = 				\
	-I$(top_srcdir)			\
	$(METACITY_CFLAGS)		\
	$(GNOMECC_CAPPLETS_CFLAGS)	\
	-DGLADEDIR=\""$(gladedir)"\"

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(glade_DATA) gnome-theme-save-data.c $(pixmap_DATA) $(desktop_icons_DATA)
