# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = network

bin_PROGRAMS = gnome-network-properties

gnome_network_properties_SOURCES = gnome-network-properties.c
gnome_network_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)

@INTLTOOL_DESKTOP_RULE@

gladedir = $(pkgdatadir)/glade
dist_glade_DATA = gnome-network-properties.glade

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/gnome-network-properties.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/gnome-network-properties.png
icons24dir = $(datadir)/icons/hicolor/24x24/apps
dist_icons24_DATA = icons/24x24/gnome-network-properties.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/gnome-network-properties.png
icons48dir = $(datadir)/icons/hicolor/48x48/apps
dist_icons48_DATA = icons/48x48/gnome-network-properties.png
iconssvgdir = $(datadir)/icons/hicolor/scalable/apps
dist_iconssvg_DATA = icons/scalable/gnome-network-properties.svg

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\""

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
uninstall-hook: update-icon-cache
install-data-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
