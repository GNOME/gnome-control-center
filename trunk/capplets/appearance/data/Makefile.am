
gladedir = $(pkgdatadir)/glade
dist_glade_DATA = appearance.glade

pixmapdir = $(pkgdatadir)/pixmaps
dist_pixmap_DATA = \
        subpixel-bgr.png \
        subpixel-rgb.png \
        subpixel-vbgr.png \
        subpixel-vrgb.png \
        theme-thumbnailing.png \
        gtk-theme-thumbnailing.png \
        window-theme-thumbnailing.png \
        icon-theme-thumbnailing.png \
        mouse-cursor-normal.png \
        mouse-cursor-normal-large.png \
        mouse-cursor-white.png \
        mouse-cursor-white-large.png

cursorfontdir   = $(datadir)/gnome/cursor-fonts
dist_cursorfont_DATA = \
	cursor-large.pcf		\
	cursor-white.pcf		\
	cursor-large-white.pcf

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


install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif


EXTRA_DIST = $(xml_in_files)


CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(mime_DATA)
