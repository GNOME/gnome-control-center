Applicationsdir = $(datadir)/control-center/capplets
Applications_in_files = background-properties.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = $(Applications_DATA)

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	background-properties.glade

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA)

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
