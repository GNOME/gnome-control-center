INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DGNOMESYSCONFDIR=\""$(sysconfdir)"\"		\
	@CAPPLET_CFLAGS@

bin_PROGRAMS = default-application-properties-capplet

default_application_properties_capplet_SOURCES = \
					 default-application-properties.c \
					 interface.c \
					 callbacks.c \
					 support.c
					 

default_application_properties_capplet_LDADD = @CAPPLET_LIBS@ 

EXTRA_DIST =					\
	default-application-properties.desktop.in       \
	default-application-properties.desktop    	\
	interface.h				\
	callbacks.h				\
	support.h

sysdir = $(datadir)/control-center/capplets/
sys_in_files = default-application-properties.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

install-data-local:
	$(INSTALL_DATA) $(srcdir)/default-application-properties.desktop $(DESTDIR)$(datadir)/gnome/apps/Settings/default-application-properties.desktop
