INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../control-center		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DGNOMESYSCONFDIR=\""$(sysconfdir)"\"

bin_PROGRAMS = url-properties

url_properties_SOURCES = url-properties.c

url_properties_LDADD = ../../control-center/libcapplet.la \
		     $(GNOME_LIBDIR) $(ORB_LIBS) \
		     $(GNOMEUI_LIBS) $(INTLLIBS) -lgnorba


EXTRA_DIST = \
	url-properties.desktop

sysdir = $(datadir)/control-center/Documents
sys_DATA = url-properties.desktop

install-data-local:
	$(INSTALL_DATA) $(srcdir)/url-properties.desktop $(DESTDIR)$(datadir)/gnome/apps/Settings/Documents/url-properties.desktop
