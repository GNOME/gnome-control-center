Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	mouse-properties.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	mouse-properties.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	mouse-properties.glade

glade_msgs = \
	mouse-properties.glade.h

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA) $(glade_msgs)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"mouse-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	$(GNOME_INCLUDEDIR)				\
	$(CAPPLET_CFLAGS)				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl

bin_PROGRAMS = mouse-properties-capplet

mouse_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c preferences.h \
	main.c

mouse_properties_capplet_LDADD = \
	$(GNOME_LIBDIR)	\
	$(CAPPLET_LIBS)
