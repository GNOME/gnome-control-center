cappletname = keyboard
cappletgroup =
bin_PROGRAMS = keyboard-properties-control
bin_SCRIPTS = keyboard-properties

keyboard_properties_control_LDADD = $(GNOMECC_CAPPLETS_LIBS)

keyboard_properties_control_SOURCES =		\
	keyboard-properties.c

DESKTOP_EXEC_LINE = gnomecc --run-capplet keyboard-properties

## this is not generic

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = keyboard-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = Bonobo_Control_Capplet_keyboard_properties.oaf

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

install-data-am: install-data-local
####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

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

