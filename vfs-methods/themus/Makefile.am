INCLUDES = $(THEME_METHOD_CFLAGS) \
  $(THEMUS_THUMBNAILER_CFLAGS) \
  $(THEMUS_COMPONENT_CFLAGS) \
  $(THEMUS_APPLIER_CFLAGS) \
  -I$(top_srcdir)/capplets/common \
  -DGNOMECC_GLADE_DIR=\"$(GNOMECC_GLADE_DIR)\" \
  -DDIRECTORY_DIR=\"$(vfsdirectorydir)\" \
  -DDATA_DIR=\"$(datadir)/themus\"

bin_PROGRAMS = gnome-theme-thumbnailer themus-theme-applier

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libtheme-method.la

nautilusdir = $(libdir)/nautilus/extensions-1.0
nautilus_LTLIBRARIES = libnautilus-themus.la

libtheme_method_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'vfs_module_.*'
libtheme_method_la_LIBADD = $(THEME_METHOD_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libtheme_method_la_SOURCES = theme-method.c

gnome_theme_thumbnailer_LDADD = $(THEMUS_THUMBNAILER_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
gnome_theme_thumbnailer_SOURCES = \
	theme-thumbnailer.c

themus_theme_applier_LDADD = $(THEMUS_APPLIER_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
themus_theme_applier_SOURCES = \
	themus-theme-applier.c

libnautilus_themus_la_LIBADD = $(THEMUS_COMPONENT_LIBS) \
  $(top_builddir)/capplets/common/libcommon.la
libnautilus_themus_la_LDFLAGS = -module -avoid-version \
  -export-symbols-regex 'nautilus_module_.*'
libnautilus_themus_la_SOURCES = \
	themus-properties-view.c \
	themus-properties-view.h \
	themus-properties-main.c

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = apply-font.glade

@INTLTOOL_DIRECTORY_RULE@

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = theme-method.conf

vfsdirectorydir = $(datadir)/gnome/vfolders
vfsdirectory_DATA = theme-method.directory

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = themus.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = themus.keys

appregdir = $(datadir)/application-registry
appreg_DATA = themus.applications

CLEANFILES = Themus_Properties_View.server Themus_Properties_View.server.in

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/themus.schemas; \
	fi
else
install-data-local:
endif

@INTLTOOL_KEYS_RULE@
@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = \
  $(Glade_DATA) \
  apply-font.glade \
  theme-method.directory.in \
  theme-method.conf \
  $(schemas_DATA)	\
  $(schemas_in_files)	\
  themus.applications \
  themus.keys \
  ChangeLog

DISTCLEANFILES = \
  theme-method.directory \
  themus.schemas
