cappletname = power

SUBDIRS = icons

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(POWER_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-I$(srcdir)/../../shell/			\
	$(NULL)

noinst_LTLIBRARIES = libpower.la

BUILT_SOURCES =			\
	cc-power-resources.c	\
	cc-power-resources.h

libpower_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-power-panel.c	\
	cc-power-panel.h

libpower_la_LIBADD = $(PANEL_LIBS) $(POWER_PANEL_LIBS)

if BUILD_BLUETOOTH
AM_CPPFLAGS += $(BLUETOOTH_CFLAGS)
libpower_la_LIBADD += $(BLUETOOTH_LIBS)
endif

if BUILD_NETWORK
AM_CPPFLAGS += $(NETWORK_MANAGER_CFLAGS)
libpower_la_LIBADD += $(NETWORK_MANAGER_LIBS)
endif

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/power.gresource.xml)
cc-power-resources.c: power.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_power $<
cc-power-resources.h: power.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_power $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) power.gresource.xml meson.build

-include $(top_srcdir)/git.mk
