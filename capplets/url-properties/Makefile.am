INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../libcapplet		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(CAPPLET_CFLAGS)				\
	-DGNOMESYSCONFDIR=\""$(sysconfdir)"\"

bin_PROGRAMS = url-properties

url_properties_SOURCES = url-properties.c

url_properties_LDADD = $(CAPPLET_LIBS)

EXTRA_DIST = \
	url-properties.desktop

sysdir = $(datadir)/control-center/Documents/
sys_DATA = url-properties.desktop

psysdir = $(datadir)/gnome/apps/Settings/Documents/
psys_DATA = url-properties.desktop
