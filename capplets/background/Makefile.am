cappletname = background
cappletgroup = 
bin_PROGRAMS = background-properties-capplet

background_properties_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)
background_properties_capplet_SOURCES = \
	main.c\
	applier.c			applier.h
#	prefs-widget.c		prefs-widget.h \
#preferences.c		preferences.h \

pixmap_DATA = 

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = background-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_background_properties.oaf

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(defaults_DATA) $(oaf_DATA)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)

