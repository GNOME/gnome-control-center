Applicationsdir = $(datadir)/control-center/capplets
Applications_in_files = keyboard-properties.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = $(Applications_DATA)

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	keyboard-properties.glade

EXTRA_DIST = ChangeLog $(Applications_DATA) $(Glade_DATA)

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
