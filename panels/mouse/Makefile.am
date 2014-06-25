# This is used in PANEL_CFLAGS
cappletname = mouse

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(MOUSE_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	$(NULL)

noinst_LTLIBRARIES = libmouse-properties.la

BUILT_SOURCES =			\
	cc-mouse-resources.c	\
	cc-mouse-resources.h

libmouse_properties_la_SOURCES =	\
	$(BUILT_SOURCES)		\
	cc-mouse-panel.c		\
	cc-mouse-panel.h		\
	gnome-mouse-properties.c	\
	gnome-mouse-properties.h	\
	gsd-input-helper.c		\
	gsd-input-helper.h		\
	gnome-mouse-test.c		\
	gnome-mouse-test.h

libmouse_properties_la_LIBADD = $(PANEL_LIBS) $(MOUSE_PANEL_LIBS)

test_gnome_mouse_test_SOURCES =		\
	$(BUILT_SOURCES)		\
	gnome-mouse-test.c		\
	gnome-mouse-test.h		\
	test-gnome-mouse-test.c

noinst_PROGRAMS = test-gnome-mouse-test

test_gnome_mouse_test_CPPFLAGS = $(AM_CPPFLAGS)
test_gnome_mouse_test_LDADD = $(PANEL_LIBS) $(MOUSE_PANEL_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/mouse.gresource.xml)
cc-mouse-resources.c: mouse.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_mouse $<
cc-mouse-resources.h: mouse.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_mouse $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-mouse-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INPUTDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/common/
INPUTFILES=gsd-input-helper.h gsd-input-helper.c
update-from-gsd:
	FILES="$(INPUTFILES)" DIR="$(INPUTDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	git commit -m "mouse: Update from gnome-settings-daemon" $(INPUTFILES)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(resource_files) mouse.gresource.xml

-include $(top_srcdir)/git.mk
