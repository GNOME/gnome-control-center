SUBDIRS = icons

cappletname = color

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(COLOR_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DBINDIR="\"$(bindir)\""			\
	$(NULL)

noinst_LTLIBRARIES = libcolor.la

BUILT_SOURCES =			\
	cc-color-resources.h	\
	cc-color-resources.c

libcolor_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-color-calibrate.c		\
	cc-color-calibrate.h		\
	cc-color-cell-renderer-text.c	\
	cc-color-cell-renderer-text.h	\
	cc-color-profile.c	\
	cc-color-profile.h	\
	cc-color-device.c	\
	cc-color-device.h	\
	cc-color-common.c	\
	cc-color-common.h	\
	cc-color-panel.c	\
	cc-color-panel.h

libcolor_la_LIBADD = $(PANEL_LIBS) $(COLOR_PANEL_LIBS) $(LIBM)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/color.gresource.xml)
cc-color-resources.c: color.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_color $<
cc-color-resources.h: color.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_color $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-color-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) color.gresource.xml meson.build

-include $(top_srcdir)/git.mk
