pixmapdir = $(datadir)/pixmaps
pixmap_DATA = \
	no-hack.png 

Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	screensaver-properties.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	screensaver-properties.desktop

SUBDIRS = screensavers

EXTRA_DIST = ChangeLog $(Applications_DATA) $(pixmap_DATA) \
	checked.xpm unchecked.xpm checked-disabled.xpm unchecked-disabled.xpm

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\" \
	-DSSPROP_DATADIR=\""${prefix}/share"\" \
	-DG_LOG_DOMAIN=\"screensaver-properties\" \
	@CAPPLET_CFLAGS@ \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/intl

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

screensaver_properties_capplet_LDADD = @CAPPLET_LIBS@ -lXt
