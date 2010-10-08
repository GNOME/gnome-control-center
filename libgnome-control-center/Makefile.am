NULL =

uidir = $(pkgdatadir)/ui

INCLUDES =					\
	$(CAPPLET_CFLAGS)			\
	$(NULL)

lib_LTLIBRARIES = libgnome-control-center.la

libgnome_control_center_include_HEADERS =	\
	cc-panel.h				\
	cc-setting-editor.h			\
	cc-shell.h				\
	gconf-property-editor.h			\
	gconf-property-editor-marshal.h 	\
	$(NULL)

libgnome_control_center_la_SOURCES =		\
	cc-marshal.c				\
	cc-marshal.h				\
	cc-panel.c				\
	cc-panel.h				\
	cc-setting-editor.c			\
	cc-setting-editor.h			\
	cc-shell.c				\
	cc-shell.h				\
	gconf-property-editor.c			\
	gconf-property-editor.h			\
	$(NULL)

libgnome_control_center_la_LDFLAGS =		\
	no-undefined				\
	-version-info $(LIBGNOMECONTROLCENTER_CURRENT):$(LIBGNOMECONTROLCENTER_REVISION):$(LIBGNOMECONTROLCENTER_AGE) \
	$(NULL)

libgnome_control_center_la_LIBADD = 		\
	$(CAPPLET_LIBS)				\
	$(NULL)

libgnome_control_center_la_LIBTOOLFLAGS = --tag=disable-static

libgnome_control_center_includedir = $(includedir)/gnome-control-center-1/libgnome-control-center

noinst_PROGRAMS = test-setting-editor

test_setting_editor_SOURCES = test-setting-editor.c
test_setting_editor_LDADD =							\
	$(CAPPLET_LIBS)								\
	$(top_builddir)/libgnome-control-center/libgnome-control-center.la	\
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libgnome-control-center.pc

EXTRA_DIST =					\
        libgnome-control-center.pc.in	\
        $(NULL)

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""		\
	-DUIDIR="\"$(uidir)\""					\
	-DMENUDIR="\"$(menudir)\""

-include $(top_srcdir)/git.mk
