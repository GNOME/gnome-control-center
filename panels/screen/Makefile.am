cappletname = screen

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(SCREEN_PANEL_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	$(NULL)

noinst_LTLIBRARIES = libscreen.la

BUILT_SOURCES =			\
	cc-screen-resources.c	\
	cc-screen-resources.h

libscreen_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-screen-panel.c	\
	cc-screen-panel.h

libscreen_la_LIBADD = $(PANEL_LIBS) $(SCREEN_PANEL_LIBS)

resource_files = $(shell glib-compile-resources --generate-dependencies $(srcdir)/screen.gresource.xml)
cc-screen-resources.c: screen.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_screen $<
cc-screen-resources.h: screen.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_screen $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-screen-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) screen.gresource.xml

-include $(top_srcdir)/git.mk
