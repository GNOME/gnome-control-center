INCLUDES = $(THEME_METHOD_CFLAGS) \
  $(THEMUS_THUMBNAILER_CFLAGS) \
  $(THEMUS_COMPONENT_CFLAGS) \
  -I$(top_srcdir)/capplets/common \
  -DDIRECTORY_DIR=\"$(vfsdirectorydir)\"

AM_CFLAGS = -g

bin_PROGRAMS = gnome-theme-thumbnailer

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libtheme-method.la

bonobodir = $(libdir)/bonobo
bonobo_LTLIBRARIES = libthemus-theme-properties-view.la libthemus-component.la

libtheme_method_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'vfs_module_.*'
libtheme_method_la_LIBADD = $(THEME_METHOD_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libtheme_method_la_SOURCES = theme-method.c

gnome_theme_thumbnailer_LDADD = $(THEMUS_THUMBNAILER_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
gnome_theme_thumbnailer_SOURCES = \
	theme-thumbnailer.c

libthemus_component_la_LIBADD =  $(THEMUS_COMPONENT_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libthemus_component_la_LDFLAGS = -module -avoid-version
libthemus_component_la_SOURCES = \
	themus-component.c \
	themus-component.h \
	themus-component-main.c

libthemus_theme_properties_view_la_LIBADD = $(THEMUS_COMPONENT_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libthemus_theme_properties_view_la_LDFLAGS = -module -avoid-version
libthemus_theme_properties_view_la_SOURCES = \
	themus-properties-view.c \
	themus-properties-view.h \
	themus-properties-main.c

@INTLTOOL_DIRECTORY_RULE@

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = theme-method.conf

vfsdirectorydir = $(datadir)/gnome/vfolders
vfsdirectory_DATA = theme-method.directory

schemasdir       = $(sysconfdir)/gconf/schemas
schemas_in_files = themus.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

server_in_files = Themus_Component.server.in.in Themus_Properties_View.server.in.in
serverdir = $(libdir)/bonobo/servers
server_DATA = $(server_in_files:.server.in.in=.server)

Themus_Component.server.in: Themus_Component.server.in.in
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" Themus_Component.server.in.in > Themus_Component.server.in

Themus_Properties_View.server.in: Themus_Properties_View.server.in.in
	sed -e "s|\@BONOBODIR\@|$(bonobodir)|" Themus_Properties_View.server.in.in > Themus_Properties_View.server.in

CLEANFILES = Themus_Component.server Themus_Component.server.in Themus_Properties_View.server Themus_Properties_View.server.in

install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule themus.schemas; \
	fi
	
@INTLTOOL_KEYS_RULE@
@INTLTOOL_SERVER_RULE@  
@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = \
  theme-method.directory.in \
  theme-method.conf \
  themus.schemas.in \
  Themus_Component.server.in.in \
  Themus_Properties_View.server.in.in \
  ChangeLog

DISTCLEANFILES = \
  theme-method.directory \
  themus.schemas
