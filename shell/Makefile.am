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
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

sysdir = $(datadir)/applications
sys_in_files = gnomecc.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
schemas_DATA = control-center.schemas
@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi

EXTRA_DIST = gnomecc.desktop.in.in $(schemas_DATA).in

DISTCLEANFILES = gnomecc.desktop gnomecc.desktop.in $(schemas_DATA)
