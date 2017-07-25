cappletname = notifications

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(NOTIFICATIONS_PANEL_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	$(NULL)

noinst_LTLIBRARIES = libnotifications.la

BUILT_SOURCES =				\
	cc-notifications-resources.c	\
	cc-notifications-resources.h

libnotifications_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	cc-edit-dialog.c		\
	cc-edit-dialog.h		\
	cc-notifications-panel.c	\
	cc-notifications-panel.h

libnotifications_la_LIBADD = $(NOTIFICATIONS_PANEL_LIBS) $(PANEL_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/notifications.gresource.xml)
cc-notifications-resources.c: notifications.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_notifications $<
cc-notifications-resources.h: notifications.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_notifications $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-notifications-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) notifications.gresource.xml meson.build

-include $(top_srcdir)/git.mk
