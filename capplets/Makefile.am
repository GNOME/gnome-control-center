always_built_SUBDIRS = desktop-links  \
	new-background-properties new-screensaver-properties \
        theme-switcher new-sound-properties new-bell-properties \
	new-ui-properties url-properties gnome-edit-properties \
	session-properties wm-properties new-keyboard-properties \
	mime-type new-mouse-properties

SUBDIRS = $(always_built_SUBDIRS)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"		\
	-DGLADE_DATADIR=\""$(datadir)/control-center-data"\"	\
	@CAPPLET_CFLAGS@					\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/intl

bin_PROGRAMS = sound-properties-capplet

sound_properties_capplet_SOURCES = sound-properties-capplet.c

sound_properties_capplet_LDADD = @CAPPLET_LIBS@

oafdir = $(datadir)/oaf
oaf_DATA = \
	Bonobo_Control_Capplet_sound_properties.oaf			

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	sound-properties.glade

glade_msgs = \
	sound-properties.glade.h
Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	sound-properties.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	sound-properties.desktop

EXTRA_DIST = ChangeLog $(Applications_DATA) $(Glade_DATA) $(glade_msgs) $(oaf_DATA)
