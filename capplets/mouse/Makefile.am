cappletname = mouse
cappletgroup =
bin_PROGRAMS = mouse-properties-capplet

mouse_properties_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)

bonobo_sources =				\
	mouse-properties-capplet.c

legacy_sources =				\
	main.c					\
	prefs-widget.c		prefs-widget.h	\
	preferences.c		preferences.h

if BONOBO_CONF_ENABLE
DESKTOP_EXEC_LINE = gnomecc --run-capplet mouse-properties-capplet
mouse_properties_capplet_SOURCES = $(bonobo_sources)
else
DESKTOP_EXEC_LINE = mouse-properties-capplet
mouse_properties_capplet_SOURCES = $(legacy_sources)
endif

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
####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@
$(desktop).in: %.desktop.in: %.desktop.in.in
	sed -e "s#@DESKTOP_EXEC_LINE@#$(DESKTOP_EXEC_LINE)#" -e "s#Icon=.*#Icon=$(GNOMECC_ICONS_DIR)/${cappletname}-capplet.png#" < $< > ${cappletname}.desktop.in

install-data-local:
	$(mkinstalldirs) $(datadir)/control-center/capplets/$(cappletgroup)
	$(INSTALL_DATA) $(desktop) $(datadir)/control-center/capplets/$(cappletgroup)$(desktop)

install-data-am: install-data-local
####@###GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST) $(oaf_DATA).in $(defaults_DATA) $(bonobo_sources) $(legacy_sources)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
oaffile    = $(oaf_DATA)
all-local: $(desktop)
