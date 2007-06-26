SUBDIRS = libuuid

bin_PROGRAMS = gnome-file-types-properties

gnome_file_types_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(VFS_CAPPLET_LIBS) libuuid/libuuid.a -lpopt

gnome_file_types_properties_SOURCES =					\
	file-types-capplet.c						\
	mime-types-model.c		mime-types-model.h		\
	model-entry.c			model-entry.h			\
	mime-type-info.c		mime-type-info.h		\
	mime-edit-dialog.c		mime-edit-dialog.h		\
	mime-category-edit-dialog.c	mime-category-edit-dialog.h	\
	service-info.c			service-info.h			\
	service-edit-dialog.c		service-edit-dialog.h

@INTLTOOL_DESKTOP_RULE@

icondir = $(datadir)/icons/hicolor/48x48/apps
dist_icon_DATA = file-types-capplet.png

Gladedir   = $(pkgdatadir)/glade
Glade_DATA = file-types-properties.glade

desktopdir = $(datadir)/applications
Desktop_in_files = file-types.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(VFS_CAPPLET_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(Desktop_in_files) $(Desktop_in_files)

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

