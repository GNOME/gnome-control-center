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
xmldatadir = $(pkgdatadir)/gnome-default-applications
xmldata_DATA = $(xmldata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

INCLUDES   = 					\
	$(GNOMECC_CAPPLETS_CFLAGS)		\
	$(DEFAULT_APPLICATIONS_CAPPLET_CFLAGS)	\
	-DGLADEDIR=\""$(gladedir)"\"		\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""\
	-DDATADIR=\""$(datadir)"\"

noinst_DATA = gnome-default-applications.xml.in
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA) $(xmldata_DATA)
EXTRA_DIST = $(glade_DATA) gnome-default-applications.xml.in
