Applicationsdir = $(datadir)/control-center/capplets/Desktop
Applications_DATA = \
	sound-properties.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	sound-properties.desktop

Gladedir = $(datadir)/control-center/interfaces
Glade_DATA = \
	sound-properties.glade

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = \
	sound-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = \
	Bonobo_Control_Capplet_sound_properties.oaf

glade_msgs = \
	sound-properties.glade.h

EXTRA_DIST = ChangeLog $(Applications_DATA) $(Glade_DATA) $(glade_msgs) $(oaf_DATA) $(defaults_DATA)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"sound-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	@CAPPLET_CFLAGS@				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl

bin_PROGRAMS = sound-properties-capplet

#sound_properties_capplet_SOURCES = \
#	prefs-widget.c prefs-widget.h \
#	preferences.c preferences.h \
#	main.c

sound_properties_capplet_SOURCES = \
	sound-properties-capplet.c

sound_properties_capplet_LDADD = @CAPPLET_LIBS@
