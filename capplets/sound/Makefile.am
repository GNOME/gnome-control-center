cappletname = sound
cappletgroup = 
bin_PROGRAMS = sound-properties-control
bin_SCRIPTS = sound-properties

sound_properties_control_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(top_builddir)/libsounds/libsounds.a

sound_properties_control_SOURCES =		\
	sound-properties-capplet.c

DESKTOP_EXEC_LINE = gnomecc --run-capplet sound-properties

pixmap_DATA =

##
## this is not generic
##

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = sound-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_sound_properties.oaf
oaffile = $(oaf_DATA)

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@

$(bin_SCRIPTS): $(top_srcdir)/capplets/common/wrapper-script.in
	sed -e "s#@BINDIR@#$(bindir)#" -e "s#@CAPPLET_NAME@#$(cappletname)-properties#" < $< > $@
	chmod a+x $@

$(oaffile): %.oaf: $(top_srcdir)/capplets/common/Bonobo_Control_Capplet_generic.oaf.in
	sed -e "s#@BINDIR@#$(bindir)#" -e "s#@SHORT_CAPPLET_NAME@#$(cappletname)#" -e "s#@CAPPLET_BINARY_NAME@#$(bin_PROGRAMS)#" < $< > ${oaffile}

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
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(defaults_DATA)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop) $(oaffile)
