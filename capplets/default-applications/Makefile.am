cappletname = default-applications
cappletgroup =
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
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)

