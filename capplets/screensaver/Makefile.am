SUBDIRS = screensavers

cappletname = screensaver
cappletgroup =
bin_PROGRAMS = screensaver-properties-capplet

screensaver_properties_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS) -lXt
screensaver_properties_capplet_SOURCES = \
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

pixmaps_DATA = no-hack.png blank-screen.png
#screen.png 

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@

####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@
$(desktop).in: %.desktop.in: %.desktop.in.in
	sed -e "s#@DESKTOP_EXEC_LINE@#$(DESKTOP_EXEC_LINE)#" -e "s#Icon=.*#Icon=$(GNOMECC_ICONS_DIR)/${cappletname}-capplet.png#" < $< > ${cappletname}.desktop.in

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/control-center/capplets/$(cappletgroup)
	$(INSTALL_DATA) $(desktop) $(DESTDIR)$(datadir)/control-center/capplets/$(cappletgroup)$(desktop)

install-data-am: install-data-local
####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(pixmaps_DATA) 
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapsdir = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)

