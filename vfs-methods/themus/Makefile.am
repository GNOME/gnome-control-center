INCLUDES = $(THEME_METHOD_CFLAGS) \
  $(THEMUS_THUMBNAILER_CFLAGS) \
  $(THEMUS_COMPONENT_CFLAGS) \
  -I$(top_srcdir)/capplets/common \
  -DDIRECTORY_DIR=\"$(vfsdirectorydir)\" \
  -DDATA_DIR=\"$(datadir)/themus\"

AM_CFLAGS = -g

bin_PROGRAMS = gnome-theme-thumbnailer themus-theme-applier

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libtheme-method.la

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libthemus-theme-properties-view.la

libtheme_method_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'vfs_module_.*'
libtheme_method_la_LIBADD = $(THEME_METHOD_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libtheme_method_la_SOURCES = theme-method.c

gnome_theme_thumbnailer_LDADD = $(THEMUS_THUMBNAILER_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
gnome_theme_thumbnailer_SOURCES = \
	theme-thumbnailer.c

themus_theme_applier_LDADD = $(THEMUS_COMPONENT_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
themus_theme_applier_SOURCES = \
	themus-theme-applier.c

libthemus_theme_properties_view_la_LIBADD = $(THEMUS_COMPONENT_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libthemus_theme_properties_view_la_LDFLAGS = -module -avoid-version
libthemus_theme_properties_view_la_SOURCES = \
	themus-properties-view.c \
	themus-properties-view.h \
	themus-properties-main.c

pkgdata_DATA = apply-font.glade

@INTLTOOL_DIRECTORY_RULE@

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = theme-method.conf

vfsdirectorydir = $(datadir)/gnome/vfolders
vfsdirectory_DATA = theme-method.directory

schemasdir       = $(sysconfdir)/gconf/schemas
schemas_in_files = themus.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = themus.keys

appregdir = $(datadir)/application-registry
appreg_DATA = themus.applications

server_in_files = Themus_Properties_View.server.in.in
serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)

Themus_Properties_View.server.in: Themus_Properties_View.server.in.in
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" $< > $@

CLEANFILES = Themus_Properties_View.server Themus_Properties_View.server.in

install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule themus.schemas; \
	fi

@INTLTOOL_KEYS_RULE@
@INTLTOOL_SERVER_RULE@  
@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = \
  apply-font.glade \
  theme-method.directory.in \
  theme-method.conf \
  themus.schemas.in \
  Themus_Properties_View.server.in.in \
  themus.applications \
  themus.keys \
  ChangeLog

DISTCLEANFILES = \
  theme-method.directory \
  themus.schemas
