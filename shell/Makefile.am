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

EXTRA_DIST = gnomecc.desktop.in

DISTCLEANFILES = gnomecc.desktop
