pixmapdir = $(datadir)/pixmaps
pixmap_DATA = \
	no-hack.png 

Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	screensaver-properties.desktop

SUBDIRS = screensavers

EXTRA_DIST = ChangeLog TODO $(Applications_DATA) $(pixmap_DATA) \
	checked.xpm unchecked.xpm checked-disabled.xpm unchecked-disabled.xpm

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DSSPROP_DATADIR=\""${prefix}/share/screensaver-properties"\" \
	-DG_LOG_DOMAIN=\"screensaver-properties\" \
	$(GNOME_INCLUDEDIR) \
	$(LIBGLADE_CFLAGS) \
	$(XML_CFLAGS) \
	-I$(top_srcdir)/
	-I../intl
	-I$(top_srcdir)/intl
	-I$(top_srcdir)/control-center

bin_PROGRAMS = screensaver-properties-capplet

screensaver_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h\
	preferences.c preferences.h\
	screensaver-prefs-dialog.c screensaver-prefs-dialog.h\
	selection-dialog.c selection-dialog.h \
	pref-file.c pref-file.h \
	preview.c preview.h \
	expr.c expr.h \
	daemon.c daemon.h \
	rc-parse.c rc-parse.h \
	resources.c resources.h \
	XScreenSaver_ad.h \
	main.c

screensaver_properties_capplet_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOMEUI_LIBS) \
	$(ORBIT_LIBS) \
	$(LIBGLADE_LIBS) \
	$(GNOME_XML_LIB) \
	-lXt \
	-lgdk_pixbuf \
	../../control-center/libcapplet.la
