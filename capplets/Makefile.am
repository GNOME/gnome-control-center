always_built_SUBDIRS = desktop-links  \
	 new-background-properties new-screensaver-properties \
        theme-switcher new-sound-properties new-bell-properties \
	new-ui-properties url-properties gnome-edit-properties \
	session-properties wm-properties new-keyboard-properties \
	mime-type new-mouse-properties

SUBDIRS = $(always_built_SUBDIRS)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	@CAPPLET_CFLAGS@				\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/intl

bin_PROGRAMS = sound-properties-capplet

sound_properties_capplet_SOURCES = sound-properties-capplet.c

sound_properties_capplet_LDADD = @CAPPLET_LIBS@

OAF_FILES = 						\
	Bonobo_Control_Capplet_sound_properties.oaf			

oafdir = $(datadir)/oaf
oaf_DATA = $(OAF_FILES)

EXTRA_DIST = $(OAF_FILES)
