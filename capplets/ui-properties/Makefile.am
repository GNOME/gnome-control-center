Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = behavior.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = behavior.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	ui-properties.glade	\
	gnome-warning.png

glade_msgs = \
	ui-properties.glade.h

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA) $(glade_msgs)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"ui-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	$(CAPPLET_CFLAGS)				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				

bin_PROGRAMS = ui-properties-capplet

ui_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c preferences.h \
	main.c

ui_properties_capplet_LDADD = $(CAPPLET_LIBS)
