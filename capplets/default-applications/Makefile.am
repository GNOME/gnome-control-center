SUBDIRS = 

cappletname = default-applications
bin_PROGRAMS = default-applications-properties

default_applications_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
default_applications_properties_SOURCES = \
	default-application-properties.c \
	interface.c		interface.h \
	callbacks.c		callbacks.h \
	support.c			support.h	\
	defaults.h

pixmap_DATA =

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) -DGNOMESYSCONFDIR=\""$(sysconfdir)"\"
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)

