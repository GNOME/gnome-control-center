# This is used in PANEL_CFLAGS
cappletname = wacom

SUBDIRS = calibrator

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(WACOM_PANEL_CFLAGS)				\
	-I$(srcdir)/calibrator				\
	-I$(top_srcdir)/panels/common			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	$(NULL)

noinst_LTLIBRARIES = libwacom-properties.la

BUILT_SOURCES =			\
	cc-wacom-resources.c	\
	cc-wacom-resources.h

libwacom_properties_la_SOURCES =	\
	$(BUILT_SOURCES)		\
	cc-drawing-area.c		\
	cc-drawing-area.h		\
	cc-tablet-tool-map.c		\
	cc-tablet-tool-map.h		\
	cc-wacom-device.c		\
	cc-wacom-device.h		\
	cc-wacom-tool.c			\
	cc-wacom-tool.h			\
	cc-wacom-panel.c		\
	cc-wacom-panel.h		\
	gsd-wacom-key-shortcut-button.c \
	gsd-wacom-key-shortcut-button.h \
	cc-wacom-button-row.c		\
	cc-wacom-button-row.h		\
	cc-wacom-page.c			\
	cc-wacom-page.h			\
	cc-wacom-stylus-page.c		\
	cc-wacom-stylus-page.h		\
	cc-wacom-nav-button.c		\
	cc-wacom-nav-button.h		\
	cc-wacom-mapping-panel.c	\
	cc-wacom-mapping-panel.h	\
	gsd-enums.h

libwacom_properties_la_LIBADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS) $(builddir)/calibrator/libwacom-calibrator.la $(top_builddir)/panels/common/libdevice.la

noinst_PROGRAMS = test-wacom

test_wacom_SOURCES =			\
	$(BUILT_SOURCES)		\
	test-wacom.c			\
	cc-tablet-tool-map.c		\
	cc-tablet-tool-map.h		\
	cc-wacom-device.c		\
	cc-wacom-device.h		\
	cc-wacom-tool.c			\
	cc-wacom-tool.h			\
	cc-wacom-button-row.c		\
	cc-wacom-button-row.h		\
	gsd-wacom-key-shortcut-button.c \
	gsd-wacom-key-shortcut-button.h \
	cc-wacom-page.c			\
	cc-wacom-page.h			\
	cc-wacom-stylus-page.c		\
	cc-wacom-stylus-page.h		\
	cc-wacom-nav-button.c		\
	cc-wacom-nav-button.h		\
	cc-wacom-mapping-panel.c	\
	cc-wacom-mapping-panel.h	\
	gsd-enums.h

test_wacom_CPPFLAGS = $(AM_CPPFLAGS) -DFAKE_AREA
test_wacom_LDADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS) $(builddir)/calibrator/libwacom-calibrator-test.la $(top_builddir)/panels/common/libdevice.la

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/wacom.gresource.xml)
cc-wacom-resources.c: wacom.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_wacom $<
cc-wacom-resources.h: wacom.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_wacom $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-wacom-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

WACOMDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/wacom/
WACOMFILES=gsd-wacom-device.c gsd-wacom-device.h gsd-wacom-key-shortcut-button.c gsd-wacom-key-shortcut-button.h
update-from-gsd:
	FILES="$(WACOMFILES)" DIR="$(WACOMDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	git commit -m "wacom: Update from gnome-settings-daemon" $(WACOMFILES)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) wacom.gresource.xml

-include $(top_srcdir)/git.mk
