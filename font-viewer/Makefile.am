
INCLUDES = $(FONT_VIEWER_CFLAGS) $(GNOMECC_CAPPLETS_CFLAGS) -DDIRECTORY_DIR=\"$(directorydir)\" \
  -DGNOMELOCALEDIR=\"$(datadir)/locale\"

bin_PROGRAMS = gnome-thumbnail-font gnome-font-viewer

gnome_thumbnail_font_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(FONT_VIEWER_LIBS)
gnome_thumbnail_font_SOURCES = ftstream-vfs.c font-thumbnailer.c totem-resources.c totem-resources.h

gnome_font_viewer_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(FONT_VIEWER_LIBS)
gnome_font_viewer_SOURCES = ftstream-vfs.c font-view.c

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = fontilus.schemas

desktopdir = $(datadir)/applications
desktop_in_files = gnome-font-viewer.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

install-data-local: install-gconf-schemas install-desktop-database

if GCONF_SCHEMAS_INSTALL
install-gconf-schemas:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule fontilus.schemas; \
	fi
else
install-gconf-schemas:
endif

install-desktop-database: install-desktopDATA
	update-desktop-database $(DESTDIR)$(desktopdir)

uninstall-local:
	rm -f $(DESTDIR)$(desktopdir)/mimeinfo.cache

@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_DIRECTORY_RULE@
@INTLTOOL_SCHEMAS_RULE@

CLEANFILES = $(desktop_in_files) $(desktop_DATA) \
  $(schemas_DATA) $(directory_DATA)

EXTRA_DIST = \
  fontilus.schemas.in \
  gnome-font-viewer.desktop.in.in

-include $(top_srcdir)/git.mk
