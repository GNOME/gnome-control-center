Applicationsdir = $(datadir)/control-center/capplets
Applications_DATA = \
	keyboard-properties.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	keyboard-properties.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	keyboard-properties.glade

glade_msgs = \
	keyboard-properties.glade.h

EXTRA_DIST = ChangeLog $(Applications_DATA) $(Glade_DATA) $(glade_msgs)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"keyboard-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	@CAPPLET_CFLAGS@				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				

bin_PROGRAMS = keyboard-properties

keyboard_properties_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c preferences.h \
	main.c

keyboard_properties_LDADD = @CAPPLET_LIBS@ $(XF86MISC_LIBS)
