
INCLUDES = $(FONT_METHOD_CFLAGS) $(THUMBNAILER_CFLAGS) $(FONT_VIEW_CFLAGS) \
  $(CONTEXT_MENU_CFLAGS) -DDIRECTORY_DIR=\"$(vfsdirectorydir)\" 

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libfont-method.la

bin_PROGRAMS = gnome-thumbnail-font gnome-font-viewer

libfont_method_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'vfs_module_.*'
libfont_method_la_LIBADD = $(FONT_METHOD_LIBS)
libfont_method_la_SOURCES = font-method.c

gnome_thumbnail_font_LDADD = $(THUMBNAILER_LIBS)
gnome_thumbnail_font_SOURCES = ftstream-vfs.c thumbnailer.c

gnome_font_viewer_LDADD = $(FONT_VIEW_LIBS)
gnome_font_viewer_SOURCES = ftstream-vfs.c font-view.c

libexec_PROGRAMS = fontilus-context-menu
fontilus_context_menu_LDADD = $(CONTEXT_MENU_LIBS)
fontilus_context_menu_SOURCES = fontilus-context-menu.c

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = font-method.conf

vfsdirectorydir = $(datadir)/gnome/vfolders
vfsdirectory_DATA = font-method.directory

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = fontilus.keys fontilus.mime

appregdir = $(datadir)/application-registry
appreg_DATA = fontilus.applications

schemasdir = $(sysconfdir)/gconf/schemas
schemas_DATA = fontilus.schemas

serverdir = $(libdir)/bonobo/servers
server_DATA = fontilus.server
fontilus.server.in: fontilus.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule fontilus.schemas; \
	fi

@INTLTOOL_DIRECTORY_RULE@
@INTLTOOL_SCHEMAS_RULE@
@INTLTOOL_SERVER_RULE@

EXTRA_DIST = \
  font-method.conf \
  font-method.directory.in \
  fontilus.keys \
  fontilus.mime \
  fontilus.applications \
  fontilus.schemas.in
