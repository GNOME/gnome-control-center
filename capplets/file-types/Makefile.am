SUBDIRS = libuuid

cappletname = file-types
cappletgroup = "Advanced/"
bin_PROGRAMS = file-types-capplet

file_types_capplet_LDADD = $(VFS_CAPPLET_LIBS) libuuid/libuuid.a

file_types_capplet_SOURCES =		\
	file-types-capplet.h 		\
	file-types-capplet-dialogs.h 	\
	file-types-icon-entry.h		\
	file-types-capplet.c		\
	file-types-capplet-dialogs.c 	\
	file-types-icon-entry.c

pixmap_DATA = 

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(VFS_CAPPLET_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)






