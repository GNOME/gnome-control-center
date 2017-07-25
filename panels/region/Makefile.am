# This is used in PANEL_CFLAGS
cappletname = region

AM_CPPFLAGS =						\
	$(PANEL_CFLAGS)					\
	$(REGION_PANEL_CFLAGS)				\
	-I$(srcdir)/../common/				\
	-I$(top_srcdir)/subprojects/libgd/		\
	$(NULL)

noinst_LTLIBRARIES = libregion.la

BUILT_SOURCES =			\
	cc-region-resources.c	\
	cc-region-resources.h

libregion_la_SOURCES =	\
	$(BUILT_SOURCES) \
	cc-region-panel.c \
	cc-region-panel.h \
	cc-format-chooser.c \
	cc-format-chooser.h \
	cc-input-options.c \
	cc-input-options.h \
	cc-input-chooser.c \
	cc-input-chooser.h \
	cc-ibus-utils.c	\
	cc-ibus-utils.h \
	$(NULL)

libregion_la_LIBADD = \
	$(PANEL_LIBS) \
	$(REGION_PANEL_LIBS) \
	$(top_builddir)/subprojects/libgd/libgd.la \
	$(builddir)/../common/liblanguage.la

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/region.gresource.xml)
cc-region-resources.c: region.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_region $<
cc-region-resources.h: region.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_region $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-region-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) region.gresource.xml meson.build

-include $(top_srcdir)/git.mk
