INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	@CAPPLET_CFLAGS@

bin_PROGRAMS = wm-properties-capplet


wm_properties_capplet_SOURCES = \
	wm-properties.h          \
	wm-properties-capplet.c	 \
	wm-list.c                \
	wm-exec.c

wm_properties_capplet_LDADD = @CAPPLET_LIBS@

EXTRA_DIST = wm-properties.desktop 

SUBDIRS=wm-desktops

sysdir = $(datadir)/control-center/capplets/Advanced/
sys_DATA = wm-properties.desktop

psysdir = $(datadir)/gnome/apps/Settings/Advanced
psys_DATA = wm-properties.desktop
