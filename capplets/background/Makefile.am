cappletname = background
cappletgroup = 
bin_PROGRAMS = background-properties-control
bin_SCRIPTS = background-properties-capplet

background_properties_control_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(top_builddir)/libbackground/libbackground.a

background_properties_control_SOURCES =		\
	background-properties-capplet.c

DESKTOP_EXEC_LINE = gnomecc --run-capplet background-properties
BONOBO_CONF_DEFINE=-DBONOBO_CONF_ENABLE

pixmap_DATA = 

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = background-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_background_properties.oaf

##
## You should not need to modify anything below this line
##
@INTLTOOL_DESKTOP_RULE@

$(bin_SCRIPTS): $(top_srcdir)/capplets/common/wrapper-script.in
	sed -e "s#@BINDIR@#$(bindir)#" -e "s#@CAPPLET_NAME@#$(cappletname)-properties#" < $< > $@
	chmod a+x $@

$(oaf_DATA): %.oaf: $(top_srcdir)/capplets/common/Bonobo_Control_Capplet_generic.oaf.in
	sed -e "s#@BINDIR@#$(bindir)#" -e "s#@SHORT_CAPPLET_NAME@#$(cappletname)#" -e "s#@CAPPLET_BINARY_NAME@#$(bin_PROGRAMS)#" < $< > $@

####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@
$(desktop).in: %.desktop.in: %.desktop.in.in
	sed -e "s#@DESKTOP_EXEC_LINE@#$(DESKTOP_EXEC_LINE)#" -e "s#Icon=.*#Icon=$(GNOMECC_ICONS_DIR)/${cappletname}-capplet.png#" < $< > ${cappletname}.desktop.in

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/control-center/capplets/$(cappletgroup)
	$(INSTALL_DATA) $(desktop) $(DESTDIR)$(datadir)/control-center/capplets/$(cappletgroup)$(desktop)
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) gconftool --makefile-install-rule $(schema_DATA)

install-data-am: install-data-local
####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES     = $(GNOMECC_CAPPLETS_CFLAGS) $(BONOBO_CONF_DEFINE) -I$(top_srcdir)/libbackground
CLEANFILES   = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST   = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(defaults_DATA) $(oaf_DATA).in
iconsdir     = $(GNOMECC_ICONS_DIR)
Gladedir     = $(GNOMECC_GLADE_DIR)
pixmapdir    = $(GNOMECC_PIXMAPS_DIR)
schemadir    = $(sysconfdir)/control-center/schemas
Glade_DATA   = $(cappletname)-properties.glade
icons_DATA   = $(cappletname)-capplet.png
schema_DATA  = background.schema
desktop      = $(cappletname).desktop
all-local: $(desktop) $(oaffile)


