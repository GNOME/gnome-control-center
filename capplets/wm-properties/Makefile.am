INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../libcapplet		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR) $(IMLIB_CFLAGS)

bin_PROGRAMS = wm-properties-capplet


wm_properties_capplet_SOURCES = \
	wm-properties.h          \
	wm-properties-capplet.c	 \
	wm-list.c                \
	wm-exec.c

wm_properties_capplet_LDADD = \
../../libcapplet/libcapplet.la \
$(GNOME_LIBS) $(ORBIT_LIBS) $(GNOMEUI_LIBS) $(GNOME_LIBDIR) $(INTLLIBS) \
$(IMLIB_LIBS)

EXTRA_DIST = wm-properties.desktop 

SUBDIRS=wm-desktops

sysdir = $(datadir)/control-center/Desktop
sys_DATA = wm-properties.desktop

Setdir = $(datadir)/gnome/apps/Settings/Desktop
Set_DATA = wm-properties.desktop
