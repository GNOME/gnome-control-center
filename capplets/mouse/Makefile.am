NULL =

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = mouse

INCLUDES = 						\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-I$(top_srcdir)/libgnome-control-center-extension	\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	$(NULL)

bin_PROGRAMS = gnome-mouse-properties

ccmodulesdir = $(EXTENSIONSDIR)
ccmodules_LTLIBRARIES = libpointer.la

libpointer_la_SOURCES =			\
	pointer-module.c		\
	cc-pointer-panel.h		\
	cc-pointer-panel.c		\
	gnome-mouse-properties.c	\
	$(NULL)

libpointer_la_LDFLAGS =			\
	$(EXTENSION_LIBTOOL_FLAGS)	\
	$(NULL)

libpointer_la_LIBADD =			\
	$(EXTENSION_LIBS)		\
	$(EXTENSION_COMMON_LIBS)	\
	$(NULL)

libpointer_la_CFLAGS =			\
	$(EXTENSION_CFLAGS)		\
	$(EXTENSION_COMMON_CFLAGS)	\
	$(NULL)

libpointer_la_LIBTOOLFLAGS = --tag=disable-static

gnome_mouse_properties_LDADD = 		\
	$(top_builddir)/libgnome-control-center-extension/libcommon.la \
	$(GNOMECC_CAPPLETS_LIBS)	\
	$(NULL)

gnome_mouse_properties_SOURCES =	\
	gnome-mouse-properties.c	\
	gnome-mouse-accessibility.c	\
	gnome-mouse-accessibility.h

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(pkgdatadir)/pixmaps
pixmap_DATA = \
	double-click-on.png		\
	double-click-off.png		\
	double-click-maybe.png

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-mouse-properties.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-settings-mouse.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
