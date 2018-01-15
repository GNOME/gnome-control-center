dbus_shell_search_provider_built_sources =	\
	cc-shell-search-provider-generated.c	\
	cc-shell-search-provider-generated.h

# The upstream for the DBus interface definition is
# at http://git.gnome.org/browse/gnome-shell/plain/data/org.gnome.ShellSearchProvider2.xml
$(dbus_shell_search_provider_built_sources) : Makefile.am $(srcdir)/org.gnome.ShellSearchProvider2.xml
	$(AM_V_GEN)gdbus-codegen					\
		--interface-prefix org.gnome.				\
		--c-namespace Cc					\
		--generate-c-code cc-shell-search-provider-generated	\
		$(srcdir)/org.gnome.ShellSearchProvider2.xml		\
		$(NULL)

AM_CPPFLAGS =					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""\
	-I$(top_srcdir)				\
	$(SHELL_CFLAGS)				\
	-I$(top_srcdir)/panels/common/		\
	-I$(top_srcdir)/subprojects/libgd

libexec_PROGRAMS = gnome-control-center-search-provider

BUILT_SOURCES =	$(dbus_shell_search_provider_built_sources)

gnome_control_center_search_provider_SOURCES =	\
	$(BUILT_SOURCES)			\
	control-center-search-provider.c	\
	control-center-search-provider.h	\
	cc-search-provider.c			\
	cc-search-provider.h

gnome_control_center_search_provider_LDADD =	\
	$(top_builddir)/panels/common/liblanguage.la	\
	$(top_builddir)/shell/libshell.la		\
	$(top_builddir)/shell/libpanel_loader.la	\
	$(SHELL_LIBS)

CLEANFILES = $(BUILT_SOURCES) $(service_DATA)

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.gnome.ControlCenter.SearchProvider.service.in

org.gnome.ControlCenter.SearchProvider.service: org.gnome.ControlCenter.SearchProvider.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(service_in_files) org.gnome.ShellSearchProvider2.xml

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = gnome-control-center-search-provider.ini

-include $(top_srcdir)/git.mk
