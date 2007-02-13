# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = default-applications

bin_PROGRAMS = gnome-default-applications-properties

gnome_default_applications_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_default_applications_properties_SOURCES = \
	gnome-da-capplet.c gnome-da-capplet.h \
	gnome-da-xml.c gnome-da-xml.h \
	gnome-da-item.c gnome-da-item.h

@INTLTOOL_DESKTOP_RULE@

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-default-applications-properties.glade

desktopdir = $(datadir)/applications
Desktop_in_files = default-applications.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

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
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA) $(xmldata_DATA)
EXTRA_DIST = $(glade_DATA) gnome-default-applications.xml.in
