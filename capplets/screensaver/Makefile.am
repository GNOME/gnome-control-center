pixmapdir = $(datadir)/pixmaps
pixmap_DATA = \
	no-hack.png 

Applicationsdir = $(datadir)/control-center/capplets
Applications_in_files = screensaver-properties.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = $(Applications_DATA)

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	screensaver-properties.glade

SUBDIRS = screensavers

EXTRA_DIST = ChangeLog $(Applications_in_files) $(pixmap_DATA) $(Glade_DATA)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\" \
	-DSSPROP_DATADIR=\""${prefix}/share"\" \
	-DG_LOG_DOMAIN=\"screensaver-properties\" \
	-DGLADE_DATADIR=\""$(Gladedir)"\"	\
	@SCREENSAVER_CAPPLET_CFLAGS@ \
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

screensaver_properties_capplet_LDADD = @SCREENSAVER_CAPPLET_LIBS@ -lXt
