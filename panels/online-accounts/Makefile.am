cappletname = online-accounts

SUBDIRS = icons

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(ONLINE_ACCOUNTS_PANEL_CFLAGS)			\
	-I$(top_srcdir)/libgd				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	$(NULL)

noinst_LTLIBRARIES = libonline-accounts.la

BUILT_SOURCES =				\
	cc-online-accounts-resources.c	\
	cc-online-accounts-resources.h

libonline_accounts_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	cc-online-accounts-panel.c	\
	cc-online-accounts-panel.h

libonline_accounts_la_LIBADD =				\
	$(PANEL_LIBS)					\
	$(ONLINE_ACCOUNTS_PANEL_LIBS)			\
	$(top_builddir)/libgd/libgd.la			\
	$(NULL)

libonline_accounts_la_LDFLAGS = $(PANEL_LDFLAGS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/online-accounts.gresource.xml)
cc-online-accounts-resources.c: online-accounts.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_online_accounts $<
cc-online-accounts-resources.h: online-accounts.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_online_accounts $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-online-accounts-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) online-accounts.gresource.xml

-include $(top_srcdir)/git.mk
