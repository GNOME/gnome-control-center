cappletname = mime-type
cappletgroup = "Advanced/"
bin_PROGRAMS = mime-type-capplet

mime_type_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)
mime_type_capplet_SOURCES = \
	mime-type-capplet.c\
	mime-data.h		mime-data.c\
	edit-window.c		edit-window.h\
	mime-info.c		mime-info.h\
	new-mime-window.h	new-mime-window.c

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
