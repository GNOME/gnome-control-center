SUBDIRS = screensavers

cappletname = screensaver
bin_PROGRAMS = screensaver-properties

screensaver_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) -lXt
screensaver_properties_SOURCES = \
	main.c \
	prefs-widget.c				prefs-widget.h\
	preferences.c				preferences.h\
	screensaver-prefs-dialog.c	screensaver-prefs-dialog.h\
	selection-dialog.c			selection-dialog.h \
	pref-file.c				pref-file.h\
	preview.c					preview.h\
	expr.c					expr.h\
	daemon.c					daemon.h\
	rc-parse.c				rc-parse.h\
	resources.c				resources.h\
	XScreenSaver_ad.h

pixmap_DATA = no-hack.png 

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) -DSSPROP_DATADIR=\""${prefix}/share"\"
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)

