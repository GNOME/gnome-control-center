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

desktopdir = $(datadir)/applications
desktop_in_files = themus-theme-applier.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

install-data-local: install-gconf-schemas install-desktop-database

if GCONF_SCHEMAS_INSTALL
install-gconf-schemas:
	if test -z "$(DESTDIR)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule themus.schemas; \
	fi
else
install-gconf-schemas:
endif

install-desktop-database: install-desktopDATA
	update-desktop-database $(DESTDIR)$(desktopdir)

distclean-local:
	rm -f $(DESTDIR)$(desktopdir)/mimeinfo.cache

@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

CLEANFILES = $(desktop_in_files) $(desktop_DATA) \
  $(schemas_DATA) $(vfsdirectory_DATA)

EXTRA_DIST = \
  $(Glade_DATA) \
  apply-font.glade \
  theme-method.directory.in \
  theme-method.conf \
  $(schemas_in_files)	\
  themus-theme-applier.desktop.in.in
