SUBDIRS =

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = sound-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_sound_properties.oaf

EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(oaf_DATA) $(defaults_DATA)

cappletname = sound
bin_PROGRAMS = sound-properties

sound_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
sound_properties_SOURCES = \
	sound-properties-capplet.c

pixmap_DATA =

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)
