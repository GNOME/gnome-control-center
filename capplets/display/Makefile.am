# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = display

bin_PROGRAMS = gnome-display-properties

gnome_display_properties_LDADD = $(DISPLAY_CAPPLET_LIBS) $(top_builddir)/capplets/common/libcommon.la
gnome_display_properties_SOURCES = \
	main.c		
gnome_display_properties_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

icondir = $(datadir)/icons/hicolor/48x48/apps
dist_icon_DATA = display-capplet.png

desktopdir = $(datadir)/applications
Desktop_in_files = display-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(DISPLAY_CAPPLET_CFLAGS)     \
             $(GNOMECC_CAPPLETS_CFLAGS) \
	     -DGLADEDIR=\""$(gladedir)"\" \
	     -DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	     -DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)

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

