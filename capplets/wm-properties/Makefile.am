INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../control-center		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR) $(IMLIB_CFLAGS)

bin_PROGRAMS = wm-properties-capplet


wm_properties_capplet_SOURCES = \
	wm-properties.h          \
	wm-properties-capplet.c	 \
	wm-list.c                \
	wm-exec.c

wm_properties_capplet_LDADD = \
../../control-center/libcapplet.la \
$(GNOME_LIBDIR) $(ORB_LIBS) $(GNOMEUI_LIBS) $(INTLLIBS) \
-lgnorba $(IMLIB_LIBS)

EXTRA_DIST = wm-properties.desktop.in wm-properties.desktop

SUBDIRS=wm-desktops

sysdir = $(datadir)/control-center/Desktop
sys_in_files = wm-properties.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)

Setdir = $(datadir)/gnome/apps/Settings/Desktop

Set_DATA = $(sys_DATA)

@XML_I18N_MERGE_DESKTOP_RULE@
