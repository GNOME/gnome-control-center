# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = default-applications

bin_PROGRAMS = gnome-default-applications-properties

bin_SCRIPTS = gnome-at-visual gnome-at-mobility

gnome_default_applications_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_default_applications_properties_SOURCES = \
	gnome-da-capplet.c gnome-da-capplet.h \
	gnome-da-xml.c gnome-da-xml.h \
	gnome-da-item.c gnome-da-item.h

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-default-applications-properties.glade

gnome-at-visual: gnome-at-commandline.in
	cp $< $@

gnome-at-mobility: gnome-at-commandline.in
	cp $< $@

desktopdir = $(datadir)/applications
Desktop_in_files = default-applications.desktop.in \
	default-applications-accessibility.desktop.in \
	gnome-at-visual.desktop.in gnome-at-mobility.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

autostartdir = $(datadir)/gnome/autostart
autostart_in_files = gnome-at-session.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

xmldata_in_files = gnome-default-applications.xml.in
xmldatadir = $(pkgdatadir)
xmldata_DATA = $(xmldata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

INCLUDES   = 					\
	$(GNOMECC_CAPPLETS_CFLAGS)		\
	$(DEFAULT_APPLICATIONS_CAPPLET_CFLAGS)	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"\
	-DGNOMECC_GLADE_DIR=\""$(gladedir)"\"	\
	-DGNOMECC_DATA_DIR=\""$(pkgdatadir)"\"

noinst_DATA = gnome-default-applications.xml.in
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA) $(xmldata_DATA) $(autostart_DATA)
EXTRA_DIST = $(glade_DATA) gnome-default-applications.xml.in

