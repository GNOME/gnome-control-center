Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	background-properties.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	background-properties.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	background-properties.glade

glade_msgs = \
	background-properties.glade.h

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA) $(glade_msgs)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"background-properties\"	\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	@BG_CAPPLET_CFLAGS@				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl

bin_PROGRAMS = background-properties-capplet

background_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h 			\
	preferences.c preferences.h 			\
	applier.c applier.h 				\
	main.c

background_properties_capplet_LDADD = \
	@BG_CAPPLET_LIBS@
