
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

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = fontilus.keys fontilus.mime

appregdir = $(datadir)/application-registry
appreg_DATA = fontilus.applications

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = fontilus.schemas

desktopdir = $(datadir)/applications
desktop_in_files = gnome-font-viewer.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/fontilus.schemas; \
	fi
else
install-data-local:
endif

@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_DIRECTORY_RULE@
@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = \
  font-method.conf \
  font-method.directory.in \
  fontilus.keys \
  fontilus.mime \
  fontilus.applications \
  fontilus.schemas	\
  fontilus.schemas.in \
  gnome-font-viewer.desktop.in
