cappletname = mouse
cappletgroup =
bin_PROGRAMS = mouse-properties-control
bin_SCRIPTS = mouse-properties-capplet

mouse_properties_control_LDADD = $(GNOMECC_CAPPLETS_LIBS)

mouse_properties_control_SOURCES =		\
	mouse-properties-capplet.c

DESKTOP_EXEC_LINE = gnomecc --run-capplet mouse-properties

pixmap_DATA = mouse-left.png mouse-right.png

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = mouse-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_mouse_properties.oaf

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

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(oaf_DATA).in $(defaults_DATA) $(pixmap_DATA)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
oaffile    = $(oaf_DATA)
all-local: $(desktop)
