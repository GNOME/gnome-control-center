cappletname = background
cappletgroup = 
bin_PROGRAMS = background-properties-control
bin_SCRIPTS = background-properties-capplet

background_properties_control_LDADD = $(GNOMECC_CAPPLETS_LIBS)

background_properties_control_SOURCES =		\
	background-properties-capplet.c		\
	preferences.c		preferences.h   \
	applier.c		applier.h

DESKTOP_EXEC_LINE = gnomecc --run-capplet background-properties
BONOBO_CONF_DEFINE=-DBONOBO_CONF_ENABLE

pixmap_DATA = 

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = background-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_background_properties.oaf
oaffile = $(oaf_DATA)

$(oaffile): %.oaf: %.oaf.in
	sed -e "s#REPLACEME_PLIZ#$(bindir)/$(bin_PROGRAMS)#" < $< > ${oaffile}

##
## You should not need to modify anything below this line
##
@XML_I18N_MERGE_DESKTOP_RULE@

$(bin_SCRIPTS): $(top_srcdir)/capplets/common/wrapper-script.in
	sed -e "s#@BINDIR@#$(bindir)#" -e "s#@CAPPLET_NAME@#$(cappletname)-properties#" < $< > $@
	chmod a+x $@

####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@
$(desktop).in: %.desktop.in: %.desktop.in.in
	sed -e "s#@DESKTOP_EXEC_LINE@#$(DESKTOP_EXEC_LINE)#" -e "s#Icon=.*#Icon=$(GNOMECC_ICONS_DIR)/${cappletname}-capplet.png#" < $< > ${cappletname}.desktop.in

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/control-center/capplets/$(cappletgroup)
	$(INSTALL_DATA) $(desktop) $(DESTDIR)$(datadir)/control-center/capplets/$(cappletgroup)$(desktop)

install-data-am: install-data-local
####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(BONOBO_CONF_DEFINE)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(defaults_DATA) $(oaf_DATA).in
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop) $(oaffile)
