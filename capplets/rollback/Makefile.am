Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	rollback.desktop

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = \
	rollback.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	rollback.glade

glade_msgs = \
	rollback.glade.h

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA) $(glade_msgs)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"background-properties\"	\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	$(GNOME_INCLUDEDIR)				\
	$(CAPPLET_CFLAGS)				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl

bin_PROGRAMS = rollback-capplet

rollback_capplet_SOURCES = \
	config-manager-dialog.c	config-manager-dialog.h		\
	rollback-widget.c	rollback-widget.h		\
	main.c

rollback_capplet_LDADD = \
	$(GNOME_LIBDIR)	$(CAPPLET_LIBS)
