cappletname = keyboard
cappletgroup =
bin_PROGRAMS = keyboard-properties

keyboard_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(XF86MISC_LIBS)

keyboard_properties_SOURCES = \
	main.c \
	bonobo-property-editor-range.c \
	bonobo-property-editor-range.h 

## this is not generic

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = keyboard-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_keyboard_properties.oaf

###$(oaffile): %.oaf: %.oaf.in
###	sed -e "s#REPLACEME_PLIZ#$(bindir)/$(bin_PROGRAMS)#" < $< > ${oaffile}

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@
@GNOMECC_CAPPLETS_OAF_FILE_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(defaults_DATA) $(oaf_DATA).in
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
oaffile    = $(oaf_DATA)
all-local: $(desktop) $(oaffile)

