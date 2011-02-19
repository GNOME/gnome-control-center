NULL =

uidir = $(pkgdatadir)/ui

AM_CPPFLAGS =					\
	$(LIBGNOME_CONTROL_CENTER_CFLAGS)	\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""\
	-DUIDIR="\"$(uidir)\""			\
	-DMENUDIR="\"$(menudir)\""		\
	$(NULL)

lib_LTLIBRARIES = libgnome-control-center.la

libgnome_control_center_include_HEADERS =	\
	cc-panel.h				\
	cc-shell.h				\
	gconf-property-editor.h			\
	$(NULL)

libgnome_control_center_la_SOURCES =		\
	cc-marshal.c				\
	cc-marshal.h				\
	cc-panel.c				\
	cc-panel.h				\
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
	$(LIBGNOME_CONTROL_CENTER_LIBS)		\
	$(NULL)

libgnome_control_center_la_LIBTOOLFLAGS = --tag=disable-static

libgnome_control_center_includedir = $(includedir)/gnome-control-center-1/libgnome-control-center

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libgnome-control-center.pc

EXTRA_DIST =					\
        libgnome-control-center.pc.in	\
        $(NULL)

-include $(top_srcdir)/git.mk
