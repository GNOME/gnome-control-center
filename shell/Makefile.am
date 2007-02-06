INCLUDES =					\
	-I$(top_srcdir)/libslab			\
	$(GNOMECC_CFLAGS)

bin_PROGRAMS = gnome-control-center 

gnome_control_center_SOURCES =		\
	control-center.c

gnome_control_center_LDADD =						\
	$(GNOMECC_LIBS)							\
	$(top_builddir)/libslab/libslab.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""

sysdir = $(datadir)/applications
sys_in_files = gnomecc.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

directorydir = $(datadir)/desktop-directories
directory_in_files = gnomecc.directory.in
directory_DATA = $(directory_in_files:.directory.in=.directory)
@INTLTOOL_DIRECTORY_RULE@

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
schemas_DATA = control-center.schemas
@INTLTOOL_SCHEMAS_RULE@

install-data-local: $(schema_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
		for p in $^ ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p 2>&1 > /dev/null; \
		done \
	fi
endif

menudir = $(sysconfdir)/xdg/menus
menu_DATA = gnomecc.menu

EXTRA_DIST = gnomecc.desktop.in.in gnomecc.directory.in gnomecc.menu $(schemas_DATA).in

DISTCLEANFILES = gnomecc.desktop gnomecc.desktop.in gnomecc.directory $(schemas_DATA)
