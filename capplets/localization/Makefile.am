bin_PROGRAMS = gnome-localization-properties

gnome_localization_properties_SOURCES =	\
	gnome-localization-properties.c

gnome_localization_properties_LDADD =	\
	$(GNOMECC_CAPPLETS_LIBS)	\
	$(LOCALIZATION_CAPPLET_LIBS)

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-localization-properties.glade

icondir = $(datadir)/icons/hicolor/48x48/apps
dist_icon_DATA = localization-capplet.png

desktopdir = $(datadir)/applications
desktop_in_files = localization.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(glade_DATA) $(pixmaps_DATA) $(desktop_in_files)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

