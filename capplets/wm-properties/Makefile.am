SUBDIRS = wm-desktops

cappletname = wm
cappletgroup = "Advanced/"
bin_PROGRAMS = wm-properties-capplet

wm_properties_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)
wm_properties_capplet_SOURCES = \
	wm-properties.h         \
	wm-properties-capplet.c \
	wm-list.c               \
	wm-exec.c

pixmap_DATA = 

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)
