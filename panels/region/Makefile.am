# This is used in PANEL_CFLAGS
cappletname = region

INCLUDES =						\
	$(PANEL_CFLAGS)					\
	$(REGION_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-I$(srcdir)/../common/				\
	$(NULL)

noinst_LTLIBRARIES = libregion.la

BUILT_SOURCES =			\
	cc-region-resources.c	\
	cc-region-resources.h

libregion_la_SOURCES =	\
	$(BUILT_SOURCES) \
	cc-region-panel.c \
	cc-region-panel.h \
	gnome-region-panel-formats.c \
	gnome-region-panel-formats.h \
	gnome-region-panel-lang.c \
	gnome-region-panel-lang.h \
	gnome-region-panel-system.c \
	gnome-region-panel-system.h \
	gnome-region-panel-input.c \
	gnome-region-panel-input.h \
	$(NULL)

libregion_la_LIBADD = $(PANEL_LIBS) $(REGION_PANEL_LIBS) $(builddir)/../common/liblanguage.la

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
EXTRA_DIST = $(resource_files) region.gresource.xml

-include $(top_srcdir)/git.mk
