
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = \
	mouse-left.png	\
	mouse-right.png

Applicationsdir = $(datadir)/control-center/capplets
Applications_in_files = mouse-properties.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = $(Applications_DATA)

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	mouse-properties.glade

EXTRA_DIST = $(Applications_in_files) $(Glade_DATA) $(pixmap_DATA)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"mouse-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	@CAPPLET_CFLAGS@				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl

bin_PROGRAMS = mouse-properties-capplet

mouse_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c preferences.h \
	main.c

mouse_properties_capplet_LDADD = \
	@CAPPLET_LIBS@
