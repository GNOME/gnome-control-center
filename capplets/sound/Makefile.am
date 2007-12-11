# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = sound

bin_PROGRAMS = gnome-sound-properties

gnome_sound_properties_LDADD = \
	$(top_builddir)/libsounds/libsounds.a \
	$(GNOMECC_CAPPLETS_LIBS) \
	$(SOUND_CAPPLET_LIBS) \
	$(HAL_LIBS)
gnome_sound_properties_SOURCES = sound-properties-capplet.c pipeline-tests.h pipeline-tests.c mixer-support.h mixer-support.c

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(SOUND_CAPPLET_CFLAGS) \
	$(HAL_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\""

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(pkgdatadir)/glade
dist_glade_DATA = sound-properties.glade

pixmapdir = $(pkgdatadir)/pixmaps
dist_pixmap_DATA = visual-bell.png

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/gnome-sound-properties.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/gnome-sound-properties.png
icons24dir = $(datadir)/icons/hicolor/24x24/apps
dist_icons24_DATA = icons/24x24/gnome-sound-properties.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/gnome-sound-properties.png
icons48dir = $(datadir)/icons/hicolor/48x48/apps
dist_icons48_DATA = icons/48x48/gnome-sound-properties.png
iconssvgdir = $(datadir)/icons/hicolor/scalable/apps
dist_iconssvg_DATA = icons/scalable/gnome-sound-properties.svg

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-settings-sound.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

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

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
