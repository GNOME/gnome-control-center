Applicationsdir = $(datadir)/control-center/capplets
Applications_in_files = sound-properties.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

pApplicationsdir = $(datadir)/gnome/apps/Settings/Desktop
pApplications_DATA = $(Applications_DATA)

Gladedir = $(datadir)/control-center/interfaces
Glade_DATA = \
	sound-properties.glade

defaultsdir= $(datadir)/control-center/defaults
defaults_DATA = \
	sound-properties.xml

oafdir = $(datadir)/oaf
oaf_DATA = \
	Bonobo_Control_Capplet_sound_properties.oaf

EXTRA_DIST = ChangeLog $(Applications_in_files) $(Glade_DATA) $(oaf_DATA) $(defaults_DATA)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DG_LOG_DOMAIN=\"sound-properties\"		\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)/capplets/common			\
	@CAPPLET_CFLAGS@

bin_PROGRAMS = sound-properties-capplet

sound_properties_capplet_SOURCES = \
	sound-properties-capplet.c

sound_properties_capplet_LDADD = \
	@CAPPLET_LIBS@					\
	$(top_builddir)/capplets/common/libcommon.a
