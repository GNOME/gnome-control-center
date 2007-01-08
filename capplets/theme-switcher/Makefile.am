bin_PROGRAMS = gnome-theme-manager



pixmapdir = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	theme-thumbnailing.png

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
	gnome-theme-installer.h \
	gtkrc-utils.c		\
	gtkrc-utils.h

gnome_theme_manager_LDFLAGS = -export-dynamic


@INTLTOOL_DESKTOP_RULE@

xml_in_files = gnome-theme-package.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

desktopdir = $(datadir)/applications
Desktop_in_files = gtk-theme-selector.desktop.in \
									 gnome-theme-installer.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_GLADE_DIR)
glade_DATA = theme-properties.glade

INCLUDES   = 				\
	-I$(top_srcdir)			\
	$(METACITY_CFLAGS)		\
	$(GNOMECC_CAPPLETS_CFLAGS)	\
	-DGLADEDIR=\""$(gladedir)"\"

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA) $(xml_files)
EXTRA_DIST = $(glade_DATA) gnome-theme-save-data.c $(pixmap_DATA) $(xml_in_files)

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif
