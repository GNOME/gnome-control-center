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
	@ROLLBACK_CAPPLET_CFLAGS@			\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)/archiver

bin_PROGRAMS = rollback-capplet

rollback_capplet_SOURCES = \
	config-manager-dialog.c	config-manager-dialog.h		\
	rollback-widget.c	rollback-widget.h		\
	rollback-control.c	rollback-control.h		\
	main.c

rollback_capplet_LDADD = \
	$(top_builddir)/archiver/libximian_archiver.la		\
	@ROLLBACK_CAPPLET_LIBS@
