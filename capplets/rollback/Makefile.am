Applicationsdir = $(datadir)/control-center/capplets
Applications_in_files = rollback.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = $(Applications_DATA)

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	rollback.glade

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA)

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
	$(top_builddir)/archiver/libconfig_archiver.la		\
	@ROLLBACK_CAPPLET_LIBS@
