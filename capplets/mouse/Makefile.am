cappletname = mouse
bin_PROGRAMS = mouse-properties

mouse_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
mouse_properties_SOURCES = \
	main.c\
	prefs-widget.c		prefs-widget.h \
	preferences.c		preferences.h

pixmap_DATA = mouse-left.png mouse-right.png

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) mouse-left.png mouse-right.png
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)

