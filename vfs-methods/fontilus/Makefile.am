
INCLUDES = $(FONT_METHOD_CFLAGS) $(FONT_THUMBNAILER_CFLAGS) $(FONT_VIEW_CFLAGS) \
  $(FONT_CONTEXT_MENU_CFLAGS) -DDIRECTORY_DIR=\"$(vfsdirectorydir)\" 

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libfont-method.la

bin_PROGRAMS = gnome-thumbnail-font gnome-font-viewer

libfont_method_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'vfs_module_.*'
libfont_method_la_LIBADD = $(FONT_METHOD_LIBS)
libfont_method_la_SOURCES = font-method.c

gnome_thumbnail_font_LDADD = $(FONT_THUMBNAILER_LIBS)
gnome_thumbnail_font_SOURCES = ftstream-vfs.c thumbnailer.c

gnome_font_viewer_LDADD = $(FONT_VIEW_LIBS)
gnome_font_viewer_SOURCES = ftstream-vfs.c font-view.c

nautilusdir = $(libdir)/nautilus/extensions-1.0
nautilus_LTLIBRARIES = libnautilus-fontilus.la
libnautilus_fontilus_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'nautilus_module_.*'
libnautilus_fontilus_la_LIBADD = $(FONT_CONTEXT_MENU_LIBS)
libnautilus_fontilus_la_SOURCES = fontilus-context-menu.c

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = font-method.conf

vfsdirectorydir = $(datadir)/gnome/vfolders
vfsdirectory_DATA = font-method.directory

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

install-desktop-database:
	update-desktop-database $(DESTDIR)$(desktopdir)

@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_DIRECTORY_RULE@
@INTLTOOL_SCHEMAS_RULE@

CLEANFILES = $(desktop_in_files) $(desktop_DATA) \
  $(schemas_DATA) $(vfsdirectory_DATA)

EXTRA_DIST = \
  font-method.conf \
  font-method.directory.in \
  fontilus.schemas.in \
  gnome-font-viewer.desktop.in.in
